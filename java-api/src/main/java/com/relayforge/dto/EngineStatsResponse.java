package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import java.util.Map;

@Schema(description = "RelayForge Runtime Engine Telemetry Statistics")
public record EngineStatsResponse(
    long events_received_total,
    long events_success_total,
    long events_failed_total,
    long events_retried_total,
    long events_dlq_total,
    long in_flight_count,
    long queue_depth,
    long backpressure_drops_total,
    double throughput_eps,
    double worker_utilization,
    Map<String, Double> latency_ms,
    Map<String, Object> recovery
) {}
