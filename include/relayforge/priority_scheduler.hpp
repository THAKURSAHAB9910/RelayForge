#pragma once

#include "relayforge/types.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>
#include <atomic>
#include <thread>

namespace relayforge {

struct DelayedEventComparator {
    bool operator()(const Event& a, const Event& b) const {
        return a.scheduled_at > b.scheduled_at; // Min-heap: earliest scheduled_at on top
    }
};

struct ActiveEventComparator {
    bool operator()(const Event& a, const Event& b) const {
        if (a.priority != b.priority) {
            return static_cast<int>(a.priority) > static_cast<int>(b.priority); // lower int = higher priority
        }
        return a.created_at > b.created_at; // FIFO for same priority
    }
};

class PriorityScheduler {
public:
    explicit PriorityScheduler(size_t max_queue_depth = 50000);
    ~PriorityScheduler();

    // Starts the delayed-retry timer worker thread
    void start();

    // Gracefully signals shutdown and wakes up all waiters
    void shutdown();

    // Pushes event into scheduler. If total depth >= max_queue_depth, returns false (Backpressure triggered)
    bool push(const Event& event);

    // Blocking pop with timeout. Returns std::nullopt on timeout or if scheduler is stopped and empty.
    std::optional<Event> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100));

    // Non-blocking try_pop
    std::optional<Event> try_pop();

    size_t active_depth() const;
    size_t delayed_depth() const;
    size_t total_depth() const;
    size_t max_depth() const { return max_queue_depth_; }

    bool is_stopped() const { return stopped_.load(); }
    void clear();

private:
    size_t max_queue_depth_;
    std::atomic<bool> stopped_{false};
    std::atomic<bool> started_{false};

    mutable std::mutex mutex_;
    std::condition_variable cv_active_;  // Signals workers when active queue has events
    std::condition_variable cv_delayed_; // Signals timer thread when delayed queue changes

    std::priority_queue<Event, std::vector<Event>, ActiveEventComparator> active_queue_;
    std::priority_queue<Event, std::vector<Event>, DelayedEventComparator> delayed_queue_;

    std::thread timer_thread_;
    void timer_loop();
};

} // namespace relayforge
