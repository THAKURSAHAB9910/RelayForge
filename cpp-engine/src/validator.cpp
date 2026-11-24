#include "relayforge/validator.hpp"
#include <algorithm>
#include <regex>

namespace relayforge {

EventValidator::EventValidator(size_t max_payload_bytes)
    : max_payload_bytes_(max_payload_bytes) {
    allowed_event_types_ = {
        "order.created",
        "payment.process",
        "notification.send",
        "inventory.reserve",
        "job.execute"
    };
}

void EventValidator::set_allowed_event_types(const std::vector<std::string>& types) {
    allowed_event_types_ = types;
}

ValidationResult EventValidator::validate_type_specific_payload(
    const std::string& type,
    const nlohmann::json& j
) const {
    if (!j.is_object()) {
        return ValidationResult::failure("Payload must be a JSON object");
    }

    if (type == "order.created") {
        if (!j.contains("order_id") || (!j["order_id"].is_string() && !j["order_id"].is_number())) {
            return ValidationResult::failure("order.created requires 'order_id' as string or number");
        }
        if (!j.contains("amount") || !j["amount"].is_number() || j["amount"].get<double>() <= 0.0) {
            return ValidationResult::failure("order.created requires 'amount' as positive number");
        }
    } else if (type == "payment.process") {
        if (!j.contains("payment_id") || (!j["payment_id"].is_string() && !j["payment_id"].is_number())) {
            return ValidationResult::failure("payment.process requires 'payment_id'");
        }
        if (!j.contains("amount") || !j["amount"].is_number() || j["amount"].get<double>() <= 0.0) {
            return ValidationResult::failure("payment.process requires positive 'amount'");
        }
        if (!j.contains("currency") || !j["currency"].is_string()) {
            return ValidationResult::failure("payment.process requires 'currency' as string");
        }
    } else if (type == "notification.send") {
        if (!j.contains("recipient") || !j["recipient"].is_string() || j["recipient"].get<std::string>().empty()) {
            return ValidationResult::failure("notification.send requires non-empty 'recipient'");
        }
        if (!j.contains("message") || !j["message"].is_string()) {
            return ValidationResult::failure("notification.send requires 'message' string");
        }
    } else if (type == "inventory.reserve") {
        if (!j.contains("item_id") || !j["item_id"].is_string()) {
            return ValidationResult::failure("inventory.reserve requires 'item_id'");
        }
        if (!j.contains("quantity") || !j["quantity"].is_number_integer() || j["quantity"].get<int>() <= 0) {
            return ValidationResult::failure("inventory.reserve requires positive integer 'quantity'");
        }
    }

    return ValidationResult::success();
}

ValidationResult EventValidator::validate(const Event& event) const {
    if (event.id.empty()) {
        return ValidationResult::failure("Event ID cannot be empty");
    }

    if (event.event_type.empty()) {
        return ValidationResult::failure("Event type cannot be empty");
    }

    static const std::regex type_regex("^[a-zA-Z0-9_.-]+$");
    if (!std::regex_match(event.event_type, type_regex)) {
        return ValidationResult::failure("Event type contains invalid characters; must match [a-zA-Z0-9_.-]+");
    }

    if (!allowed_event_types_.empty()) {
        auto it = std::find(allowed_event_types_.begin(), allowed_event_types_.end(), event.event_type);
        if (it == allowed_event_types_.end()) {
            return ValidationResult::failure("Event type '" + event.event_type + "' is not recognized");
        }
    }

    int priority_val = static_cast<int>(event.priority);
    if (priority_val < 0 || priority_val > 3) {
        return ValidationResult::failure("Priority value out of range (0=CRITICAL, 1=HIGH, 2=NORMAL, 3=LOW)");
    }

    if (event.max_retries < 0 || event.max_retries > 20) {
        return ValidationResult::failure("max_retries must be between 0 and 20");
    }

    if (event.payload.empty()) {
        return ValidationResult::failure("Payload cannot be empty");
    }

    if (event.payload.size() > max_payload_bytes_) {
        return ValidationResult::failure("Payload size (" + std::to_string(event.payload.size()) +
                                         " bytes) exceeds maximum allowed size (" +
                                         std::to_string(max_payload_bytes_) + " bytes)");
    }

    nlohmann::json parsed_json;
    try {
        parsed_json = nlohmann::json::parse(event.payload);
    } catch (const nlohmann::json::parse_error& e) {
        return ValidationResult::failure(std::string("Malformed JSON payload: ") + e.what());
    }

    return validate_type_specific_payload(event.event_type, parsed_json);
}

} // namespace relayforge
