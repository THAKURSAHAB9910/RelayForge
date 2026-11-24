package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import java.util.Map;

@Schema(description = "Event Ingestion Acknowledgement")
public record IngestResponse(
    @Schema(description = "Status code (0 = success, non-zero = error)", example = "0")
    int status,

    @Schema(description = "Canonical RFC 4122 UUID assigned to the event", example = "f47ac10b-58cc-4372-a567-0e02b2c3d479")
    String event_id,

    @Schema(description = "HTTP status code (202 = Accepted, 200 = Deduplicated)", example = "202")
    int http_status_code,

    @Schema(description = "Informational status message", example = "Event accepted and persisted to WAL")
    String message,

    @Schema(description = "Persisted event snapshot")
    Map<String, Object> event
) {}
