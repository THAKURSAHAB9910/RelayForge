#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <cstdint>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace relayforge {

enum class EventPriority : int {
    CRITICAL = 0,
    HIGH = 1,
    NORMAL = 2,
    LOW = 3
};

inline std::string to_string(EventPriority priority) {
    switch (priority) {
        case EventPriority::CRITICAL: return "CRITICAL";
        case EventPriority::HIGH: return "HIGH";
        case EventPriority::NORMAL: return "NORMAL";
        case EventPriority::LOW: return "LOW";
        default: return "UNKNOWN";
    }
}

inline EventPriority priority_from_string(const std::string& str) {
    if (str == "CRITICAL" || str == "0") return EventPriority::CRITICAL;
    if (str == "HIGH" || str == "1") return EventPriority::HIGH;
    if (str == "LOW" || str == "3") return EventPriority::LOW;
    return EventPriority::NORMAL;
}

enum class EventStatus {
    PENDING,
    IN_FLIGHT,
    COMPLETED,
    FAILED_RETRYABLE,
    DEAD_LETTER
};

inline std::string to_string(EventStatus status) {
    switch (status) {
        case EventStatus::PENDING: return "PENDING";
        case EventStatus::IN_FLIGHT: return "IN_FLIGHT";
        case EventStatus::COMPLETED: return "COMPLETED";
        case EventStatus::FAILED_RETRYABLE: return "FAILED_RETRYABLE";
        case EventStatus::DEAD_LETTER: return "DEAD_LETTER";
        default: return "UNKNOWN";
    }
}

inline EventStatus status_from_string(const std::string& str) {
    if (str == "PENDING") return EventStatus::PENDING;
    if (str == "IN_FLIGHT") return EventStatus::IN_FLIGHT;
    if (str == "COMPLETED") return EventStatus::COMPLETED;
    if (str == "FAILED_RETRYABLE") return EventStatus::FAILED_RETRYABLE;
    if (str == "DEAD_LETTER") return EventStatus::DEAD_LETTER;
    return EventStatus::PENDING;
}

struct RetryPolicy {
    int max_retries = 3;
    int64_t initial_interval_ms = 100;
    double backoff_factor = 2.0;
    int64_t max_interval_ms = 10000;
    bool enable_jitter = true;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RetryPolicy, max_retries, initial_interval_ms, backoff_factor, max_interval_ms, enable_jitter)
};

struct Event {
    std::string id;
    std::string idempotency_key;
    std::string event_type;
    EventPriority priority = EventPriority::NORMAL;
    std::string payload;
    EventStatus status = EventStatus::PENDING;
    int attempt_count = 0;
    int max_retries = 3;
    int64_t created_at = 0;
    int64_t updated_at = 0;
    int64_t scheduled_at = 0;
    std::string last_error;
    std::string execution_result;

    nlohmann::json to_json() const {
        nlohmann::json p = nlohmann::json::accept(payload) ? nlohmann::json::parse(payload) : nlohmann::json(payload);
        return nlohmann::json{
            {"id", id},
            {"idempotency_key", idempotency_key},
            {"event_type", event_type},
            {"priority", to_string(priority)},
            {"priority_val", static_cast<int>(priority)},
            {"payload", p},
            {"status", to_string(status)},
            {"attempt_count", attempt_count},
            {"max_retries", max_retries},
            {"created_at", created_at},
            {"updated_at", updated_at},
            {"scheduled_at", scheduled_at},
            {"last_error", last_error},
            {"execution_result", execution_result}
        };
    }
};

struct AuditLogEntry {
    int64_t id = 0;
    std::string event_id;
    int attempt_number = 0;
    std::string status_transition;
    std::string error_message;
    double duration_ms = 0.0;
    int64_t timestamp = 0;

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"id", id},
            {"event_id", event_id},
            {"attempt_number", attempt_number},
            {"status_transition", status_transition},
            {"error_message", error_message},
            {"duration_ms", duration_ms},
            {"timestamp", timestamp}
        };
    }
};

struct EngineConfig {
    std::string db_path = "relayforge.db";
    size_t worker_count = 4;
    size_t max_queue_depth = 50000;
    int rate_limit_eps = 50000;
    int burst_size = 100000;
    int64_t default_timeout_ms = 30000;
    RetryPolicy default_retry_policy;
    bool enable_wal = true;
    bool synchronous_normal = true;
};

inline int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string generate_uuid_v4();

} // namespace relayforge
