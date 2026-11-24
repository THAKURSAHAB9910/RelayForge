package com.relayforge.controller;

import com.relayforge.dto.DLQReplayResponse;
import com.relayforge.service.EventService;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.Parameter;
import io.swagger.v3.oas.annotations.media.Content;
import io.swagger.v3.oas.annotations.media.Schema;
import io.swagger.v3.oas.annotations.responses.ApiResponse;
import io.swagger.v3.oas.annotations.responses.ApiResponses;
import io.swagger.v3.oas.annotations.tags.Tag;
import org.springframework.http.ProblemDetail;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/api/v1/dlq")
@Tag(name = "Dead Letter Queue (DLQ)", description = "Operations for inspecting poisoned events and initiating replays")
public class DLQController {

    private final EventService eventService;

    public DLQController(EventService eventService) {
        this.eventService = eventService;
    }

    @GetMapping
    @Operation(
        summary = "List Dead Letter Queue events",
        description = "Retrieves events that exhausted all retry attempts and were permanently quarantined in the C++ DLQ."
    )
    @ApiResponses({
        @ApiResponse(responseCode = "200", description = "List of quarantined events successfully retrieved"),
        @ApiResponse(responseCode = "503", description = "C++ Core engine unavailable",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class)))
    })
    public ResponseEntity<Map<String, Object>> getDLQ(
        @Parameter(description = "Maximum events to fetch (pagination)", example = "50")
        @RequestParam(defaultValue = "50") int limit,
        @Parameter(description = "Pagination offset", example = "0")
        @RequestParam(defaultValue = "0") int offset
    ) {
        Map<String, Object> dlqData = eventService.listDLQEvents(limit, offset);
        return ResponseEntity.ok(dlqData);
    }

    @PostMapping("/{id}/replay")
    @Operation(
        summary = "Replay a Dead Letter Queue event",
        description = "Resets the attempt counter and re-enqueues the poisoned event back into the C++ priority scheduler."
    )
    @ApiResponses({
        @ApiResponse(responseCode = "200", description = "Event re-enqueued for execution",
                     content = @Content(schema = @Schema(implementation = DLQReplayResponse.class))),
        @ApiResponse(responseCode = "404", description = "Event not found in DLQ",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class))),
        @ApiResponse(responseCode = "503", description = "C++ Core engine unavailable",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class)))
    })
    public ResponseEntity<DLQReplayResponse> replayDLQ(
        @Parameter(description = "Canonical UUID of the event in DLQ", example = "f47ac10b-58cc-4372-a567-0e02b2c3d479")
        @PathVariable("id") String eventId
    ) {
        DLQReplayResponse response = eventService.replayDLQEvent(eventId);
        return ResponseEntity.ok(response);
    }
}
