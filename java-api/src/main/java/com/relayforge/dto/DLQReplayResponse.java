package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;

@Schema(description = "Dead Letter Queue Replay Acknowledgement")
public record DLQReplayResponse(
    @Schema(description = "Replay execution status", example = "REPLAY_QUEUED")
    String status,

    @Schema(description = "Target event identifier", example = "f47ac10b-58cc-4372-a567-0e02b2c3d479")
    String event_id,

    @Schema(description = "Replay confirmation message", example = "Event reset to PENDING and re-enqueued to scheduler")
    String message
) {}
