#pragma once

#include "relayforge/types.hpp"
#include "relayforge/sqlite_storage.hpp"
#include "relayforge/validator.hpp"
#include "relayforge/rate_limiter.hpp"
#include "relayforge/priority_scheduler.hpp"
#include "relayforge/event_handlers.hpp"
#include "relayforge/retry_manager.hpp"
#include "relayforge/worker_pool.hpp"
#include "relayforge/recovery_manager.hpp"
#include "relayforge/metrics_registry.hpp"
#include <memory>
#include <atomic>
#include <thread>

namespace relayforge {

enum class IngestStatus {
    ACCEPTED,
    DUPLICATE,
    VALIDATION_FAILED,
    RATE_LIMITED,
    BACKPRESSURE_EXCEEDED,
    STORAGE_ERROR
};

struct IngestResult {
    IngestStatus status;
    std::string event_id;
    std::string message;
    std::optional<Event> event;
    int http_status_code = 202;

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"status", static_cast<int>(status)},
            {"event_id", event_id},
            {"message", message},
            {"http_status_code", http_status_code},
            {"event", event.has_value() ? event->to_json() : nullptr}
        };
    }
};

class RelayEngine {
public:
    explicit RelayEngine(EngineConfig config = EngineConfig{});
    ~RelayEngine();

    bool start();
    void stop(bool wait_for_drain = true);

    IngestResult ingest(Event event);

    std::optional<Event> get_event(const std::string& event_id);
    std::optional<Event> get_event_by_idempotency_key(const std::string& key);
    std::vector<Event> get_dead_letter_events(int limit = 100, int offset = 0);
    bool replay_dlq_event(const std::string& event_id);
    std::vector<AuditLogEntry> get_event_audit_trail(const std::string& event_id);

    MetricsRegistry& metrics() { return *metrics_; }
    PriorityScheduler& scheduler() { return *scheduler_; }
    StorageEngine& storage() { return *storage_; }
    HandlerRegistry& handlers() { return *handlers_; }
    const EngineConfig& config() const { return config_; }

    bool is_running() const { return running_.load(); }

private:
    EngineConfig config_;
    std::atomic<bool> running_{false};

    std::unique_ptr<SqliteStorage> storage_;
    std::unique_ptr<EventValidator> validator_;
    std::unique_ptr<TokenBucketRateLimiter> rate_limiter_;
    std::unique_ptr<PriorityScheduler> scheduler_;
    std::unique_ptr<HandlerRegistry> handlers_;
    std::unique_ptr<RetryManager> retry_manager_;
    std::unique_ptr<WorkerPool> worker_pool_;
    std::unique_ptr<RecoveryManager> recovery_manager_;
    std::unique_ptr<MetricsRegistry> metrics_;

    std::thread metrics_sync_thread_;
    std::atomic<bool> sync_stopped_{false};
    void metrics_sync_loop();
};

} // namespace relayforge
