#include "relayforge/priority_scheduler.hpp"
#include <iostream>

namespace relayforge {

PriorityScheduler::PriorityScheduler(size_t max_queue_depth)
    : max_queue_depth_(max_queue_depth) {}

PriorityScheduler::~PriorityScheduler() {
    shutdown();
}

void PriorityScheduler::start() {
    if (started_.exchange(true)) {
        return;
    }
    stopped_.store(false);
    timer_thread_ = std::thread(&PriorityScheduler::timer_loop, this);
}

void PriorityScheduler::shutdown() {
    if (stopped_.exchange(true)) {
        return;
    }

    cv_active_.notify_all();
    cv_delayed_.notify_all();

    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

bool PriorityScheduler::push(const Event& event) {
    if (stopped_.load()) {
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    // Check backpressure threshold
    size_t total = active_queue_.size() + delayed_queue_.size();
    if (total >= max_queue_depth_) {
        return false; // Backpressure: capacity exceeded
    }

    int64_t now = current_time_ms();
    if (event.scheduled_at > now) {
        delayed_queue_.push(event);
        cv_delayed_.notify_one();
    } else {
        active_queue_.push(event);
        cv_active_.notify_one();
    }

    return true;
}

std::optional<Event> PriorityScheduler::pop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!cv_active_.wait_for(lock, timeout, [this]() {
        return stopped_.load() || !active_queue_.empty();
    })) {
        return std::nullopt; // Timeout
    }

    if (active_queue_.empty()) {
        return std::nullopt; // Stopped and queue is empty
    }

    Event event = active_queue_.top();
    active_queue_.pop();
    return event;
}

std::optional<Event> PriorityScheduler::try_pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (active_queue_.empty()) {
        return std::nullopt;
    }
    Event event = active_queue_.top();
    active_queue_.pop();
    return event;
}

size_t PriorityScheduler::active_depth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_queue_.size();
}

size_t PriorityScheduler::delayed_depth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return delayed_queue_.size();
}

size_t PriorityScheduler::total_depth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_queue_.size() + delayed_queue_.size();
}

void PriorityScheduler::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!active_queue_.empty()) active_queue_.pop();
    while (!delayed_queue_.empty()) delayed_queue_.pop();
    cv_delayed_.notify_all();
    cv_active_.notify_all();
}

void PriorityScheduler::timer_loop() {
    while (!stopped_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (delayed_queue_.empty()) {
            cv_delayed_.wait(lock, [this]() {
                return stopped_.load() || !delayed_queue_.empty();
            });
            if (stopped_.load()) break;
        }

        if (delayed_queue_.empty()) continue;

        int64_t next_scheduled = delayed_queue_.top().scheduled_at;
        int64_t now = current_time_ms();

        if (next_scheduled <= now) {
            Event event = delayed_queue_.top();
            delayed_queue_.pop();
            active_queue_.push(event);
            cv_active_.notify_one();
        } else {
            int64_t wait_ms = next_scheduled - now;
            cv_delayed_.wait_for(lock, std::chrono::milliseconds(wait_ms), [this, next_scheduled]() {
                if (stopped_.load()) return true;
                if (delayed_queue_.empty()) return true;
                return delayed_queue_.top().scheduled_at < next_scheduled;
            });
        }
    }
}

} // namespace relayforge
