#pragma once

#include <mutex>
#include <chrono>
#include <condition_variable>

namespace relayforge {

class TokenBucketRateLimiter {
public:
    TokenBucketRateLimiter(double tokens_per_second, double capacity);

    bool try_acquire(size_t tokens = 1);
    bool acquire_with_timeout(size_t tokens, std::chrono::milliseconds timeout);

    void set_rate(double tokens_per_second, double capacity);
    double available_tokens();
    void reset();

private:
    double tokens_per_second_;
    double capacity_;
    double current_tokens_;
    std::chrono::steady_clock::time_point last_refill_time_;
    std::mutex mutex_;
    std::condition_variable cv_;

    void refill_unlocked();
};

} // namespace relayforge
