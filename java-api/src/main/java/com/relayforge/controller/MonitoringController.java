package com.relayforge.controller;

import com.relayforge.dto.EngineHealthResponse;
import com.relayforge.dto.EngineStatsResponse;
import com.relayforge.service.EventService;
import io.swagger.v3.oas.annotations.Operation;
import io.swagger.v3.oas.annotations.media.Content;
import io.swagger.v3.oas.annotations.media.Schema;
import io.swagger.v3.oas.annotations.responses.ApiResponse;
import io.swagger.v3.oas.annotations.responses.ApiResponses;
import io.swagger.v3.oas.annotations.tags.Tag;
import org.springframework.http.MediaType;
import org.springframework.http.ProblemDetail;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@Tag(name = "Observability & Health", description = "Operational telemetry, Prometheus metrics exposition, and health verification")
public class MonitoringController {

    private final EventService eventService;

    public MonitoringController(EventService eventService) {
        this.eventService = eventService;
    }

    @GetMapping("/api/v1/stats")
    @Operation(
        summary = "Retrieve engine statistics",
        description = "Provides real-time telemetry including throughput, p50/p95/p99 latency percentiles, queue depth, and worker utilization directly from C++ WAL metrics registry."
    )
    @ApiResponses({
        @ApiResponse(responseCode = "200", description = "Statistics retrieved",
                     content = @Content(schema = @Schema(implementation = EngineStatsResponse.class))),
        @ApiResponse(responseCode = "503", description = "C++ Core engine unavailable",
                     content = @Content(schema = @Schema(implementation = ProblemDetail.class)))
    })
    public ResponseEntity<EngineStatsResponse> getStats() {
        return ResponseEntity.ok(eventService.getEngineStatistics());
    }

    @GetMapping(value = "/api/v1/metrics", produces = "text/plain; version=0.0.4; charset=utf-8")
    @Operation(
        summary = "Prometheus metrics scrape target",
        description = "Exposes Prometheus text format metrics directly from the C++ Prometheus registry."
    )
    @ApiResponses({
        @ApiResponse(responseCode = "200", description = "Prometheus text formatted metrics"),
        @ApiResponse(responseCode = "503", description = "C++ Core engine unavailable")
    })
    public ResponseEntity<String> getMetrics() {
        return ResponseEntity.ok()
            .contentType(MediaType.parseMediaType("text/plain; version=0.0.4; charset=utf-8"))
            .body(eventService.getRawPrometheusMetrics());
    }

    @GetMapping({"/health", "/api/v1/health"})
    @Operation(
        summary = "Hybrid system health check",
        description = "Verifies reachability and operational readiness of both Spring Boot Gateway and C++ WAL Engine."
    )
    @ApiResponse(responseCode = "200", description = "Health status report",
                 content = @Content(schema = @Schema(implementation = EngineHealthResponse.class)))
    public ResponseEntity<EngineHealthResponse> getHealth() {
        return ResponseEntity.ok(eventService.getHealthDiagnostics());
    }
}
