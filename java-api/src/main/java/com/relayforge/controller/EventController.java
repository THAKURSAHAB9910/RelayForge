package com.relayforge.controller;

import com.relayforge.dto.EventResponse;
import com.relayforge.dto.EventSubmitRequest;
import com.relayforge.dto.IngestResponse;
import com.relayforge.service.EventService;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.Parameter;
import io.swagger.v3.oas.annotations.media.Content;
import io.swagger.v3.oas.annotations.media.Schema;
import io.swagger.v3.oas.annotations.responses.ApiResponse;
import io.swagger.v3.oas.annotations.responses.ApiResponses;
import io.swagger.v3.oas.annotations.tags.Tag;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/events")
@Tag(name = "Event Ingestion & Lifecycle", description = "Endpoints for durable event ingestion, deduplication, and status queries")
public class EventController {

    private final EventService eventService;

    public EventController(EventService eventService) {
        this.eventService = eventService;
    }

    @PostMapping
    @Operation(
        summary = "Submit an event for durable processing",
        description = "Validates event payload, enforces idempotency, and commits event to the C++ WAL engine prior to returning acknowledgement."
    )
    @ApiResponses({
        @ApiResponse(responseCode = "202", description = "Event accepted and durably persisted to SQLite WAL",
                     content = @Content(schema = @Schema(implementation = IngestResponse.class))),
        @ApiResponse(responseCode = "200", description = "Duplicate request deduplicated via idempotency key",
                     content = @Content(schema = @Schema(implementation = IngestResponse.class))),
        @ApiResponse(responseCode = "400", description = "Payload validation failure",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class))),
        @ApiResponse(responseCode = "429", description = "Backpressure queue capacity exceeded",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class))),
        @ApiResponse(responseCode = "503", description = "C++ Core engine unavailable",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class)))
    })
    public ResponseEntity<IngestResponse> submitEvent(@Valid @RequestBody EventSubmitRequest request) {
        IngestResponse response = eventService.processEventSubmission(request);
        HttpStatus status = HttpStatus.resolve(response.http_status_code());
        if (status == null) {
            status = HttpStatus.ACCEPTED;
        }
        return ResponseEntity.status(status).body(response);
    }

    @GetMapping("/{id}")
    @Operation(summary = "Retrieve event status by UUID", description = "Queries current processing status, attempt count, last error, and execution results from C++ WAL.")
    @ApiResponses({
        @ApiResponse(responseCode = "200", description = "Event found",
                     content = @Content(schema = @Schema(implementation = EventResponse.class))),
        @ApiResponse(responseCode = "404", description = "Event ID not found",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class))),
        @ApiResponse(responseCode = "503", description = "C++ Core engine unavailable")
    })
    public ResponseEntity<EventResponse> getEvent(
        @Parameter(description = "Canonical UUID of the event", example = "f47ac10b-58cc-4372-a567-0e02b2c3d479")
        @PathVariable("id") String eventId
    ) {
        EventResponse event = eventService.getEventById(eventId);
        return ResponseEntity.ok(event);
    }
}
