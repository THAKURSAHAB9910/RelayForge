#include <gtest/gtest.h>
#include "relayforge/sqlite_storage.hpp"
#include <filesystem>

using namespace relayforge;

class SqliteStorageTest : public ::testing::Test {
protected:
    std::string test_db;

    void SetUp() override {
        static int counter = 0;
        test_db = "/tmp/test_storage_" + std::to_string(++counter) + "_" + std::to_string(current_time_ms()) + ".db";
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

TEST_F(SqliteStorageTest, InitializesWalModeAndTables) {
    {
        SqliteStorage storage(test_db, true, true);
        ASSERT_TRUE(storage.initialize());
        EXPECT_EQ(storage.count_total_events(), 0);
        storage.close();
    }
}

TEST_F(SqliteStorageTest, InsertsAndRetrievesEvent) {
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        Event e;
        e.id = generate_uuid_v4();
        e.idempotency_key = "order-xyz-100";
        e.event_type = "order.created";
        e.priority = EventPriority::HIGH;
        e.payload = "{\"order_id\": 100, \"amount\": 49.99}";
        e.status = EventStatus::PENDING;

        auto [inserted, result] = storage.insert_event(e);
        EXPECT_TRUE(inserted);
        EXPECT_EQ(result.id, e.id);
        EXPECT_EQ(result.idempotency_key, e.idempotency_key);
        EXPECT_EQ(storage.count_total_events(), 1);

        auto fetched = storage.get_event(e.id);
        ASSERT_TRUE(fetched.has_value());
        EXPECT_EQ(fetched->event_type, "order.created");
        EXPECT_EQ(fetched->priority, EventPriority::HIGH);
        EXPECT_EQ(fetched->status, EventStatus::PENDING);

        auto fetched_by_key = storage.get_event_by_idempotency_key("order-xyz-100");
        ASSERT_TRUE(fetched_by_key.has_value());
        EXPECT_EQ(fetched_by_key->id, e.id);
        storage.close();
    }
}

TEST_F(SqliteStorageTest, EnforcesIdempotencyDeduplication) {
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        Event e1;
        e1.id = generate_uuid_v4();
        e1.idempotency_key = "idemp-key-dup-test";
        e1.event_type = "payment.process";
        e1.priority = EventPriority::CRITICAL;
        e1.payload = "{\"amount\": 100}";

        auto [inserted1, res1] = storage.insert_event(e1);
        EXPECT_TRUE(inserted1);
        EXPECT_EQ(res1.id, e1.id);

        // Second event with duplicate idempotency key
        Event e2;
        e2.id = generate_uuid_v4();
        e2.idempotency_key = "idemp-key-dup-test";
        e2.event_type = "payment.process";
        e2.priority = EventPriority::LOW;
        e2.payload = "{\"amount\": 200}";

        auto [inserted2, res2] = storage.insert_event(e2);
        EXPECT_FALSE(inserted2); // Deduplicated!
        EXPECT_EQ(res2.id, e1.id); // Returns the original event
        EXPECT_EQ(storage.count_total_events(), 1); // Only 1 event exists
        storage.close();
    }
}

TEST_F(SqliteStorageTest, UpdatesStatusAndRecordsAuditTrail) {
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        Event e;
        e.id = generate_uuid_v4();
        e.event_type = "notification.send";
        e.payload = "{\"user\": \"alice\"}";

        auto [inserted, res] = storage.insert_event(e);
        ASSERT_TRUE(inserted);

        // Transition to IN_FLIGHT
        EXPECT_TRUE(storage.update_event_status(e.id, EventStatus::IN_FLIGHT, 1, "", "", 0, 5.2));
        auto current = storage.get_event(e.id);
        ASSERT_TRUE(current.has_value());
        EXPECT_EQ(current->status, EventStatus::IN_FLIGHT);
        EXPECT_EQ(current->attempt_count, 1);

        // Transition to COMPLETED
        EXPECT_TRUE(storage.update_event_status(e.id, EventStatus::COMPLETED, 1, "", "{\"sent\": true}", 0, 12.4));
        current = storage.get_event(e.id);
        ASSERT_TRUE(current.has_value());
        EXPECT_EQ(current->status, EventStatus::COMPLETED);
        EXPECT_EQ(current->execution_result, "{\"sent\": true}");

        // Check audit logs
        auto logs = storage.get_audit_log(e.id);
        EXPECT_GE(logs.size(), 3u); // Ingested, In_flight, Completed
        storage.close();
    }
}

TEST_F(SqliteStorageTest, RecoversUnsettledEventsAndHandlesDLQReplay) {
    {
        SqliteStorage storage(test_db);
        ASSERT_TRUE(storage.initialize());

        Event e1;
        e1.id = generate_uuid_v4();
        e1.event_type = "job.step1";
        e1.payload = "{}";
        storage.insert_event(e1);
        storage.update_event_status(e1.id, EventStatus::IN_FLIGHT, 1);

        Event e2;
        e2.id = generate_uuid_v4();
        e2.event_type = "job.step2";
        e2.payload = "{}";
        storage.insert_event(e2);
        storage.update_event_status(e2.id, EventStatus::FAILED_RETRYABLE, 2, "Timeout", "", current_time_ms() + 5000);

        Event e3;
        e3.id = generate_uuid_v4();
        e3.event_type = "job.step3";
        e3.payload = "{}";
        storage.insert_event(e3);
        storage.update_event_status(e3.id, EventStatus::DEAD_LETTER, 3, "Fatal exception");

        auto unsettled = storage.get_unsettled_events();
        EXPECT_EQ(unsettled.size(), 2u); // e1 (IN_FLIGHT) and e2 (FAILED_RETRYABLE)

        auto dlq = storage.get_dead_letter_events();
        ASSERT_EQ(dlq.size(), 1u);
        EXPECT_EQ(dlq[0].id, e3.id);

        // Replay DLQ event
        EXPECT_TRUE(storage.reset_event_for_replay(e3.id));
        auto replayed = storage.get_event(e3.id);
        ASSERT_TRUE(replayed.has_value());
        EXPECT_EQ(replayed->status, EventStatus::PENDING);
        EXPECT_EQ(replayed->attempt_count, 0);

        EXPECT_EQ(storage.get_dead_letter_events().size(), 0u);
        storage.close();
    }
}
