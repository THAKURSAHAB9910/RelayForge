package com.relayforge.client;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.relayforge.config.EngineProperties;
import com.relayforge.dto.DLQReplayResponse;
import com.relayforge.dto.EventResponse;
import com.relayforge.dto.EventSubmitRequest;
import com.relayforge.dto.IngestResponse;
import com.relayforge.exception.EngineConnectionException;
import com.relayforge.exception.EngineOverloadedException;
import com.relayforge.exception.EngineTimeoutException;
import com.relayforge.exception.ResourceNotFoundException;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.http.HttpMethod;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.test.web.client.MockRestServiceServer;
import org.springframework.web.client.ResourceAccessException;
import org.springframework.web.client.RestTemplate;

import java.net.SocketTimeoutException;
import java.util.List;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.*;
import static org.springframework.test.web.client.match.MockRestRequestMatchers.*;
import static org.springframework.test.web.client.response.MockRestResponseCreators.*;

class RelayForgeEngineHttpClientTest {

    private RestTemplate restTemplate;
    private MockRestServiceServer mockServer;
    private RelayForgeEngineHttpClient client;
    private final ObjectMapper objectMapper = new ObjectMapper();

    private final EngineProperties testProperties = new EngineProperties(
        "http://localhost:8080", 3000, 5000, 100, 30, new EngineProperties.Retry(3, 500)
    );

    @BeforeEach
    void setUp() {
        restTemplate = new RestTemplate();
        mockServer = MockRestServiceServer.createServer(restTemplate);
        client = new RelayForgeEngineHttpClient(restTemplate, testProperties);
    }

    @Test
    @DisplayName("submitEvent - Successfully sends POST /events and maps IngestResponse")
    void submitEvent_Success() throws Exception {
        EventSubmitRequest request = new EventSubmitRequest(
            "ORDER_CREATED",
            "idemp-101",
            "HIGH",
            3,
            Map.of("item", "laptop"),
            null
        );

        IngestResponse mockResponse = new IngestResponse(
            0,
            "evt-101",
            202,
            "Persisted to WAL",
            Map.of("id", "evt-101", "event_type", "ORDER_CREATED")
        );

        mockServer.expect(requestTo("http://localhost:8080/events"))
            .andExpect(method(HttpMethod.POST))
            .andExpect(content().contentType(MediaType.APPLICATION_JSON))
            .andRespond(withStatus(HttpStatus.ACCEPTED)
                .contentType(MediaType.APPLICATION_JSON)
                .body(objectMapper.writeValueAsString(mockResponse)));

        IngestResponse result = client.submitEvent(request);
        assertNotNull(result);
        assertEquals(0, result.status());
        assertEquals("evt-101", result.event_id());
        mockServer.verify();
    }

    @Test
    @DisplayName("submitEvent - Throws EngineOverloadedException on HTTP 429 backpressure")
    void submitEvent_Backpressure_ThrowsEngineOverloaded() {
        EventSubmitRequest request = new EventSubmitRequest(
            "SPIKE_TEST",
            "idemp-spike",
            "LOW",
            1,
            Map.of(),
            null
        );

        mockServer.expect(requestTo("http://localhost:8080/events"))
            .andExpect(method(HttpMethod.POST))
            .andRespond(withStatus(HttpStatus.TOO_MANY_REQUESTS)
                .body("Queue capacity reached"));

        assertThrows(EngineOverloadedException.class, () -> client.submitEvent(request));
        mockServer.verify();
    }

    @Test
    @DisplayName("submitEvent - Throws EngineTimeoutException on SocketTimeoutException")
    void submitEvent_Timeout_ThrowsEngineTimeout() {
        EventSubmitRequest request = new EventSubmitRequest("TASK", "idemp-t", "NORMAL", 3, Map.of(), null);

        RestTemplate failingRestTemplate = new RestTemplate() {
            @Override
            public <T> org.springframework.http.ResponseEntity<T> postForEntity(String url, Object req, Class<T> responseType, Object... uriVariables) {
                throw new ResourceAccessException("Read timed out", new SocketTimeoutException("Read timed out"));
            }
        };

        RelayForgeEngineHttpClient timeoutClient = new RelayForgeEngineHttpClient(failingRestTemplate, testProperties);

        assertThrows(EngineTimeoutException.class, () -> timeoutClient.submitEvent(request));
    }

