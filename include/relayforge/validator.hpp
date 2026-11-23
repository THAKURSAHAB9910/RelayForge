#pragma once

#include "relayforge/types.hpp"
#include <string>
#include <vector>

namespace relayforge {

struct ValidationResult {
    bool valid = true;
    std::string error_message;

    static ValidationResult success() { return {true, ""}; }
    static ValidationResult failure(std::string msg) { return {false, std::move(msg)}; }
};

class EventValidator {
public:
    explicit EventValidator(size_t max_payload_bytes = 524288); // 512 KB

    ValidationResult validate(const Event& event) const;
    void set_allowed_event_types(const std::vector<std::string>& types);

private:
    size_t max_payload_bytes_;
    std::vector<std::string> allowed_event_types_;

    ValidationResult validate_type_specific_payload(const std::string& type, const nlohmann::json& payload_json) const;
};

} // namespace relayforge
