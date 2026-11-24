#include "relayforge/metrics_registry.hpp"
#include <algorithm>
#include <random>
#include <iomanip>

namespace relayforge {

MetricsRegistry::MetricsRegistry() {
    start_time_ = std::chrono::steady_clock::now();
    last_throughput_calc_ = start_time_;
    latency_reservoir_.reserve(RESERVOIR_CAPACITY);
}

void MetricsRegistry::reset() {
    events_received_total_.store(0);
    events_success_total_.store(0);
    events_failed_total_.store(0);
    events_retried_total_.store(0);
    events_dlq_total_.store(0);
    backpressure_drops_total_.store(0);
    queue_depth_.store(0);
    in_flight_count_.store(0);
    worker_utilization_.store(0.0);
    recovered_events_count_.store(0);
    recovery_duration_ms_.store(0.0);

    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_reservoir_.clear();
        sample_count_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(throughput_mutex_);
        start_time_ = std::chrono::steady_clock::now();
        last_throughput_calc_ = start_time_;
        last_success_count_ = 0;
        current_throughput_ = 0.0;
    }
}

void MetricsRegistry::record_event_received(const std::string& /*type*/, EventPriority /*priority*/) {
    events_received_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_event_success(const std::string& /*type*/, double duration_ms) {
    events_success_total_.fetch_add(1, std::memory_order_relaxed);
    add_latency_sample(duration_ms);
}

void MetricsRegistry::record_event_retry(const std::string& /*type*/, double duration_ms) {
    events_retried_total_.fetch_add(1, std::memory_order_relaxed);
    events_failed_total_.fetch_add(1, std::memory_order_relaxed);
    add_latency_sample(duration_ms);
}

void MetricsRegistry::record_event_dead_letter(const std::string& /*type*/, double duration_ms) {
    events_dlq_total_.fetch_add(1, std::memory_order_relaxed);
    events_failed_total_.fetch_add(1, std::memory_order_relaxed);
    add_latency_sample(duration_ms);
}

void MetricsRegistry::record_backpressure_drop() {
    backpressure_drops_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::set_queue_depth(size_t depth) {
    queue_depth_.store(depth, std::memory_order_relaxed);
}

void MetricsRegistry::set_in_flight_count(size_t count) {
    in_flight_count_.store(count, std::memory_order_relaxed);
}

void MetricsRegistry::set_worker_utilization(double utilization) {
    worker_utilization_.store(utilization, std::memory_order_relaxed);
}

void MetricsRegistry::record_recovery(size_t event_count, double duration_ms) {
    recovered_events_count_.store(event_count);
    recovery_duration_ms_.store(duration_ms);
}

void MetricsRegistry::add_latency_sample(double duration_ms) {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    sample_count_++;

    if (latency_reservoir_.size() < RESERVOIR_CAPACITY) {
        latency_reservoir_.push_back(duration_ms);
    } else {
        // Algorithm R reservoir sampling
        static thread_local std::mt19937_64 rng(1337);
        std::uniform_int_distribution<size_t> dist(0, sample_count_ - 1);
        size_t j = dist(rng);
        if (j < RESERVOIR_CAPACITY) {
            latency_reservoir_[j] = duration_ms;
        }
    }
}

double MetricsRegistry::calculate_percentile(double percentile) {
    std::vector<double> copy;
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        if (latency_reservoir_.empty()) return 0.0;
        copy = latency_reservoir_;
    }

    std::sort(copy.begin(), copy.end());
    size_t index = static_cast<size_t>(std::ceil(percentile / 100.0 * copy.size())) - 1;
    index = std::min(index, copy.size() - 1);
    return copy[index];
}

double MetricsRegistry::get_p50_latency_ms() {
    return calculate_percentile(50.0);
}

double MetricsRegistry::get_p95_latency_ms() {
    return calculate_percentile(95.0);
}

double MetricsRegistry::get_p99_latency_ms() {
    return calculate_percentile(99.0);
}

double MetricsRegistry::get_throughput_eps() {
    std::lock_guard<std::mutex> lock(throughput_mutex_);
    auto now = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(now - start_time_).count();

    if (total_sec > 0.05) {
        uint64_t current_success = events_success_total_.load();
        current_throughput_ = static_cast<double>(current_success) / total_sec;
    }
    return current_throughput_;
}

nlohmann::json MetricsRegistry::to_json() {
    return nlohmann::json{
        {"events_received_total", events_received_total_.load()},
        {"events_success_total", events_success_total_.load()},
        {"events_failed_total", events_failed_total_.load()},
        {"events_retried_total", events_retried_total_.load()},
        {"events_dlq_total", events_dlq_total_.load()},
        {"backpressure_drops_total", backpressure_drops_total_.load()},
        {"queue_depth", queue_depth_.load()},
        {"in_flight_count", in_flight_count_.load()},
        {"worker_utilization", worker_utilization_.load()},
        {"throughput_eps", get_throughput_eps()},
        {"latency_ms", {
            {"p50", get_p50_latency_ms()},
            {"p95", get_p95_latency_ms()},
            {"p99", get_p99_latency_ms()}
        }},
        {"recovery", {
            {"recovered_events_count", recovered_events_count_.load()},
            {"recovery_duration_ms", recovery_duration_ms_.load()}
        }}
    };
}

std::string MetricsRegistry::render_prometheus() {
    std::stringstream ss;
    ss << "# HELP relayforge_events_received_total Total events accepted by engine\n";
    ss << "# TYPE relayforge_events_received_total counter\n";
    ss << "relayforge_events_received_total " << events_received_total_.load() << "\n\n";

    ss << "# HELP relayforge_events_success_total Total events successfully processed\n";
    ss << "# TYPE relayforge_events_success_total counter\n";
    ss << "relayforge_events_success_total " << events_success_total_.load() << "\n\n";

    ss << "# HELP relayforge_events_failed_total Total event failure executions\n";
    ss << "# TYPE relayforge_events_failed_total counter\n";
    ss << "relayforge_events_failed_total " << events_failed_total_.load() << "\n\n";

    ss << "# HELP relayforge_events_retried_total Total retry attempts scheduled\n";
    ss << "# TYPE relayforge_events_retried_total counter\n";
    ss << "relayforge_events_retried_total " << events_retried_total_.load() << "\n\n";

    ss << "# HELP relayforge_events_dlq_total Total events moved to Dead Letter Queue\n";
    ss << "# TYPE relayforge_events_dlq_total counter\n";
    ss << "relayforge_events_dlq_total " << events_dlq_total_.load() << "\n\n";

    ss << "# HELP relayforge_backpressure_drops_total Total events dropped or rejected due to queue saturation\n";
    ss << "# TYPE relayforge_backpressure_drops_total counter\n";
    ss << "relayforge_backpressure_drops_total " << backpressure_drops_total_.load() << "\n\n";

    ss << "# HELP relayforge_queue_depth Current number of events waiting in scheduler\n";
    ss << "# TYPE relayforge_queue_depth gauge\n";
    ss << "relayforge_queue_depth " << queue_depth_.load() << "\n\n";

    ss << "# HELP relayforge_in_flight_count Current number of events being processed by workers\n";
    ss << "# TYPE relayforge_in_flight_count gauge\n";
    ss << "relayforge_in_flight_count " << in_flight_count_.load() << "\n\n";

    ss << "# HELP relayforge_worker_utilization Fraction of active workers (0.0 to 1.0)\n";
    ss << "# TYPE relayforge_worker_utilization gauge\n";
    ss << "relayforge_worker_utilization " << worker_utilization_.load() << "\n\n";

    ss << "# HELP relayforge_processing_throughput_eps Processing throughput in events per second\n";
    ss << "# TYPE relayforge_processing_throughput_eps gauge\n";
    ss << "relayforge_processing_throughput_eps " << get_throughput_eps() << "\n\n";

    ss << "# HELP relayforge_processing_latency_ms Event processing latency in milliseconds\n";
    ss << "# TYPE relayforge_processing_latency_ms summary\n";
    ss << "relayforge_processing_latency_ms{quantile=\"0.5\"} " << get_p50_latency_ms() << "\n";
    ss << "relayforge_processing_latency_ms{quantile=\"0.95\"} " << get_p95_latency_ms() << "\n";
    ss << "relayforge_processing_latency_ms{quantile=\"0.99\"} " << get_p99_latency_ms() << "\n\n";

    ss << "# HELP relayforge_crash_recovered_events_total Events recovered on boot from prior crash\n";
    ss << "# TYPE relayforge_crash_recovered_events_total gauge\n";
    ss << "relayforge_crash_recovered_events_total " << recovered_events_count_.load() << "\n";
    ss << "relayforge_crash_recovery_duration_ms " << recovery_duration_ms_.load() << "\n";

    return ss.str();
}

} // namespace relayforge
