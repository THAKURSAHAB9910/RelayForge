#include "relayforge/engine.hpp"
#include <iostream>

namespace relayforge {

RelayEngine::RelayEngine(EngineConfig config)
    : config_(std::move(config)) {}

RelayEngine::~RelayEngine() {
    stop(false);
}

bool RelayEngine::start() {
    if (running_.exchange(true)) {
        return true;
    }

    storage_ = std::make_unique<SqliteStorage>(
        config_.db_path,
        config_.enable_wal,
        config_.synchronous_normal
    );
    if (!storage_->initialize()) {
        std::cerr << "[RelayEngine] Failed to initialize storage at " << config_.db_path << std::endl;
        running_.store(false);
        return false;
    }

    validator_ = std::make_unique<EventValidator>();
    rate_limiter_ = std::make_unique<TokenBucketRateLimiter>(config_.rate_limit_eps, config_.burst_size);
    scheduler_ = std::make_unique<PriorityScheduler>(config_.max_queue_depth);
    scheduler_->start();

    handlers_ = std::make_unique<HandlerRegistry>();
    retry_manager_ = std::make_unique<RetryManager>(config_.default_retry_policy);
    metrics_ = std::make_unique<MetricsRegistry>();

    // Crash Recovery step on startup
    recovery_manager_ = std::make_unique<RecoveryManager>(*storage_, *scheduler_, *metrics_);
    auto report = recovery_manager_->recover_unsettled_events();
    if (report.total_recovered > 0) {
        std::cout << "[RelayEngine] Crash recovery completed: " << report.total_recovered
                  << " events restored (" << report.in_flight_recovered << " in-flight, "
                  << report.pending_recovered << " pending, " << report.retryable_recovered
                  << " retryable) in " << report.duration_ms << " ms" << std::endl;
    }

    worker_pool_ = std::make_unique<WorkerPool>(
        config_.worker_count,
        *scheduler_,
        *storage_,
        *handlers_,
        *retry_manager_
    );

    worker_pool_->set_on_success([this](const Event& e, double dur) {
        metrics_->record_event_success(e.event_type, dur);
    });

    worker_pool_->set_on_retry([this](const Event& e, const std::string&, double dur) {
        metrics_->record_event_retry(e.event_type, dur);
    });

    worker_pool_->set_on_dead_letter([this](const Event& e, const std::string&, double dur) {
        metrics_->record_event_dead_letter(e.event_type, dur);
    });

    worker_pool_->start();

    sync_stopped_.store(false);
    metrics_sync_thread_ = std::thread(&RelayEngine::metrics_sync_loop, this);

    return true;
}

void RelayEngine::stop(bool wait_for_drain) {
    if (!running_.exchange(false)) {
        return;
    }

    sync_stopped_.store(true);
    if (metrics_sync_thread_.joinable()) {
        metrics_sync_thread_.join();
    }

    if (worker_pool_) {
        worker_pool_->stop(wait_for_drain);
    }

    if (scheduler_) {
        scheduler_->shutdown();
    }

    if (storage_) {
        storage_->close();
    }
}

IngestResult RelayEngine::ingest(Event event) {
    if (!running_.load()) {
        return {IngestStatus::STORAGE_ERROR, "", "Engine is stopped", std::nullopt, 503};
    }

    // 1. Rate Limiting Check
    if (!rate_limiter_->try_acquire(1)) {
        metrics_->record_backpressure_drop();
        return {
            IngestStatus::RATE_LIMITED,
            "",
            "Rate limit exceeded (token bucket exhausted)",
            std::nullopt,
            429
        };
    }

    // 2. Event ID generation if not provided
    if (event.id.empty()) {
        event.id = generate_uuid_v4();
    }

    // 3. Validation
    auto val_res = validator_->validate(event);
    if (!val_res.valid) {
        return {
            IngestStatus::VALIDATION_FAILED,
            event.id,
            "Validation failed: " + val_res.error_message,
            std::nullopt,
            400
        };
    }

    // 4. Backpressure Check
    if (scheduler_->total_depth() >= config_.max_queue_depth) {
        metrics_->record_backpressure_drop();
        return {
            IngestStatus::BACKPRESSURE_EXCEEDED,
            event.id,
            "Engine backpressure triggered: scheduler queue capacity saturated",
            std::nullopt,
            429
        };
    }

    // 5. Durable Storage Persistence before Acknowledgment (WAL commit)
    auto [inserted, stored_event] = storage_->insert_event(event);
    if (!inserted) {
        // Idempotency Deduplication: Event with this idempotency key already exists!
        return {
            IngestStatus::DUPLICATE,
            stored_event.id,
            "Duplicate event deduplicated by idempotency key",
            stored_event,
            200 // OK with cached event
        };
    }

    // 6. Push to Scheduler (In-Memory Priority Queue)
    if (!scheduler_->push(stored_event)) {
        // Queue saturated right after insert
        metrics_->record_backpressure_drop();
        return {
            IngestStatus::BACKPRESSURE_EXCEEDED,
            stored_event.id,
            "Engine backpressure triggered: unable to enqueue event",
            stored_event,
            429
        };
    }

    // 7. Record Metrics
    metrics_->record_event_received(stored_event.event_type, stored_event.priority);

    return {
        IngestStatus::ACCEPTED,
        stored_event.id,
        "Event accepted and persisted to WAL",
        stored_event,
        202 // Accepted
    };
}

std::optional<Event> RelayEngine::get_event(const std::string& event_id) {
    if (!storage_) return std::nullopt;
    return storage_->get_event(event_id);
}

std::optional<Event> RelayEngine::get_event_by_idempotency_key(const std::string& key) {
    if (!storage_) return std::nullopt;
    return storage_->get_event_by_idempotency_key(key);
}

std::vector<Event> RelayEngine::get_dead_letter_events(int limit, int offset) {
    if (!storage_) return {};
    return storage_->get_dead_letter_events(limit, offset);
}

bool RelayEngine::replay_dlq_event(const std::string& event_id) {
    if (!storage_ || !scheduler_) return false;

    if (!storage_->reset_event_for_replay(event_id)) {
        return false;
    }

    auto maybe_event = storage_->get_event(event_id);
    if (!maybe_event.has_value()) {
        return false;
    }

    return scheduler_->push(*maybe_event);
}

std::vector<AuditLogEntry> RelayEngine::get_event_audit_trail(const std::string& event_id) {
    if (!storage_) return {};
    return storage_->get_audit_log(event_id);
}

void RelayEngine::metrics_sync_loop() {
    while (!sync_stopped_.load()) {
        if (scheduler_ && metrics_ && worker_pool_) {
            metrics_->set_queue_depth(scheduler_->total_depth());
            metrics_->set_in_flight_count(worker_pool_->active_worker_count());
            metrics_->set_worker_utilization(worker_pool_->worker_utilization());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace relayforge
