package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import java.util.List;
import java.util.Map;

@Schema(description = "Event State & Lifecycle Query Details")
public record EventResponse(
    String id,
    String idempotency_key,
    String event_type,
    String priority,
    String status,
    int attempt_count,
    int max_retries,
    long created_at_epoch_ms,
    long scheduled_at_epoch_ms,
    long completed_at_epoch_ms,
    String last_error,
    String execution_result,
    Object payload,
    List<AuditLogEntryDto> audit_trail
) {}
