#pragma once

#include "relayforge/types.hpp"
#include "relayforge/storage_engine.hpp"
#include "relayforge/priority_scheduler.hpp"
#include "relayforge/metrics_registry.hpp"
#include <chrono>

namespace relayforge {

struct RecoveryReport {
    size_t in_flight_recovered = 0;
    size_t pending_recovered = 0;
    size_t retryable_recovered = 0;
    size_t dlq_due_to_crash_limit = 0;
    size_t total_recovered = 0;
    double duration_ms = 0.0;

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"in_flight_recovered", in_flight_recovered},
            {"pending_recovered", pending_recovered},
            {"retryable_recovered", retryable_recovered},
            {"dlq_due_to_crash_limit", dlq_due_to_crash_limit},
            {"total_recovered", total_recovered},
            {"duration_ms", duration_ms}
        };
    }
};

class RecoveryManager {
public:
    RecoveryManager(
        StorageEngine& storage,
        PriorityScheduler& scheduler,
        MetricsRegistry& metrics
    );

    RecoveryReport recover_unsettled_events();

private:
    StorageEngine& storage_;
    PriorityScheduler& scheduler_;
    MetricsRegistry& metrics_;
};

} // namespace relayforge