    @Test
    @DisplayName("submitEvent - Throws EngineConnectionException when C++ engine connection is refused")
    void submitEvent_ConnectionRefused_ThrowsEngineConnectionException() {
        EventSubmitRequest request = new EventSubmitRequest("TASK", "idemp-c", "NORMAL", 3, Map.of(), null);

        RestTemplate failingRestTemplate = new RestTemplate() {
            @Override
            public <T> org.springframework.http.ResponseEntity<T> postForEntity(String url, Object req, Class<T> responseType, Object... uriVariables) {
                throw new ResourceAccessException("Connection refused: connect");
            }
        };

        RelayForgeEngineHttpClient connectionClient = new RelayForgeEngineHttpClient(failingRestTemplate, testProperties);

        assertThrows(EngineConnectionException.class, () -> connectionClient.submitEvent(request));
    }

    @Test
    @DisplayName("getEvent - Successfully retrieves existing event")
    void getEvent_Success() throws Exception {
        String eventId = "evt-404-ok";
        EventResponse expected = new EventResponse(
            eventId,
            "idemp-ok",
            "ORDER_PLACED",
            "HIGH",
            "COMPLETED",
            1,
            3,
            1732492800000L,
            1732492801000L,
            1732492802000L,
            null,
            "Success",
            Map.of(),
            List.of()
        );

        mockServer.expect(requestTo("http://localhost:8080/events/" + eventId))
            .andExpect(method(HttpMethod.GET))
            .andRespond(withSuccess(objectMapper.writeValueAsString(expected), MediaType.APPLICATION_JSON));

        EventResponse result = client.getEvent(eventId);
        assertNotNull(result);
        assertEquals(eventId, result.id());
        assertEquals("COMPLETED", result.status());
        mockServer.verify();
    }

    @Test
    @DisplayName("getEvent - Throws ResourceNotFoundException on HTTP 404")
    void getEvent_NotFound_ThrowsResourceNotFoundException() {
        String missingId = "evt-ghost";

        mockServer.expect(requestTo("http://localhost:8080/events/" + missingId))
            .andExpect(method(HttpMethod.GET))
            .andRespond(withStatus(HttpStatus.NOT_FOUND));

        assertThrows(ResourceNotFoundException.class, () -> client.getEvent(missingId));
        mockServer.verify();
    }

    @Test
    @DisplayName("replayDLQ - Successfully posts to /dlq/{id}/replay and returns DLQReplayResponse")
    void replayDLQ_Success() throws Exception {
        String eventId = "evt-poison-1";
        DLQReplayResponse expected = new DLQReplayResponse("REPLAY_QUEUED", eventId, "Replay enqueued");

        mockServer.expect(requestTo("http://localhost:8080/dlq/" + eventId + "/replay"))
            .andExpect(method(HttpMethod.POST))
            .andRespond(withSuccess(objectMapper.writeValueAsString(expected), MediaType.APPLICATION_JSON));

        DLQReplayResponse result = client.replayDLQ(eventId);
        assertNotNull(result);
        assertEquals("REPLAY_QUEUED", result.status());
        assertEquals(eventId, result.event_id());
        mockServer.verify();
    }

    @Test
    @DisplayName("checkHealth - Returns UNREACHABLE status when C++ core probe fails")
    void checkHealth_CoreDown_ReturnsUnreachable() {
        RestTemplate failingRestTemplate = new RestTemplate() {
            @Override
            public <T> org.springframework.http.ResponseEntity<T> exchange(
                String url, HttpMethod method, org.springframework.http.HttpEntity<?> requestEntity,
                org.springframework.core.ParameterizedTypeReference<T> responseType, Object... uriVariables
            ) {
                throw new ResourceAccessException("Connection refused");
            }
        };

        RelayForgeEngineHttpClient clientWithFailingCore = new RelayForgeEngineHttpClient(failingRestTemplate, testProperties);

        Map<String, Object> health = clientWithFailingCore.checkHealth();
        assertNotNull(health);
        assertEquals("UNREACHABLE", health.get("status"));
    }
}
