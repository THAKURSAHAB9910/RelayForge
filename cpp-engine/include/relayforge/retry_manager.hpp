#pragma once

#include "relayforge/types.hpp"
#include "relayforge/storage_engine.hpp"
#include <random>
#include <mutex>

namespace relayforge {

enum class RetryDecision {
    RETRY,
    DEAD_LETTER
};

struct RetryEvaluation {
    RetryDecision decision;
    int next_attempt_count;
    int64_t next_scheduled_at;
    int64_t delay_ms;
    std::string failure_reason;
};

class RetryManager {
public:
    explicit RetryManager(RetryPolicy default_policy = RetryPolicy{});

    RetryEvaluation evaluate_failure(
        const Event& event,
        const std::string& error_message,
        bool is_fatal = false
    );

    int64_t calculate_backoff_ms(int attempt_number, const RetryPolicy& policy);

    const RetryPolicy& default_policy() const { return default_policy_; }
    void set_default_policy(RetryPolicy policy) { default_policy_ = policy; }

private:
    RetryPolicy default_policy_;
    std::mt19937_64 rng_;
    std::mutex rng_mutex_;
};

} // namespace relayforge
