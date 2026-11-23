#pragma once

#include "relayforge/types.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <utility>

namespace relayforge {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual bool initialize() = 0;
    virtual void close() = 0;

    // Returns {true, event} if newly inserted; {false, existing_event} if duplicate idempotency key
    virtual std::pair<bool, Event> insert_event(const Event& event) = 0;

    virtual bool update_event_status(
        const std::string& event_id,
        EventStatus new_status,
        int attempt_count,
        const std::string& last_error = "",
        const std::string& execution_result = "",
        int64_t scheduled_at = 0,
        double duration_ms = 0.0
    ) = 0;

    virtual std::optional<Event> get_event(const std::string& event_id) = 0;
    virtual std::optional<Event> get_event_by_idempotency_key(const std::string& key) = 0;

    // Crash recovery query
    virtual std::vector<Event> get_unsettled_events() = 0;

    // DLQ Inspection & Replay
    virtual std::vector<Event> get_dead_letter_events(int limit = 100, int offset = 0) = 0;
    virtual bool reset_event_for_replay(const std::string& event_id) = 0;

    // Audit logs
    virtual std::vector<AuditLogEntry> get_audit_log(const std::string& event_id) = 0;

    // Metrics counts
    virtual size_t count_events_by_status(EventStatus status) = 0;
    virtual size_t count_total_events() = 0;
};

} // namespace relayforge
