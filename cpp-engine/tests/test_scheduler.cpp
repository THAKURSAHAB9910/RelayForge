#include <gtest/gtest.h>
#include "relayforge/validator.hpp"
#include "relayforge/rate_limiter.hpp"
#include "relayforge/priority_scheduler.hpp"
#include <thread>
#include <chrono>

using namespace relayforge;

TEST(ValidatorTest, ValidatesBuiltInEventTypes) {
    EventValidator validator;

    Event order;
    order.id = generate_uuid_v4();
    order.event_type = "order.created";
    order.payload = "{\"order_id\": \"ord_123\", \"amount\": 99.50}";
    EXPECT_TRUE(validator.validate(order).valid);

    // Missing required field
    order.payload = "{\"amount\": 99.50}";
    EXPECT_FALSE(validator.validate(order).valid);

    // Negative amount
    order.payload = "{\"order_id\": \"ord_123\", \"amount\": -5.0}";
    EXPECT_FALSE(validator.validate(order).valid);

    // Malformed JSON
    order.payload = "{not valid json}";
    EXPECT_FALSE(validator.validate(order).valid);

    // Invalid event type
    Event invalid_type;
    invalid_type.id = generate_uuid_v4();
    invalid_type.event_type = "unknown.weird.event";
    invalid_type.payload = "{}";
    EXPECT_FALSE(validator.validate(invalid_type).valid);
}

TEST(RateLimiterTest, EnforcesRateLimitAndRefills) {
    // 5 tokens per second, burst capacity of 5
    TokenBucketRateLimiter limiter(5.0, 5.0);

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.try_acquire());
    }
    // 6th acquire should fail immediately
    EXPECT_FALSE(limiter.try_acquire());

    // Sleep for 250ms -> should gain ~1.25 tokens
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_FALSE(limiter.try_acquire());
}

TEST(SchedulerTest, PrioritizesCriticalOverLowPriority) {
    PriorityScheduler scheduler(100);
    scheduler.start();

    Event low;
    low.id = "low-1";
    low.priority = EventPriority::LOW;
    low.created_at = 100;
    scheduler.push(low);

    Event normal;
    normal.id = "normal-1";
    normal.priority = EventPriority::NORMAL;
    normal.created_at = 101;
    scheduler.push(normal);

    Event critical;
    critical.id = "critical-1";
    critical.priority = EventPriority::CRITICAL;
    critical.created_at = 102;
    scheduler.push(critical);

    Event high;
    high.id = "high-1";
    high.priority = EventPriority::HIGH;
    high.created_at = 103;
    scheduler.push(high);

    // Should pop in order: Critical, High, Normal, Low
    auto p1 = scheduler.pop(std::chrono::milliseconds(50));
    ASSERT_TRUE(p1.has_value());
    EXPECT_EQ(p1->id, "critical-1");

    auto p2 = scheduler.pop(std::chrono::milliseconds(50));
    ASSERT_TRUE(p2.has_value());
    EXPECT_EQ(p2->id, "high-1");

    auto p3 = scheduler.pop(std::chrono::milliseconds(50));
    ASSERT_TRUE(p3.has_value());
    EXPECT_EQ(p3->id, "normal-1");

    auto p4 = scheduler.pop(std::chrono::milliseconds(50));
    ASSERT_TRUE(p4.has_value());
    EXPECT_EQ(p4->id, "low-1");

    scheduler.shutdown();
}

TEST(SchedulerTest, EnforcesBackpressureWhenCapacityReached) {
    PriorityScheduler scheduler(3); // Capacity = 3

    Event e;
    e.id = "e1";
    EXPECT_TRUE(scheduler.push(e));
    e.id = "e2";
    EXPECT_TRUE(scheduler.push(e));
    e.id = "e3";
    EXPECT_TRUE(scheduler.push(e));

    // 4th push exceeds capacity -> Backpressure rejection!
    e.id = "e4";
    EXPECT_FALSE(scheduler.push(e));
    EXPECT_EQ(scheduler.total_depth(), 3u);

    // After popping one, we can push again
    auto popped = scheduler.try_pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_TRUE(scheduler.push(e));
}

TEST(SchedulerTest, HandlesDelayedRetriesViaTimerThread) {
    PriorityScheduler scheduler(100);
    scheduler.start();

    int64_t now = current_time_ms();

    Event delayed;
    delayed.id = "delayed-event";
    delayed.priority = EventPriority::CRITICAL;
    delayed.scheduled_at = now + 150; // 150ms in future
    scheduler.push(delayed);

    // Immediately, active queue should be empty
    EXPECT_EQ(scheduler.active_depth(), 0u);
    EXPECT_EQ(scheduler.delayed_depth(), 1u);

    // Popping immediately with short timeout should return nullopt
    auto immediate = scheduler.pop(std::chrono::milliseconds(20));
    EXPECT_FALSE(immediate.has_value());

    // Wait until scheduled time arrives (200ms)
    auto future_event = scheduler.pop(std::chrono::milliseconds(300));
    ASSERT_TRUE(future_event.has_value());
    EXPECT_EQ(future_event->id, "delayed-event");

    scheduler.shutdown();
}
