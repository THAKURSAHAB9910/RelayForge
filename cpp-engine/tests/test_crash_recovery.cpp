#include <gtest/gtest.h>
#include "relayforge/sqlite_storage.hpp"
#include "relayforge/priority_scheduler.hpp"
#include "relayforge/event_handlers.hpp"
#include "relayforge/retry_manager.hpp"
#include "relayforge/worker_pool.hpp"
#include "relayforge/metrics_registry.hpp"
#include "relayforge/recovery_manager.hpp"
#include <filesystem>
#include <atomic>
#include <thread>
#include <chrono>

using namespace relayforge;

class CrashRecoveryTest : public ::testing::Test {
protected:
    std::string test_db;

    void SetUp() override {
        static int counter = 0;
        test_db = "/tmp/test_crash_recovery_" + std::to_string(++counter) + "_" + std::to_string(current_time_ms()) + ".db";
        cleanup();
    }

    void TearDown() override {
        cleanup();
    }

    void cleanup() {
        std::error_code ec;
        std::filesystem::remove(test_db, ec);
        std::filesystem::remove(test_db + "-wal", ec);
        std::filesystem::remove(test_db + "-shm", ec);
    }
};

TEST_F(CrashRecoveryTest, RecoversInFlightAndPendingEventsAfterAbruptTermination) {
    std::string id_inflight, id_pending, id_retryable;

    // PHASE 1: Simulate engine running, persisting events, and abruptly crashing
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        // Event 1: Was in flight when crash happened
        Event e1;
        e1.id = generate_uuid_v4();
        id_inflight = e1.id;
        e1.event_type = "order.created";
        e1.payload = "{\"order_id\": \"crash_ord_1\", \"amount\": 150.0}";
        e1.max_retries = 3;
        storage.insert_event(e1);
        storage.update_event_status(e1.id, EventStatus::IN_FLIGHT, 1);

        // Event 2: Was pending in queue
        Event e2;
        e2.id = generate_uuid_v4();
        id_pending = e2.id;
        e2.event_type = "notification.send";
        e2.payload = "{\"recipient\": \"alex@example.com\", \"message\": \"Your order has shipped\"}";
        e2.max_retries = 3;
        storage.insert_event(e2);

        // Event 3: Was failed_retryable with future scheduled time
        Event e3;
        e3.id = generate_uuid_v4();
        id_retryable = e3.id;
        e3.event_type = "inventory.reserve";
        e3.payload = "{\"item_id\": \"SKU-99\", \"quantity\": 2}";
        e3.max_retries = 3;
        storage.insert_event(e3);
        storage.update_event_status(e3.id, EventStatus::FAILED_RETRYABLE, 1, "Temporary db timeout", "", current_time_ms() + 50);

        // Crash! Simulate abrupt shutdown without processing
        storage.close();
    }

    // PHASE 2: New engine instance starts up and performs crash recovery
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        PriorityScheduler scheduler(100);
        scheduler.start();

        MetricsRegistry metrics;
        RecoveryManager recovery(storage, scheduler, metrics);

        auto report = recovery.recover_unsettled_events();
        EXPECT_EQ(report.in_flight_recovered, 1u);
        EXPECT_EQ(report.pending_recovered, 1u);
        EXPECT_EQ(report.retryable_recovered, 1u);
        EXPECT_EQ(report.total_recovered, 3u);
        EXPECT_GT(report.duration_ms, 0.0);

        // Now start workers to process the recovered events to completion
        HandlerRegistry handlers;
        RetryPolicy policy;
        RetryManager retry_manager(policy);
        WorkerPool pool(2, scheduler, storage, handlers, retry_manager);

        std::atomic<int> completed_count{0};
        pool.set_on_success([&](const Event&, double) {
            completed_count.fetch_add(1);
        });

        pool.start();

        int waited = 0;
        while (completed_count.load() < 3 && waited < 3000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            waited += 30;
        }

        EXPECT_EQ(completed_count.load(), 3);

        auto check_inflight = storage.get_event(id_inflight);
        ASSERT_TRUE(check_inflight.has_value());
        EXPECT_EQ(check_inflight->status, EventStatus::COMPLETED);
        // Notice: attempt_count became 2 because crash recovery marked attempt 2
        EXPECT_EQ(check_inflight->attempt_count, 2);

        auto check_pending = storage.get_event(id_pending);
        ASSERT_TRUE(check_pending.has_value());
        EXPECT_EQ(check_pending->status, EventStatus::COMPLETED);

        auto check_retryable = storage.get_event(id_retryable);
        ASSERT_TRUE(check_retryable.has_value());
        EXPECT_EQ(check_retryable->status, EventStatus::COMPLETED);

        pool.stop(true);
        storage.close();
    }
}

TEST_F(CrashRecoveryTest, SendsRepeatedCrashPoisonPillToDLQ) {
    std::string poison_id;

    // Simulate an event that already crashed 2 times, and max_retries is 2
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        Event e;
        e.id = generate_uuid_v4();
        poison_id = e.id;
        e.event_type = "job.execute";
        e.payload = "{\"job_name\": \"poison_pill\"}";
        e.max_retries = 2;
        storage.insert_event(e);
        // Already attempted twice and crashed while IN_FLIGHT
        storage.update_event_status(e.id, EventStatus::IN_FLIGHT, 2);

        storage.close();
    }

    // Engine restarts
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        PriorityScheduler scheduler(100);
        MetricsRegistry metrics;
        RecoveryManager recovery(storage, scheduler, metrics);

        auto report = recovery.recover_unsettled_events();
        // New attempt would be 3 > 2 (max_retries) -> Sent to DLQ!
        EXPECT_EQ(report.dlq_due_to_crash_limit, 1u);
        EXPECT_EQ(report.in_flight_recovered, 0u);

        auto dlq_event = storage.get_event(poison_id);
        ASSERT_TRUE(dlq_event.has_value());
        EXPECT_EQ(dlq_event->status, EventStatus::DEAD_LETTER);
        EXPECT_EQ(dlq_event->attempt_count, 2);

        storage.close();
    }
}
