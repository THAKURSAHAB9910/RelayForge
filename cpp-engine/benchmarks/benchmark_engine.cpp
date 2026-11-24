#include "relayforge/engine.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sys/resource.h>
#include <sys/time.h>
#include <filesystem>
#include <fstream>

using namespace relayforge;

struct BenchmarkResult {
    size_t event_volume;
    size_t worker_count;
    double duration_sec;
    double throughput_eps;
    double p50_ms;
    double p95_ms;
    double p99_ms;
    double peak_memory_mb;
    double cpu_user_sec;
    double cpu_sys_sec;
};

static double get_peak_rss_mb() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss is in kilobytes on Linux
        return static_cast<double>(usage.ru_maxrss) / 1024.0;
    }
    return 0.0;
}

static std::pair<double, double> get_cpu_times() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        double user = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec * 1e-6;
        double sys = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec * 1e-6;
        return {user, sys};
    }
    return {0.0, 0.0};
}

BenchmarkResult run_volume_benchmark(size_t volume, size_t workers) {
    std::string db_path = "/tmp/rf_bench_" + std::to_string(volume) + "_w" + std::to_string(workers) + ".db";
    std::filesystem::remove(db_path);
    std::filesystem::remove(db_path + "-wal");
    std::filesystem::remove(db_path + "-shm");

    EngineConfig config;
    config.db_path = db_path;
    config.worker_count = workers;
    config.max_queue_depth = volume + 10000;
    config.rate_limit_eps = 500000;
    config.burst_size = 500000;
    config.enable_wal = true;
    config.synchronous_normal = true;

    auto [cpu_u_start, cpu_s_start] = get_cpu_times();

    RelayEngine engine(config);
    if (!engine.start()) {
        std::cerr << "Failed to start engine for bench volume: " << volume << std::endl;
        return {};
    }

    auto start_time = std::chrono::steady_clock::now();

    // Ingest all events
    for (size_t i = 0; i < volume; ++i) {
        Event e;
        e.id = "evt-" + std::to_string(i);
        e.idempotency_key = "idemp-" + std::to_string(volume) + "-" + std::to_string(i);
        e.event_type = (i % 3 == 0) ? "order.created" : ((i % 3 == 1) ? "payment.process" : "inventory.reserve");
        e.priority = static_cast<EventPriority>(i % 4);
        if (e.event_type == "order.created") {
            e.payload = "{\"order_id\": " + std::to_string(i) + ", \"amount\": 29.95}";
        } else if (e.event_type == "payment.process") {
            e.payload = "{\"payment_id\": " + std::to_string(i) + ", \"amount\": 29.95, \"currency\": \"USD\"}";
        } else {
            e.payload = "{\"item_id\": \"ITEM-" + std::to_string(i % 100) + "\", \"quantity\": 1}";
        }

        engine.ingest(e);
    }

    // Wait until all events are processed to completion
    while (engine.metrics().total_success() < volume) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end_time = std::chrono::steady_clock::now();
    double duration_sec = std::chrono::duration<double>(end_time - start_time).count();
    auto [cpu_u_end, cpu_s_end] = get_cpu_times();

    BenchmarkResult res;
    res.event_volume = volume;
    res.worker_count = workers;
    res.duration_sec = duration_sec;
    res.throughput_eps = static_cast<double>(volume) / duration_sec;
    res.p50_ms = engine.metrics().get_p50_latency_ms();
    res.p95_ms = engine.metrics().get_p95_latency_ms();
    res.p99_ms = engine.metrics().get_p99_latency_ms();
    res.peak_memory_mb = get_peak_rss_mb();
    res.cpu_user_sec = cpu_u_end - cpu_u_start;
    res.cpu_sys_sec = cpu_s_end - cpu_s_start;

    engine.stop(true);

    std::filesystem::remove(db_path);
    std::filesystem::remove(db_path + "-wal");
    std::filesystem::remove(db_path + "-shm");

    return res;
}

