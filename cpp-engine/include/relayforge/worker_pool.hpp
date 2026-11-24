#pragma once

#include "relayforge/types.hpp"
#include "relayforge/storage_engine.hpp"
#include "relayforge/priority_scheduler.hpp"
#include "relayforge/event_handlers.hpp"
#include "relayforge/retry_manager.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>

namespace relayforge {

using SuccessCallback = std::function<void(const Event&, double duration_ms)>;
using FailureCallback = std::function<void(const Event&, const std::string& error, double duration_ms)>;

class WorkerPool {
public:
    WorkerPool(
        size_t worker_count,
        PriorityScheduler& scheduler,
        StorageEngine& storage,
        HandlerRegistry& handlers,
        RetryManager& retry_manager
    );
    ~WorkerPool();

    void start();
    void stop(bool wait_for_drain = true);

    size_t worker_count() const { return worker_count_; }
    size_t active_worker_count() const { return active_workers_count_.load(); }
    double worker_utilization() const;

    void set_on_success(SuccessCallback cb) { on_success_ = std::move(cb); }
    void set_on_retry(FailureCallback cb) { on_retry_ = std::move(cb); }
    void set_on_dead_letter(FailureCallback cb) { on_dead_letter_ = std::move(cb); }

    bool is_running() const { return running_.load(); }

private:
    size_t worker_count_;
    PriorityScheduler& scheduler_;
    StorageEngine& storage_;
    HandlerRegistry& handlers_;
    RetryManager& retry_manager_;

    std::atomic<size_t> active_workers_count_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};

    std::vector<std::thread> workers_;

    SuccessCallback on_success_;
    FailureCallback on_retry_;
    FailureCallback on_dead_letter_;

    void worker_loop(size_t worker_id);
    void process_event(Event event);
};

} // namespace relayforge
