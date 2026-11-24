package com.relayforge.controller;

import com.relayforge.dto.DLQReplayResponse;
import com.relayforge.exception.ResourceNotFoundException;
import com.relayforge.service.EventService;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.test.web.servlet.MockMvc;

import java.util.List;
import java.util.Map;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(DLQController.class)
class DLQControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private EventService eventService;

    @Test
    @DisplayName("GET /api/v1/dlq - Returns 200 OK with quarantined events list")
    void getDLQ_ReturnsList() throws Exception {
        Map<String, Object> dlqData = Map.of(
            "total_count", 1,
            "events", List.of(
                Map.of(
                    "id", "evt-dlq-1",
                    "event_type", "EXTERNAL_WEBHOOK",
                    "status", "DEAD_LETTER",
                    "attempts", 3,
                    "max_retries", 3,
                    "last_error", "HTTP 500 downstream failure"
                )
            )
        );

        when(eventService.listDLQEvents(50, 0)).thenReturn(dlqData);

        mockMvc.perform(get("/api/v1/dlq?limit=50&offset=0"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.total_count").value(1))
            .andExpect(jsonPath("$.events[0].id").value("evt-dlq-1"))
            .andExpect(jsonPath("$.events[0].status").value("DEAD_LETTER"));
    }

    @Test
    @DisplayName("POST /api/v1/dlq/{id}/replay - Returns 200 OK upon successful re-enqueue")
    void replayDLQ_Success_ReturnsOk() throws Exception {
        String eventId = "evt-dlq-1";
        DLQReplayResponse replayResponse = new DLQReplayResponse(
            "REPLAY_QUEUED",
            eventId,
            "Event reset to PENDING and re-enqueued to scheduler"
        );

        when(eventService.replayDLQEvent(eq(eventId))).thenReturn(replayResponse);

        mockMvc.perform(post("/api/v1/dlq/{id}/replay", eventId))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.status").value("REPLAY_QUEUED"))
            .andExpect(jsonPath("$.event_id").value(eventId));
    }

    @Test
    @DisplayName("POST /api/v1/dlq/{id}/replay - Returns 404 when target not in DLQ")
    void replayDLQ_NotFound_Returns404() throws Exception {
        String missingId = "evt-not-in-dlq";
        when(eventService.replayDLQEvent(eq(missingId)))
            .thenThrow(new ResourceNotFoundException("Event not found in DEAD_LETTER status: " + missingId));

        mockMvc.perform(post("/api/v1/dlq/{id}/replay", missingId))
            .andExpect(status().isNotFound())
            .andExpect(jsonPath("$.title").value("Resource Not Found"))
            .andExpect(jsonPath("$.detail").value("Event not found in DEAD_LETTER status: " + missingId));
    }
}
