#include "relayforge/worker_pool.hpp"
#include <iostream>

namespace relayforge {

WorkerPool::WorkerPool(
    size_t worker_count,
    PriorityScheduler& scheduler,
    StorageEngine& storage,
    HandlerRegistry& handlers,
    RetryManager& retry_manager
) : worker_count_(worker_count > 0 ? worker_count : 1),
    scheduler_(scheduler),
    storage_(storage),
    handlers_(handlers),
    retry_manager_(retry_manager) {}

WorkerPool::~WorkerPool() {
    stop(false);
}

void WorkerPool::start() {
    if (running_.exchange(true)) {
        return;
    }
    stopped_.store(false);

    workers_.reserve(worker_count_);
    for (size_t i = 0; i < worker_count_; ++i) {
        workers_.emplace_back(&WorkerPool::worker_loop, this, i);
    }
}

void WorkerPool::stop(bool wait_for_drain) {
    if (!running_.exchange(false)) {
        return;
    }
    stopped_.store(true);

    if (wait_for_drain) {
        // Wait for scheduler queue to drain or timeout
        auto start = std::chrono::steady_clock::now();
        while ((scheduler_.active_depth() > 0 || active_workers_count_.load() > 0) &&
               std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    scheduler_.shutdown();

    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers_.clear();
}

double WorkerPool::worker_utilization() const {
    if (worker_count_ == 0) return 0.0;
    return static_cast<double>(active_workers_count_.load()) / static_cast<double>(worker_count_);
}

void WorkerPool::worker_loop(size_t /*worker_id*/) {
    while (!stopped_.load()) {
        auto maybe_event = scheduler_.pop(std::chrono::milliseconds(100));
        if (!maybe_event.has_value()) {
            continue;
        }

        active_workers_count_.fetch_add(1);
        try {
            process_event(std::move(*maybe_event));
        } catch (...) {
            std::cerr << "[WorkerPool] Uncaught outer fatal exception in worker thread!" << std::endl;
        }
        active_workers_count_.fetch_sub(1);
    }
}

void WorkerPool::process_event(Event event) {
    auto start_time = std::chrono::steady_clock::now();

    event.attempt_count += 1;
    int current_attempt = event.attempt_count;

    // Mark IN_FLIGHT in database before processing
    storage_.update_event_status(
        event.id,
        EventStatus::IN_FLIGHT,
        current_attempt
    );

    auto handler = handlers_.get_handler(event.event_type);
    if (!handler) {
        // Missing handler is unrecoverable fatal failure -> Dead letter
        auto end_time = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        std::string err = "No handler registered for event type: " + event.event_type;

        storage_.update_event_status(
            event.id,
            EventStatus::DEAD_LETTER,
            current_attempt,
            err,
            "",
            0,
            duration_ms
        );

        if (on_dead_letter_) {
            on_dead_letter_(event, err, duration_ms);
        }
        return;
    }

    try {
        std::string execution_result = handler(event);

        auto end_time = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        // Mark COMPLETED in storage
        storage_.update_event_status(
            event.id,
            EventStatus::COMPLETED,
            current_attempt,
            "",
            execution_result,
            0,
            duration_ms
        );

        if (on_success_) {
            on_success_(event, duration_ms);
        }
    } catch (const FatalExecutionException& ex) {
        auto end_time = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        storage_.update_event_status(
            event.id,
            EventStatus::DEAD_LETTER,
            current_attempt,
            ex.what(),
            "",
            0,
            duration_ms
        );

        if (on_dead_letter_) {
            on_dead_letter_(event, ex.what(), duration_ms);
        }
    } catch (const std::exception& ex) {
        auto end_time = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        auto eval = retry_manager_.evaluate_failure(event, ex.what(), false);

        if (eval.decision == RetryDecision::DEAD_LETTER) {
            storage_.update_event_status(
                event.id,
                EventStatus::DEAD_LETTER,
                current_attempt,
                ex.what(),
                "",
                0,
                duration_ms
            );

            if (on_dead_letter_) {
                on_dead_letter_(event, ex.what(), duration_ms);
            }
        } else {
            // Schedule retry
            storage_.update_event_status(
                event.id,
                EventStatus::FAILED_RETRYABLE,
                current_attempt,
                ex.what(),
                "",
                eval.next_scheduled_at,
                duration_ms
            );

            event.scheduled_at = eval.next_scheduled_at;
            event.status = EventStatus::FAILED_RETRYABLE;
            event.last_error = ex.what();

            // Re-enqueue into delayed retry queue in scheduler
            scheduler_.push(event);

            if (on_retry_) {
                on_retry_(event, ex.what(), duration_ms);
            }
        }
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        std::string err = "Unknown unhandled exception caught in worker";

        auto eval = retry_manager_.evaluate_failure(event, err, false);

        if (eval.decision == RetryDecision::DEAD_LETTER) {
            storage_.update_event_status(
                event.id,
                EventStatus::DEAD_LETTER,
                current_attempt,
                err,
                "",
                0,
                duration_ms
            );

            if (on_dead_letter_) {
                on_dead_letter_(event, err, duration_ms);
            }
        } else {
            storage_.update_event_status(
                event.id,
                EventStatus::FAILED_RETRYABLE,
                current_attempt,
                err,
                "",
                eval.next_scheduled_at,
                duration_ms
            );

            event.scheduled_at = eval.next_scheduled_at;
            event.status = EventStatus::FAILED_RETRYABLE;
            event.last_error = err;

            scheduler_.push(event);

            if (on_retry_) {
                on_retry_(event, err, duration_ms);
            }
        }
    }
}

} // namespace relayforge
