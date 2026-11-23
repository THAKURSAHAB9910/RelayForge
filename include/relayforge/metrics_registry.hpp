#pragma once

#include "relayforge/types.hpp"
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <cmath>
#include <sstream>
#include <nlohmann/json.hpp>

namespace relayforge {

class MetricsRegistry {
public:
    MetricsRegistry();

    void record_event_received(const std::string& type, EventPriority priority);
    void record_event_success(const std::string& type, double duration_ms);
    void record_event_retry(const std::string& type, double duration_ms);
    void record_event_dead_letter(const std::string& type, double duration_ms);
    void record_backpressure_drop();

    void set_queue_depth(size_t depth);
    void set_in_flight_count(size_t count);
    void set_worker_utilization(double utilization);
    void record_recovery(size_t event_count, double duration_ms);

    // Latency quantiles
    double get_p50_latency_ms();
    double get_p95_latency_ms();
    double get_p99_latency_ms();

    // Throughput estimation (events completed per second over last period)
    double get_throughput_eps();

    uint64_t total_received() const { return events_received_total_.load(); }
    uint64_t total_success() const { return events_success_total_.load(); }
    uint64_t total_retried() const { return events_retried_total_.load(); }
    uint64_t total_dead_letter() const { return events_dlq_total_.load(); }
    uint64_t total_backpressure_drops() const { return backpressure_drops_total_.load(); }

    nlohmann::json to_json();
    std::string render_prometheus();

    void reset();

private:
    std::atomic<uint64_t> events_received_total_{0};
    std::atomic<uint64_t> events_success_total_{0};
    std::atomic<uint64_t> events_failed_total_{0};
    std::atomic<uint64_t> events_retried_total_{0};
    std::atomic<uint64_t> events_dlq_total_{0};
    std::atomic<uint64_t> backpressure_drops_total_{0};

    std::atomic<uint64_t> queue_depth_{0};
    std::atomic<uint64_t> in_flight_count_{0};
    std::atomic<double> worker_utilization_{0.0};

    std::atomic<size_t> recovered_events_count_{0};
    std::atomic<double> recovery_duration_ms_{0.0};

    // Reservoir sampling for latency
    static constexpr size_t RESERVOIR_CAPACITY = 20000;
    std::vector<double> latency_reservoir_;
    size_t sample_count_ = 0;
    std::mutex latency_mutex_;

    // Throughput window tracking
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_throughput_calc_;
    uint64_t last_success_count_ = 0;
    double current_throughput_ = 0.0;
    std::mutex throughput_mutex_;

    void add_latency_sample(double duration_ms);
    double calculate_percentile(double percentile);
};

} // namespace relayforge
