#pragma once

#include "relayforge/types.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stdexcept>

namespace relayforge {

// Thrown when an error occurs that should not be retried
class FatalExecutionException : public std::runtime_error {
public:
    explicit FatalExecutionException(const std::string& msg) : std::runtime_error(msg) {}
};

// Thrown when a transient error occurs that should trigger exponential backoff retry
class RetryableExecutionException : public std::runtime_error {
public:
    explicit RetryableExecutionException(const std::string& msg) : std::runtime_error(msg) {}
};

using EventHandlerFn = std::function<std::string(const Event&)>;

class HandlerRegistry {
public:
    HandlerRegistry();

    void register_handler(const std::string& event_type, EventHandlerFn handler);
    EventHandlerFn get_handler(const std::string& event_type) const;
    bool has_handler(const std::string& event_type) const;

    // Default implementations for domain event types
    static std::string handle_order_created(const Event& event);
    static std::string handle_payment_process(const Event& event);
    static std::string handle_notification_send(const Event& event);
    static std::string handle_inventory_reserve(const Event& event);
    static std::string handle_job_execute(const Event& event);

private:
    std::unordered_map<std::string, EventHandlerFn> handlers_;
};

} // namespace relayforge
