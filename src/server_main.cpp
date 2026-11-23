#include "relayforge/engine.hpp"
#include <httplib.h>
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signum) {
    (void)signum;
    g_shutdown_requested.store(true);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    int port = 8080;
    relayforge::EngineConfig config;
    config.db_path = "relayforge.db";
    config.worker_count = 4;
    config.max_queue_depth = 50000;
    config.rate_limit_eps = 50000;
    config.burst_size = 100000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--workers" && i + 1 < argc) {
            config.worker_count = std::stoul(argv[++i]);
        } else if (arg == "--db" && i + 1 < argc) {
            config.db_path = argv[++i];
        } else if (arg == "--max-queue" && i + 1 < argc) {
            config.max_queue_depth = std::stoul(argv[++i]);
        } else if (arg == "--rate-limit" && i + 1 < argc) {
            config.rate_limit_eps = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "RelayForge - Fault-Tolerant Event Processing Engine\n"
                      << "Usage: relayforge_server [options]\n"
                      << "Options:\n"
                      << "  --port <port>         HTTP port (default 8080)\n"
                      << "  --workers <count>     Worker pool threads (default 4)\n"
                      << "  --db <path>           SQLite WAL database path (default relayforge.db)\n"
                      << "  --max-queue <size>    Backpressure queue depth threshold (default 50000)\n"
                      << "  --rate-limit <eps>    Rate limiter events per second (default 50000)\n";
            return 0;
        }
    }

    std::cout << "========================================================\n"
              << "  RelayForge Fault-Tolerant Event Processing Engine     \n"
              << "========================================================\n"
              << " Port:            " << port << "\n"
              << " Database:        " << config.db_path << "\n"
              << " Worker Threads:  " << config.worker_count << "\n"
              << " Max Queue Depth: " << config.max_queue_depth << "\n"
              << " Rate Limit EPS:  " << config.rate_limit_eps << "\n"
              << "========================================================" << std::endl;

    relayforge::RelayEngine engine(config);
    if (!engine.start()) {
        std::cerr << "Failed to start RelayForge engine!" << std::endl;
        return 1;
    }

    auto server = std::make_unique<httplib::Server>();

    // 1. POST /events
    server->Post("/events", [&engine](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);

            relayforge::Event event;
            if (j.contains("id") && j["id"].is_string()) {
                event.id = j["id"].get<std::string>();
            }
            if (j.contains("idempotency_key") && j["idempotency_key"].is_string()) {
                event.idempotency_key = j["idempotency_key"].get<std::string>();
            }
            if (j.contains("event_type") && j["event_type"].is_string()) {
                event.event_type = j["event_type"].get<std::string>();
            }
            if (j.contains("priority")) {
                if (j["priority"].is_string()) {
                    event.priority = relayforge::priority_from_string(j["priority"].get<std::string>());
                } else if (j["priority"].is_number()) {
                    event.priority = static_cast<relayforge::EventPriority>(j["priority"].get<int>());
                }
            }
            if (j.contains("max_retries") && j["max_retries"].is_number_integer()) {
                event.max_retries = j["max_retries"].get<int>();
            }
            if (j.contains("payload")) {
                if (j["payload"].is_string()) {
                    event.payload = j["payload"].get<std::string>();
                } else {
                    event.payload = j["payload"].dump();
                }
            }

            auto ingest_result = engine.ingest(event);
            res.status = ingest_result.http_status_code;
            res.set_content(ingest_result.to_json().dump(), "application/json");
        } catch (const nlohmann::json::parse_error& e) {
            res.status = 400;
            nlohmann::json err = {
                {"status", "ERROR"},
                {"message", std::string("Malformed JSON body: ") + e.what()}
            };
            res.set_content(err.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            nlohmann::json err = {
                {"status", "INTERNAL_ERROR"},
                {"message", e.what()}
            };
            res.set_content(err.dump(), "application/json");
        }
    });

    // 2. GET /events/:id
    server->Get(R"(/events/([a-zA-Z0-9_-]+))", [&engine](const httplib::Request& req, httplib::Response& res) {
        std::string event_id = req.matches[1];
        auto maybe_event = engine.get_event(event_id);
        if (!maybe_event.has_value()) {
            res.status = 404;
            nlohmann::json err = {{"error", "Event not found"}, {"id", event_id}};
            res.set_content(err.dump(), "application/json");
            return;
        }

        nlohmann::json resp = maybe_event->to_json();
        auto audit_trail = engine.get_event_audit_trail(event_id);
        nlohmann::json trail_arr = nlohmann::json::array();
        for (const auto& a : audit_trail) {
            trail_arr.push_back(a.to_json());
        }
        resp["audit_trail"] = trail_arr;

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    });

    // 3. GET /dlq
    server->Get("/dlq", [&engine](const httplib::Request& req, httplib::Response& res) {
        int limit = 50;
        int offset = 0;
        if (req.has_param("limit")) {
            limit = std::stoi(req.get_param_value("limit"));
        }
        if (req.has_param("offset")) {
            offset = std::stoi(req.get_param_value("offset"));
        }

        auto dlq_list = engine.get_dead_letter_events(limit, offset);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : dlq_list) {
            arr.push_back(e.to_json());
        }

        nlohmann::json resp = {
            {"dead_letter_events", arr},
            {"count", arr.size()},
            {"limit", limit},
            {"offset", offset}
        };

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    });

    // 4. POST /dlq/:id/replay
    server->Post(R"(/dlq/([a-zA-Z0-9_-]+)/replay)", [&engine](const httplib::Request& req, httplib::Response& res) {
        std::string event_id = req.matches[1];
        if (engine.replay_dlq_event(event_id)) {
            res.status = 200;
            nlohmann::json resp = {
                {"status", "REPLAY_QUEUED"},
                {"event_id", event_id},
                {"message", "Event reset to PENDING and re-enqueued to scheduler"}
            };
            res.set_content(resp.dump(), "application/json");
        } else {
            res.status = 404;
            nlohmann::json resp = {
                {"status", "FAILED"},
                {"event_id", event_id},
                {"message", "Event not found in DEAD_LETTER status or already active"}
            };
            res.set_content(resp.dump(), "application/json");
        }
    });

    // 5. GET /metrics (Prometheus format)
    server->Get("/metrics", [&engine](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content(engine.metrics().render_prometheus(), "text/plain; version=0.0.4; charset=utf-8");
    });

    // 6. GET /stats (JSON format)
    server->Get("/stats", [&engine](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content(engine.metrics().to_json().dump(2), "application/json");
    });

    // 7. GET /health
    server->Get("/health", [&engine](const httplib::Request&, httplib::Response& res) {
        nlohmann::json health = {
            {"status", "UP"},
            {"engine_running", engine.is_running()},
            {"workers_active", engine.metrics().to_json()["in_flight_count"]},
            {"queue_depth", engine.metrics().to_json()["queue_depth"]}
        };
        res.status = 200;
        res.set_content(health.dump(), "application/json");
    });

    std::cout << "[RelayForge Server] HTTP REST Server listening on 0.0.0.0:" << port << std::endl;

    std::thread server_thread([&server, port]() {
        server->listen("0.0.0.0", port);
    });

    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n[RelayForge Server] Stopping HTTP server..." << std::endl;
    server->stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    std::cout << "[RelayForge Server] Stopping engine..." << std::endl;
    engine.stop(true);
    std::cout << "[RelayForge Server] Shutdown clean." << std::endl;

    return 0;
}