double test_crash_recovery_time(size_t volume) {
    std::string db_path = "/tmp/rf_bench_recovery.db";
    std::filesystem::remove(db_path);
    std::filesystem::remove(db_path + "-wal");
    std::filesystem::remove(db_path + "-shm");

    {
        SqliteStorage storage(db_path, true, true);
        storage.initialize();

        for (size_t i = 0; i < volume; ++i) {
            Event e;
            e.id = "rec-" + std::to_string(i);
            e.event_type = "order.created";
            e.payload = "{\"order_id\": " + std::to_string(i) + ", \"amount\": 10.0}";
            e.priority = static_cast<EventPriority>(i % 4);
            storage.insert_event(e);
            if (i < volume / 2) {
                storage.update_event_status(e.id, EventStatus::IN_FLIGHT, 1);
            }
        }
        storage.close();
    }

    // Now measure recovery time
    auto start = std::chrono::steady_clock::now();
    {
        SqliteStorage storage(db_path, true, true);
        storage.initialize();

        PriorityScheduler scheduler(volume + 1000);
        MetricsRegistry metrics;
        RecoveryManager recovery(storage, scheduler, metrics);

        auto report = recovery.recover_unsettled_events();
        auto end = std::chrono::steady_clock::now();
        double dur_ms = std::chrono::duration<double, std::milli>(end - start).count();

        storage.close();

        std::filesystem::remove(db_path);
        std::filesystem::remove(db_path + "-wal");
        std::filesystem::remove(db_path + "-shm");

        return dur_ms;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================================================\n"
              << "          RelayForge High-Performance Benchmark Suite                   \n"
              << "========================================================================\n" << std::endl;

    std::vector<size_t> volumes = {1000, 10000, 100000};
    std::vector<size_t> worker_configs = {1, 2, 4, 8};

    if (argc > 1 && std::string(argv[1]) == "--quick") {
        volumes = {1000, 10000};
        worker_configs = {1, 4};
    }

    std::vector<BenchmarkResult> results;
    nlohmann::json json_results = nlohmann::json::array();

    for (size_t vol : volumes) {
        std::cout << ">>> Running Benchmarks for Volume = " << vol << " events <<<" << std::endl;
        for (size_t workers : worker_configs) {
            std::cout << "  [Workers: " << workers << "] Benchmarking... " << std::flush;
            auto res = run_volume_benchmark(vol, workers);
            results.push_back(res);

            std::cout << std::fixed << std::setprecision(2)
                      << "Done in " << res.duration_sec << " s | "
                      << res.throughput_eps << " events/sec | "
                      << "p50: " << res.p50_ms << " ms | "
                      << "p95: " << res.p95_ms << " ms | "
                      << "p99: " << res.p99_ms << " ms | "
                      << "Peak Mem: " << res.peak_memory_mb << " MB | "
                      << "CPU: " << (res.cpu_user_sec + res.cpu_sys_sec) << " s"
                      << std::endl;

            json_results.push_back({
                {"event_volume", res.event_volume},
                {"workers", res.worker_count},
                {"duration_sec", res.duration_sec},
                {"throughput_eps", res.throughput_eps},
                {"p50_ms", res.p50_ms},
                {"p95_ms", res.p95_ms},
                {"p99_ms", res.p99_ms},
                {"peak_memory_mb", res.peak_memory_mb},
                {"cpu_user_sec", res.cpu_user_sec},
                {"cpu_sys_sec", res.cpu_sys_sec}
            });
        }
        std::cout << std::endl;
    }

    std::cout << ">>> Measuring Crash Recovery Performance <<<" << std::endl;
    std::vector<size_t> recovery_volumes = {1000, 10000, 50000};
    nlohmann::json recovery_results = nlohmann::json::array();
    for (size_t rvol : recovery_volumes) {
        double rec_ms = test_crash_recovery_time(rvol);
        std::cout << "  Recovered " << rvol << " unsettled events in: " << rec_ms << " ms ("
                  << std::fixed << std::setprecision(0) << (static_cast<double>(rvol) / (rec_ms / 1000.0)) << " events/sec recovery rate)"
                  << std::endl;
        recovery_results.push_back({
            {"volume", rvol},
            {"recovery_time_ms", rec_ms},
            {"recovery_rate_eps", static_cast<double>(rvol) / (rec_ms / 1000.0)}
        });
    }

    nlohmann::json report = {
        {"benchmark_runs", json_results},
        {"crash_recovery", recovery_results}
    };

    std::ofstream out("benchmarks_report.json");
    out << report.dump(2);
    out.close();

    std::cout << "\nBenchmark results exported to benchmarks_report.json." << std::endl;
    return 0;
}
