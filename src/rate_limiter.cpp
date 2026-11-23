#include "relayforge/rate_limiter.hpp"
#include <algorithm>

namespace relayforge {

TokenBucketRateLimiter::TokenBucketRateLimiter(double tokens_per_second, double capacity)
    : tokens_per_second_(tokens_per_second),
      capacity_(capacity),
      current_tokens_(capacity),
      last_refill_time_(std::chrono::steady_clock::now()) {}

void TokenBucketRateLimiter::refill_unlocked() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - last_refill_time_;
    last_refill_time_ = now;

    current_tokens_ = std::min(capacity_, current_tokens_ + elapsed.count() * tokens_per_second_);
}

bool TokenBucketRateLimiter::try_acquire(size_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_unlocked();

    if (current_tokens_ >= static_cast<double>(tokens)) {
        current_tokens_ -= static_cast<double>(tokens);
        return true;
    }
    return false;
}

bool TokenBucketRateLimiter::acquire_with_timeout(size_t tokens, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        refill_unlocked();
        if (current_tokens_ >= static_cast<double>(tokens)) {
            current_tokens_ -= static_cast<double>(tokens);
            return true;
        }

        double needed = static_cast<double>(tokens) - current_tokens_;
        double wait_seconds = needed / tokens_per_second_;
        auto wait_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(wait_seconds)
        );

        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return false;
        }

        auto time_left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        auto sleep_time = std::min(wait_duration + std::chrono::milliseconds(1), time_left);

        if (cv_.wait_for(lock, sleep_time) == std::cv_status::timeout) {
            // Check condition in next loop
        }
    }
}

void TokenBucketRateLimiter::set_rate(double tokens_per_second, double capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_unlocked();
    tokens_per_second_ = tokens_per_second;
    capacity_ = capacity;
    current_tokens_ = std::min(capacity_, current_tokens_);
    cv_.notify_all();
}

double TokenBucketRateLimiter::available_tokens() {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_unlocked();
    return current_tokens_;
}

void TokenBucketRateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_tokens_ = capacity_;
    last_refill_time_ = std::chrono::steady_clock::now();
    cv_.notify_all();
}

} // namespace relayforge
