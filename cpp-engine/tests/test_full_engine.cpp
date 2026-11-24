#include <gtest/gtest.h>
#include "relayforge/engine.hpp"
#include <filesystem>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

using namespace relayforge;

class FullEngineTest : public ::testing::Test {
protected:
    std::string test_db;

    void SetUp() override {
        static int counter = 0;
        test_db = "/tmp/test_full_engine_" + std::to_string(++counter) + "_" + std::to_string(current_time_ms()) + ".db";
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

TEST_F(FullEngineTest, EndToEndConcurrentEventSubmissionAndProcessing) {
    EngineConfig config;
    config.db_path = test_db;
    config.worker_count = 4;
    config.max_queue_depth = 5000;
    config.rate_limit_eps = 10000;
    config.burst_size = 10000;

    RelayEngine engine(config);
    ASSERT_TRUE(engine.start());

    const int total_events = 100;
    std::vector<std::thread> producers;
    std::atomic<int> accepted{0};

    // 10 concurrent producer threads each submitting 10 events
    for (int t = 0; t < 10; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < 10; ++i) {
                Event e;
                e.event_type = "order.created";
                e.idempotency_key = "concurrent-ord-" + std::to_string(t) + "-" + std::to_string(i);
                e.priority = (i % 2 == 0) ? EventPriority::HIGH : EventPriority::NORMAL;
                e.payload = "{\"order_id\": \"ord_" + std::to_string(t * 10 + i) + "\", \"amount\": 25.50}";

                auto res = engine.ingest(e);
                if (res.status == IngestStatus::ACCEPTED) {
                    accepted.fetch_add(1);
                }
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }

    EXPECT_EQ(accepted.load(), total_events);

    // Wait for all 100 events to be processed by workers
    int waited = 0;
    while (engine.metrics().total_success() < static_cast<uint64_t>(total_events) && waited < 4000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        waited += 30;
    }

    EXPECT_EQ(engine.metrics().total_success(), static_cast<uint64_t>(total_events));
    EXPECT_GT(engine.metrics().get_p50_latency_ms(), 0.0);
    EXPECT_GT(engine.metrics().get_throughput_eps(), 0.0);

    engine.stop(true);
}

TEST_F(FullEngineTest, HandlesConcurrentDuplicateSubmissionWithIdempotency) {
    EngineConfig config;
    config.db_path = test_db;
    config.worker_count = 2;

    RelayEngine engine(config);
    ASSERT_TRUE(engine.start());

    // 10 threads all attempting to submit the EXACT SAME event with the SAME idempotency key
    const std::string shared_key = "idemp-race-key-42";
    std::atomic<int> accepted_count{0};
    std::atomic<int> duplicate_count{0};

    std::vector<std::thread> racers;
    for (int i = 0; i < 10; ++i) {
        racers.emplace_back([&]() {
            Event e;
            e.event_type = "payment.process";
            e.idempotency_key = shared_key;
            e.priority = EventPriority::CRITICAL;
            e.payload = "{\"payment_id\": \"race_pay_1\", \"amount\": 100.0, \"currency\": \"USD\"}";

            auto res = engine.ingest(e);
            if (res.status == IngestStatus::ACCEPTED) {
                accepted_count.fetch_add(1);
            } else if (res.status == IngestStatus::DUPLICATE) {
                duplicate_count.fetch_add(1);
            }
        });
    }

    for (auto& t : racers) {
        t.join();
    }

    // Exactly 1 must have been accepted, and 9 must be detected as duplicates
    EXPECT_EQ(accepted_count.load(), 1);
    EXPECT_EQ(duplicate_count.load(), 9);

    // Give time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(engine.metrics().total_success(), 1u);
    EXPECT_EQ(engine.storage().count_total_events(), 1u);

    engine.stop(true);
}

TEST_F(FullEngineTest, BackpressureRejectsWhenSchedulerCapacityIsExceeded) {
    EngineConfig config;
    config.db_path = test_db;
    config.worker_count = 1;
    config.max_queue_depth = 5; // Very small capacity to test backpressure

    RelayEngine engine(config);
    ASSERT_TRUE(engine.start());

    // Submit slow events that hold the worker
    Event slow_event;
    slow_event.event_type = "job.execute";
    slow_event.payload = "{\"job_name\": \"slow_job\", \"simulate_latency_ms\": 500}";
    engine.ingest(slow_event);

    int rejected_count = 0;
    int accepted_count = 0;

    for (int i = 0; i < 20; ++i) {
        Event e;
        e.event_type = "job.execute";
        e.payload = "{\"job_name\": \"fast_job\"}";
        auto res = engine.ingest(e);
        if (res.status == IngestStatus::ACCEPTED) {
            accepted_count++;
        } else if (res.status == IngestStatus::BACKPRESSURE_EXCEEDED) {
            rejected_count++;
        }
    }

    // Capacity was 5, so rejection must have occurred
    EXPECT_GT(rejected_count, 0);
    EXPECT_GT(engine.metrics().total_backpressure_drops(), 0u);

    engine.stop(false);
}

TEST_F(FullEngineTest, FailureInjectionAndRestartRecovery) {
    std::string event_id;

    // STEP 1: Ingest an event with high execution latency, terminate engine abruptly mid-execution
    {
        EngineConfig config;
        config.db_path = test_db;
        config.worker_count = 2;

        RelayEngine engine(config);
        ASSERT_TRUE(engine.start());

        Event e;
        e.id = generate_uuid_v4();
        event_id = e.id;
        e.event_type = "order.created";
        e.payload = "{\"order_id\": \"crash_ord_99\", \"amount\": 299.99, \"simulate_latency_ms\": 500}";
        e.max_retries = 3;

        auto res = engine.ingest(e);
        ASSERT_EQ(res.status, IngestStatus::ACCEPTED);

        // Wait a few milliseconds until worker picks it up and marks it IN_FLIGHT
        int waited = 0;
        while (waited < 1000) {
            auto current = engine.get_event(event_id);
            if (current.has_value() && current->status == EventStatus::IN_FLIGHT) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            waited += 10;
        }

        auto current = engine.get_event(event_id);
        ASSERT_TRUE(current.has_value());
        EXPECT_EQ(current->status, EventStatus::IN_FLIGHT);

        // ABANDON / TERMINATE abruptly! Do not drain!
        engine.stop(false);
    }

    // STEP 2: Boot new engine instance, verify crash recovery reconciles and completes the event!
    {
        EngineConfig config;
        config.db_path = test_db;
        config.worker_count = 2;

        RelayEngine engine(config);
        // Start engine: crash recovery automatically runs on boot
        ASSERT_TRUE(engine.start());

        // Wait for worker to finish processing the recovered event
        int waited = 0;
        while (waited < 3000) {
            auto current = engine.get_event(event_id);
            if (current.has_value() && current->status == EventStatus::COMPLETED) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            waited += 30;
        }

        auto final_event = engine.get_event(event_id);
        ASSERT_TRUE(final_event.has_value());
        EXPECT_EQ(final_event->status, EventStatus::COMPLETED);
        EXPECT_GT(final_event->execution_result.size(), 0u);

        // Check audit trail
        auto audit = engine.get_event_audit_trail(event_id);
        EXPECT_GE(audit.size(), 3u);

        engine.stop(true);
    }
}
