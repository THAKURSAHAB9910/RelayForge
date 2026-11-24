#include "relayforge/recovery_manager.hpp"
#include <iostream>

namespace relayforge {

RecoveryManager::RecoveryManager(
    StorageEngine& storage,
    PriorityScheduler& scheduler,
    MetricsRegistry& metrics
) : storage_(storage), scheduler_(scheduler), metrics_(metrics) {}

RecoveryReport RecoveryManager::recover_unsettled_events() {
    auto start_time = std::chrono::steady_clock::now();
    RecoveryReport report;

    auto unsettled = storage_.get_unsettled_events();

    for (auto& event : unsettled) {
        if (event.status == EventStatus::IN_FLIGHT) {
            // In-flight event was orphaned by crash
            if (event.attempt_count >= event.max_retries) {
                storage_.update_event_status(
                    event.id,
                    EventStatus::DEAD_LETTER,
                    event.attempt_count,
                    "Crash recovery: max retries reached during in-flight interruption"
                );
                report.dlq_due_to_crash_limit++;
            } else {
                storage_.update_event_status(
                    event.id,
                    EventStatus::PENDING,
                    event.attempt_count,
                    "Crash recovery: transitioned in-flight event back to pending"
                );
                event.status = EventStatus::PENDING;
                scheduler_.push(event);
                report.in_flight_recovered++;
                report.total_recovered++;
            }
        } else if (event.status == EventStatus::PENDING) {
            scheduler_.push(event);
            report.pending_recovered++;
            report.total_recovered++;
        } else if (event.status == EventStatus::FAILED_RETRYABLE) {
            scheduler_.push(event);
            report.retryable_recovered++;
            report.total_recovered++;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    report.duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    metrics_.record_recovery(report.total_recovered, report.duration_ms);

    return report;
}

} // namespace relayforge
