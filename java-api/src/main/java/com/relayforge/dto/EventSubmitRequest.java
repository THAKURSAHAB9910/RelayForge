package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import java.util.Map;

@Schema(description = "Event submission payload")
public record EventSubmitRequest(
    @NotBlank(message = "event_type is required")
    @Schema(description = "Domain event type identifier", example = "order.created")
    String event_type,

    @Schema(description = "Unique client-generated idempotency key to prevent duplicate processing", example = "order-tx-8891-v1")
    String idempotency_key,

    @Schema(description = "Execution scheduling priority: CRITICAL, HIGH, NORMAL, LOW", example = "HIGH", defaultValue = "NORMAL")
    String priority,

    @Min(value = 0, message = "max_retries cannot be negative")
    @Max(value = 20, message = "max_retries cannot exceed 20")
    @Schema(description = "Maximum transient failure retry attempts", example = "3", defaultValue = "3")
    Integer max_retries,

    @NotNull(message = "payload is required")
    @Schema(description = "Structured JSON payload object")
    Map<String, Object> payload,

    @Schema(description = "Configurable exponential retry policy")
    RetryPolicyDto retry_policy
) {
    public record RetryPolicyDto(
        Integer max_retries,
        Integer initial_backoff_ms,
        Integer max_backoff_ms,
        Double backoff_multiplier
    ) {}
}
