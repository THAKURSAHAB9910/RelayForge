#include "relayforge/retry_manager.hpp"
#include <cmath>
#include <algorithm>

namespace relayforge {

RetryManager::RetryManager(RetryPolicy default_policy)
    : default_policy_(default_policy) {
    std::random_device rd;
    rng_.seed(rd());
}

int64_t RetryManager::calculate_backoff_ms(int attempt_number, const RetryPolicy& policy) {
    if (attempt_number <= 0) attempt_number = 1;

    // Exponential base: initial * factor^(attempt - 1)
    double exp_backoff = static_cast<double>(policy.initial_interval_ms) *
                         std::pow(policy.backoff_factor, attempt_number - 1);

    double capped = std::min(exp_backoff, static_cast<double>(policy.max_interval_ms));

    if (!policy.enable_jitter) {
        return static_cast<int64_t>(capped);
    }

    // Full Jitter: uniform random between 0 and capped
    std::lock_guard<std::mutex> lock(rng_mutex_);
    std::uniform_real_distribution<double> dist(0.0, capped);
    return static_cast<int64_t>(dist(rng_));
}

RetryEvaluation RetryManager::evaluate_failure(
    const Event& event,
    const std::string& error_message,
    bool is_fatal
) {
    int current_attempt = event.attempt_count;
    int max_retries = (event.max_retries >= 0) ? event.max_retries : default_policy_.max_retries;

    if (is_fatal || current_attempt >= max_retries) {
        return {
            RetryDecision::DEAD_LETTER,
            current_attempt,
            0,
            0,
            error_message
        };
    }

    int64_t delay_ms = calculate_backoff_ms(current_attempt, default_policy_);
    int64_t now = current_time_ms();

    return {
        RetryDecision::RETRY,
        current_attempt,
        now + delay_ms,
        delay_ms,
        error_message
    };
}

} // namespace relayforge
