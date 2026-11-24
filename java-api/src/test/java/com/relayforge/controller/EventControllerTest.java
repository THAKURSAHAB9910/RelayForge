package com.relayforge.controller;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.relayforge.dto.AuditLogEntryDto;
import com.relayforge.dto.EventResponse;
import com.relayforge.dto.EventSubmitRequest;
import com.relayforge.dto.IngestResponse;
import com.relayforge.exception.ResourceNotFoundException;
import com.relayforge.service.EventService;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;

import java.util.List;
import java.util.Map;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(EventController.class)
class EventControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private ObjectMapper objectMapper;

    @MockBean
    private EventService eventService;

    @Test
    @DisplayName("POST /api/v1/events - Returns 202 Accepted for new valid event")
    void submitEvent_ReturnsAccepted() throws Exception {
        EventSubmitRequest request = new EventSubmitRequest(
            "ORDER_CREATED",
            "idemp-order-101",
            "CRITICAL",
            5,
            Map.of("order_id", "ord-101", "amount", 199.99),
            null
        );

        IngestResponse response = new IngestResponse(
            0,
            "evt-uuid-101",
            202,
            "Event persisted to WAL and scheduled",
            Map.of("id", "evt-uuid-101", "idempotency_key", "idemp-order-101", "event_type", "ORDER_CREATED", "priority", "CRITICAL")
        );

        when(eventService.processEventSubmission(any(EventSubmitRequest.class))).thenReturn(response);

        mockMvc.perform(post("/api/v1/events")
                .contentType(MediaType.APPLICATION_JSON)
                .content(objectMapper.writeValueAsString(request)))
            .andExpect(status().isAccepted())
            .andExpect(jsonPath("$.status").value(0))
            .andExpect(jsonPath("$.event_id").value("evt-uuid-101"))
            .andExpect(jsonPath("$.http_status_code").value(202))
            .andExpect(jsonPath("$.event.event_type").value("ORDER_CREATED"))
            .andExpect(jsonPath("$.event.priority").value("CRITICAL"));
    }

    @Test
    @DisplayName("POST /api/v1/events - Returns 200 OK for duplicate idempotency key")
    void submitEvent_DuplicateKey_ReturnsOk() throws Exception {
        EventSubmitRequest request = new EventSubmitRequest(
            "PAYMENT_PROCESSED",
            "idemp-pay-202",
            "NORMAL",
            3,
            Map.of("tx_id", "tx-999"),
            null
        );

        IngestResponse duplicateResponse = new IngestResponse(
            0,
            "evt-existing-202",
            200,
            "Event already accepted and logged",
            Map.of("id", "evt-existing-202", "idempotency_key", "idemp-pay-202")
        );

        when(eventService.processEventSubmission(any(EventSubmitRequest.class))).thenReturn(duplicateResponse);

        mockMvc.perform(post("/api/v1/events")
                .contentType(MediaType.APPLICATION_JSON)
                .content(objectMapper.writeValueAsString(request)))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.http_status_code").value(200))
            .andExpect(jsonPath("$.event_id").value("evt-existing-202"));
    }

    @Test
    @DisplayName("POST /api/v1/events - Returns 400 Bad Request when event_type is missing")
    void submitEvent_MissingEventType_ReturnsBadRequest() throws Exception {
        String invalidJson = """
            {
                "event_type": "",
                "payload": {"data": "test"}
            }
            """;

        mockMvc.perform(post("/api/v1/events")
                .contentType(MediaType.APPLICATION_JSON)
                .content(invalidJson))
            .andExpect(status().isBadRequest())
            .andExpect(jsonPath("$.title").value("Validation Error"))
            .andExpect(jsonPath("$.invalid_fields.event_type").exists());
    }

    @Test
    @DisplayName("POST /api/v1/events - Returns 400 Bad Request when max_retries is negative")
    void submitEvent_NegativeRetries_ReturnsBadRequest() throws Exception {
        String invalidJson = """
            {
                "event_type": "INVENTORY_SYNC",
                "max_retries": -5,
                "payload": {}
            }
            """;

        mockMvc.perform(post("/api/v1/events")
                .contentType(MediaType.APPLICATION_JSON)
                .content(invalidJson))
            .andExpect(status().isBadRequest())
            .andExpect(jsonPath("$.title").value("Validation Error"))
            .andExpect(jsonPath("$.invalid_fields.max_retries").exists());
    }

    @Test
    @DisplayName("GET /api/v1/events/{id} - Returns 200 OK when event exists")
    void getEvent_Found_ReturnsOk() throws Exception {
        String eventId = "evt-valid-303";
        EventResponse eventResponse = new EventResponse(
            eventId,
            "idemp-303",
            "NOTIFICATION_EMAIL",
            "HIGH",
            "COMPLETED",
            1,
            3,
            1732492800000L,
            1732492801000L,
            1732492802000L,
            null,
            "Success: notification dispatched",
            Map.of("user", "alice"),
            List.of()
        );

        when(eventService.getEventById(eq(eventId))).thenReturn(eventResponse);

        mockMvc.perform(get("/api/v1/events/{id}", eventId))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.id").value(eventId))
            .andExpect(jsonPath("$.status").value("COMPLETED"))
            .andExpect(jsonPath("$.priority").value("HIGH"))
            .andExpect(jsonPath("$.attempt_count").value(1));
    }

    @Test
    @DisplayName("GET /api/v1/events/{id} - Returns 404 Not Found when event missing")
    void getEvent_NotFound_Returns404() throws Exception {
        String missingId = "evt-non-existent";
        when(eventService.getEventById(eq(missingId)))
            .thenThrow(new ResourceNotFoundException("Event not found with ID: " + missingId));

        mockMvc.perform(get("/api/v1/events/{id}", missingId))
            .andExpect(status().isNotFound())
            .andExpect(jsonPath("$.title").value("Resource Not Found"))
            .andExpect(jsonPath("$.detail").value("Event not found with ID: " + missingId));
    }
}
