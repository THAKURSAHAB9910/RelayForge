#include <gtest/gtest.h>
#include "relayforge/sqlite_storage.hpp"
#include "relayforge/priority_scheduler.hpp"
#include "relayforge/event_handlers.hpp"
#include "relayforge/retry_manager.hpp"
#include "relayforge/worker_pool.hpp"
#include <filesystem>
#include <atomic>
#include <thread>
#include <chrono>

using namespace relayforge;

class WorkerPoolTest : public ::testing::Test {
protected:
    std::string test_db;

    void SetUp() override {
        static int counter = 0;
        test_db = "/tmp/test_worker_pool_" + std::to_string(++counter) + "_" + std::to_string(current_time_ms()) + ".db";
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

TEST_F(WorkerPoolTest, SuccessfullyProcessesConcurrentEvents) {
    SqliteStorage storage(test_db);
    ASSERT_TRUE(storage.initialize());

    PriorityScheduler scheduler(1000);
    scheduler.start();

    HandlerRegistry handlers;
    RetryPolicy policy;
    RetryManager retry_manager(policy);

    WorkerPool pool(4, scheduler, storage, handlers, retry_manager);

    std::atomic<int> success_count{0};
    pool.set_on_success([&](const Event&, double) {
        success_count.fetch_add(1);
    });

    pool.start();

    // Push 20 order events
    for (int i = 0; i < 20; ++i) {
        Event e;
        e.id = generate_uuid_v4();
        e.event_type = "order.created";
        e.payload = "{\"order_id\": " + std::to_string(i) + ", \"amount\": 19.99}";
        storage.insert_event(e);
        scheduler.push(e);
    }

    // Wait for all 20 to complete
    int waited = 0;
    while (success_count.load() < 20 && waited < 3000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        waited += 20;
    }

    EXPECT_EQ(success_count.load(), 20);
    pool.stop(true);
    storage.close();
}

TEST_F(WorkerPoolTest, RetriesFailedEventWithExponentialBackoff) {
    SqliteStorage storage(test_db);
    ASSERT_TRUE(storage.initialize());

    PriorityScheduler scheduler(1000);
    scheduler.start();

    HandlerRegistry handlers;
    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_interval_ms = 50;
    policy.backoff_factor = 2.0;
    policy.enable_jitter = false; // Deterministic backoff for test
    RetryManager retry_manager(policy);

    WorkerPool pool(2, scheduler, storage, handlers, retry_manager);

    std::atomic<int> retry_count{0};
    std::atomic<int> success_count{0};

    pool.set_on_retry([&](const Event&, const std::string&, double) {
        retry_count.fetch_add(1);
    });

    pool.set_on_success([&](const Event&, double) {
        success_count.fetch_add(1);
    });

    pool.start();

    // Event designed to fail twice, then succeed on attempt 3
    Event e;
    e.id = generate_uuid_v4();
    e.event_type = "payment.process";
    e.payload = "{\"payment_id\": \"pay_retry_1\", \"amount\": 50.0, \"currency\": \"USD\", \"fail_until_attempt\": 2}";
    e.max_retries = 3;

    storage.insert_event(e);
    scheduler.push(e);

    int waited = 0;
    while (success_count.load() < 1 && waited < 4000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        waited += 30;
    }

    EXPECT_EQ(retry_count.load(), 2);
    EXPECT_EQ(success_count.load(), 1);

    auto final_event = storage.get_event(e.id);
    ASSERT_TRUE(final_event.has_value());
    EXPECT_EQ(final_event->status, EventStatus::COMPLETED);
    EXPECT_EQ(final_event->attempt_count, 3);

    pool.stop(true);
    storage.close();
}

TEST_F(WorkerPoolTest, MovesExceededRetriesToDeadLetterQueueAndSupportsReplay) {
    SqliteStorage storage(test_db);
    ASSERT_TRUE(storage.initialize());

    PriorityScheduler scheduler(1000);
    scheduler.start();

    HandlerRegistry handlers;
    RetryPolicy policy;
    policy.max_retries = 2; // Only 2 retries allowed
    policy.initial_interval_ms = 20;
    policy.enable_jitter = false;
    RetryManager retry_manager(policy);

    WorkerPool pool(2, scheduler, storage, handlers, retry_manager);

    std::atomic<int> dlq_count{0};
    pool.set_on_dead_letter([&](const Event&, const std::string&, double) {
        dlq_count.fetch_add(1);
    });

    pool.start();

    // Event that always throws runtime_error
    Event e;
    e.id = generate_uuid_v4();
    e.event_type = "notification.send";
    e.payload = "{\"recipient\": \"bob@example.com\", \"message\": \"hi\", \"simulate_error\": \"SMTP Server Down\"}";
    e.max_retries = 2;

    storage.insert_event(e);
    scheduler.push(e);

    int waited = 0;
    while (dlq_count.load() < 1 && waited < 4000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        waited += 30;
    }

    EXPECT_EQ(dlq_count.load(), 1);

    auto dlq_events = storage.get_dead_letter_events();
    ASSERT_EQ(dlq_events.size(), 1u);
    EXPECT_EQ(dlq_events[0].id, e.id);
    EXPECT_EQ(dlq_events[0].status, EventStatus::DEAD_LETTER);
    EXPECT_EQ(dlq_events[0].last_error, "SMTP Server Down");

    // Test Replay: Fix the payload in DB, replay DLQ event
    EXPECT_TRUE(storage.reset_event_for_replay(e.id));
    auto replayed = storage.get_event(e.id);
    ASSERT_TRUE(replayed.has_value());
    EXPECT_EQ(replayed->status, EventStatus::PENDING);
    EXPECT_EQ(replayed->attempt_count, 0);

    // Update payload so it doesn't fail on replay
    storage.update_event_status(e.id, EventStatus::PENDING, 0, "", "");
    replayed->payload = "{\"recipient\": \"bob@example.com\", \"message\": \"hi\"}";

    std::atomic<int> replay_success{0};
    pool.set_on_success([&](const Event&, double) {
        replay_success.fetch_add(1);
    });

    scheduler.push(*replayed);

    waited = 0;
    while (replay_success.load() < 1 && waited < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        waited += 30;
    }

    EXPECT_EQ(replay_success.load(), 1);

    auto final_ev = storage.get_event(e.id);
    ASSERT_TRUE(final_ev.has_value());
    EXPECT_EQ(final_ev->status, EventStatus::COMPLETED);

    pool.stop(true);
    storage.close();
}

TEST_F(WorkerPoolTest, FatalExceptionsBypassRetryDirectlyToDLQ) {
    SqliteStorage storage(test_db);
    ASSERT_TRUE(storage.initialize());

    PriorityScheduler scheduler(100);
    scheduler.start();

    HandlerRegistry handlers;
    RetryPolicy policy;
    policy.max_retries = 5;
    RetryManager retry_manager(policy);

    WorkerPool pool(2, scheduler, storage, handlers, retry_manager);

    std::atomic<int> dlq_count{0};
    pool.set_on_dead_letter([&](const Event&, const std::string&, double) {
        dlq_count.fetch_add(1);
    });

    pool.start();

    Event e;
    e.id = generate_uuid_v4();
    e.event_type = "order.created";
    e.payload = "{\"order_id\": \"fatal_1\", \"amount\": 10.0, \"simulate_fatal\": \"Corrupted account ledger\"}";
    e.max_retries = 5;

    storage.insert_event(e);
    scheduler.push(e);

    int waited = 0;
    while (dlq_count.load() < 1 && waited < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        waited += 20;
    }

    EXPECT_EQ(dlq_count.load(), 1);

    auto stored = storage.get_event(e.id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->status, EventStatus::DEAD_LETTER);
    EXPECT_EQ(stored->attempt_count, 1); // Directly sent to DLQ without wasting 5 retries!

    pool.stop(true);
    storage.close();
}
