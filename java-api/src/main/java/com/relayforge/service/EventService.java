package com.relayforge.service;

import com.relayforge.client.RelayForgeEngineClient;
import com.relayforge.dto.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.util.Map;
import java.util.UUID;

@Service
public class EventService {

    private static final Logger log = LoggerFactory.getLogger(EventService.class);

    private final RelayForgeEngineClient engineClient;

    public EventService(RelayForgeEngineClient engineClient) {
        this.engineClient = engineClient;
    }

    public IngestResponse processEventSubmission(EventSubmitRequest request) {
        // Ensure idempotency key exists if omitted by caller
        String effectiveIdempotencyKey = request.idempotency_key();
        if (effectiveIdempotencyKey == null || effectiveIdempotencyKey.isBlank()) {
            effectiveIdempotencyKey = "auto-" + UUID.randomUUID();
        }

        EventSubmitRequest normalizedRequest = new EventSubmitRequest(
            request.event_type(),
            effectiveIdempotencyKey,
            request.priority() != null ? request.priority().toUpperCase() : "NORMAL",
            request.max_retries() != null ? request.max_retries() : 3,
            request.payload(),
            request.retry_policy()
        );

        log.info("Submitting event to C++ WAL engine: type={}, idempKey={}, priority={}",
                 normalizedRequest.event_type(), normalizedRequest.idempotency_key(), normalizedRequest.priority());

        return engineClient.submitEvent(normalizedRequest);
    }

    public EventResponse getEventById(String eventId) {
        log.debug("Fetching event state for: {}", eventId);
        return engineClient.getEvent(eventId);
    }

    public Map<String, Object> listDLQEvents(int limit, int offset) {
        log.debug("Listing DLQ events: limit={}, offset={}", limit, offset);
        return engineClient.getDLQ(limit, offset);
    }

    public DLQReplayResponse replayDLQEvent(String eventId) {
        log.info("Requesting DLQ replay from C++ engine for event: {}", eventId);
        return engineClient.replayDLQ(eventId);
    }

    public EngineStatsResponse getEngineStatistics() {
        return engineClient.getStats();
    }

    public String getRawPrometheusMetrics() {
        return engineClient.getPrometheusMetrics();
    }

    public EngineHealthResponse getHealthDiagnostics() {
        Map<String, Object> coreHealth = engineClient.checkHealth();
        String coreStatus = (String) coreHealth.getOrDefault("status", "DOWN");

        return new EngineHealthResponse(
            "UP",
            coreStatus,
            "Engine Client Active",
            coreHealth
        );
    }
}
