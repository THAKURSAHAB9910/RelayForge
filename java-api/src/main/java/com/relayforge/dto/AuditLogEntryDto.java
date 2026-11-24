package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;

@Schema(description = "Audit trail state transition record")
public record AuditLogEntryDto(
    String from_status,
    String to_status,
    String transition_reason,
    long timestamp_epoch_ms
) {}
