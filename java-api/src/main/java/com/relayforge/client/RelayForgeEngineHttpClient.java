package com.relayforge.client;

import com.relayforge.config.EngineProperties;
import com.relayforge.dto.*;
import com.relayforge.exception.EngineConnectionException;
import com.relayforge.exception.EngineOverloadedException;
import com.relayforge.exception.EngineTimeoutException;
import com.relayforge.exception.ResourceNotFoundException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.ParameterizedTypeReference;
import org.springframework.http.*;
import org.springframework.stereotype.Component;
import org.springframework.web.client.HttpClientErrorException;
import org.springframework.web.client.HttpServerErrorException;
import org.springframework.web.client.ResourceAccessException;
import org.springframework.web.client.RestTemplate;

import java.net.SocketTimeoutException;
import java.util.HashMap;
import java.util.Map;

@Component
public class RelayForgeEngineHttpClient implements RelayForgeEngineClient {

    private static final Logger log = LoggerFactory.getLogger(RelayForgeEngineHttpClient.class);

    private final RestTemplate restTemplate;
    private final EngineProperties properties;

    public RelayForgeEngineHttpClient(RestTemplate engineRestTemplate, EngineProperties properties) {
        this.restTemplate = engineRestTemplate;
        this.properties = properties;
    }

    private String url(String path) {
        String base = properties.baseUrl();
        if (base.endsWith("/")) {
            base = base.substring(0, base.length() - 1);
        }
        return base + path;
    }

    @Override
    public IngestResponse submitEvent(EventSubmitRequest request) {
        String targetUrl = url("/events");
        log.debug("Submitting event to C++ engine at {}: type={}, idemp={}", 
                  targetUrl, request.event_type(), request.idempotency_key());

        HttpHeaders headers = new HttpHeaders();
        headers.setContentType(MediaType.APPLICATION_JSON);
        HttpEntity<EventSubmitRequest> entity = new HttpEntity<>(request, headers);

        try {
            ResponseEntity<IngestResponse> response = restTemplate.postForEntity(
                targetUrl, 
                entity, 
                IngestResponse.class
            );
            return response.getBody();
        } catch (HttpClientErrorException.TooManyRequests e) {
            log.warn("C++ Engine signaled backpressure / rate limiting: {}", e.getMessage());
            throw new EngineOverloadedException("C++ Engine queue depth capacity or rate limit reached");
        } catch (ResourceAccessException e) {
            handleResourceAccessException(e, "submitEvent");
            return null; // unreachable
        } catch (Exception e) {
            log.error("Failed to submit event to C++ Engine: {}", e.getMessage(), e);
            throw new EngineConnectionException("Communication error with C++ engine: " + e.getMessage(), e);
        }
    }

    @Override
    public EventResponse getEvent(String eventId) {
        String targetUrl = url("/events/" + eventId);
        try {
            ResponseEntity<EventResponse> response = restTemplate.getForEntity(targetUrl, EventResponse.class);
            return response.getBody();
        } catch (HttpClientErrorException.NotFound e) {
            throw new ResourceNotFoundException("Event not found with ID: " + eventId);
        } catch (ResourceAccessException e) {
            handleResourceAccessException(e, "getEvent");
            return null;
        } catch (Exception e) {
            throw new EngineConnectionException("Failed to retrieve event from C++ engine: " + e.getMessage(), e);
        }
    }

    @Override
    public Map<String, Object> getDLQ(int limit, int offset) {
        String targetUrl = url(String.format("/dlq?limit=%d&offset=%d", limit, offset));
        try {
            ResponseEntity<Map<String, Object>> response = restTemplate.exchange(
                targetUrl,
                HttpMethod.GET,
                null,
                new ParameterizedTypeReference<Map<String, Object>>() {}
            );
            return response.getBody();
        } catch (ResourceAccessException e) {
            handleResourceAccessException(e, "getDLQ");
            return Map.of("events", java.util.List.of(), "total_count", 0);
        } catch (Exception e) {
            throw new EngineConnectionException("Failed to query DLQ from C++ engine: " + e.getMessage(), e);
        }
    }

    @Override
    public DLQReplayResponse replayDLQ(String eventId) {
        String targetUrl = url("/dlq/" + eventId + "/replay");
        try {
            ResponseEntity<DLQReplayResponse> response = restTemplate.postForEntity(
                targetUrl,
                null,
                DLQReplayResponse.class
            );
            return response.getBody();
        } catch (HttpClientErrorException.NotFound e) {
            throw new ResourceNotFoundException("Event not found in DEAD_LETTER status: " + eventId);
        } catch (ResourceAccessException e) {
            handleResourceAccessException(e, "replayDLQ");
            return null;
        } catch (Exception e) {
            throw new EngineConnectionException("Failed to replay DLQ event on C++ engine: " + e.getMessage(), e);
        }
    }

    @Override
    public EngineStatsResponse getStats() {
        String targetUrl = url("/stats");
        try {
            ResponseEntity<EngineStatsResponse> response = restTemplate.getForEntity(targetUrl, EngineStatsResponse.class);
            return response.getBody();
        } catch (ResourceAccessException e) {
            handleResourceAccessException(e, "getStats");
            return null;
        } catch (Exception e) {
            throw new EngineConnectionException("Failed to retrieve metrics from C++ engine: " + e.getMessage(), e);
        }
    }

    @Override
    public String getPrometheusMetrics() {
        String targetUrl = url("/metrics");
        try {
            ResponseEntity<String> response = restTemplate.getForEntity(targetUrl, String.class);
            return response.getBody();
        } catch (ResourceAccessException e) {
            handleResourceAccessException(e, "getPrometheusMetrics");
            return "";
        } catch (Exception e) {
            throw new EngineConnectionException("Failed to query Prometheus metrics from C++ engine", e);
        }
    }

    @Override
    public Map<String, Object> checkHealth() {
        String targetUrl = url("/health");
        Map<String, Object> result = new HashMap<>();
        try {
            ResponseEntity<Map<String, Object>> response = restTemplate.exchange(
                targetUrl,
                HttpMethod.GET,
                null,
                new ParameterizedTypeReference<Map<String, Object>>() {}
            );
            if (response.getStatusCode().is2xxSuccessful() && response.getBody() != null) {
                result.put("status", "UP");
                result.putAll(response.getBody());
            } else {
                result.put("status", "DEGRADED");
            }
        } catch (Exception e) {
            log.warn("Health probe to C++ engine failed: {}", e.getMessage());
            result.put("status", "UNREACHABLE");
            result.put("error", e.getMessage());
        }
        return result;
    }

    private void handleResourceAccessException(ResourceAccessException e, String operation) {
        log.error("ResourceAccessException during {}: {}", operation, e.getMessage());
        if (e.getCause() instanceof SocketTimeoutException) {
            throw new EngineTimeoutException("Timed out waiting for response from C++ Engine during " + operation, e);
        }
        throw new EngineConnectionException("Failed to connect to C++ Engine at " + properties.baseUrl() + ": " + e.getMessage(), e);
    }
}
