#include "relayforge/event_handlers.hpp"
#include <chrono>
#include <thread>
#include <sstream>

namespace relayforge {

HandlerRegistry::HandlerRegistry() {
    register_handler("order.created", handle_order_created);
    register_handler("payment.process", handle_payment_process);
    register_handler("notification.send", handle_notification_send);
    register_handler("inventory.reserve", handle_inventory_reserve);
    register_handler("job.execute", handle_job_execute);
}

void HandlerRegistry::register_handler(const std::string& event_type, EventHandlerFn handler) {
    handlers_[event_type] = std::move(handler);
}

EventHandlerFn HandlerRegistry::get_handler(const std::string& event_type) const {
    auto it = handlers_.find(event_type);
    if (it != handlers_.end()) {
        return it->second;
    }
    return nullptr;
}

bool HandlerRegistry::has_handler(const std::string& event_type) const {
    return handlers_.find(event_type) != handlers_.end();
}

static void check_simulation_flags(const Event& event, const nlohmann::json& j) {
    if (j.contains("simulate_fatal") && j["simulate_fatal"].is_string()) {
        throw FatalExecutionException(j["simulate_fatal"].get<std::string>());
    }

    if (j.contains("fail_until_attempt") && j["fail_until_attempt"].is_number_integer()) {
        int threshold = j["fail_until_attempt"].get<int>();
        if (event.attempt_count <= threshold) {
            throw RetryableExecutionException("Simulated transient failure on attempt " + std::to_string(event.attempt_count));
        }
    }

    if (j.contains("simulate_error") && j["simulate_error"].is_string()) {
        throw std::runtime_error(j["simulate_error"].get<std::string>());
    }

    if (j.contains("simulate_latency_ms") && j["simulate_latency_ms"].is_number()) {
        int ms = j["simulate_latency_ms"].get<int>();
        if (ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }
}

std::string HandlerRegistry::handle_order_created(const Event& event) {
    auto j = nlohmann::json::parse(event.payload);
    check_simulation_flags(event, j);

    std::string order_id_str = "unknown";
    if (j.contains("order_id")) {
        if (j["order_id"].is_string()) {
            order_id_str = j["order_id"].get<std::string>();
        } else if (j["order_id"].is_number()) {
            order_id_str = std::to_string(j["order_id"].get<int64_t>());
        }
    }

    nlohmann::json result = {
        {"status", "CONFIRMED"},
        {"event_id", event.id},
        {"order_id", order_id_str},
        {"invoice_number", "INV-" + event.id.substr(0, 8)},
        {"processed_at", current_time_ms()}
    };
    return result.dump();
}

std::string HandlerRegistry::handle_payment_process(const Event& event) {
    auto j = nlohmann::json::parse(event.payload);
    check_simulation_flags(event, j);

    std::string payment_id_str = "unknown";
    if (j.contains("payment_id")) {
        if (j["payment_id"].is_string()) {
            payment_id_str = j["payment_id"].get<std::string>();
        } else if (j["payment_id"].is_number()) {
            payment_id_str = std::to_string(j["payment_id"].get<int64_t>());
        }
    }

    nlohmann::json result = {
        {"status", "SETTLED"},
        {"event_id", event.id},
        {"payment_id", payment_id_str},
        {"auth_code", "AUTH-" + generate_uuid_v4().substr(0, 8)},
        {"amount", j.value("amount", 0.0)},
        {"currency", j.value("currency", "USD")},
        {"processed_at", current_time_ms()}
    };
    return result.dump();
}

std::string HandlerRegistry::handle_notification_send(const Event& event) {
    auto j = nlohmann::json::parse(event.payload);
    check_simulation_flags(event, j);

    nlohmann::json result = {
        {"status", "DELIVERED"},
        {"event_id", event.id},
        {"recipient", j.value("recipient", "")},
        {"channel", "EMAIL"},
        {"delivered_at", current_time_ms()}
    };
    return result.dump();
}

std::string HandlerRegistry::handle_inventory_reserve(const Event& event) {
    auto j = nlohmann::json::parse(event.payload);
    check_simulation_flags(event, j);

    nlohmann::json result = {
        {"status", "RESERVED"},
        {"event_id", event.id},
        {"item_id", j.value("item_id", "")},
        {"quantity", j.value("quantity", 0)},
        {"reservation_id", "RES-" + generate_uuid_v4().substr(0, 8)},
        {"reserved_at", current_time_ms()}
    };
    return result.dump();
}

std::string HandlerRegistry::handle_job_execute(const Event& event) {
    auto j = nlohmann::json::parse(event.payload);
    check_simulation_flags(event, j);

    nlohmann::json result = {
        {"status", "SUCCESS"},
        {"event_id", event.id},
        {"job_name", j.value("job_name", "default_job")},
        {"completed_at", current_time_ms()}
    };
    return result.dump();
}

} // namespace relayforge
