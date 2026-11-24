package com.relayforge.controller;

import com.relayforge.dto.EngineHealthResponse;
import com.relayforge.dto.EngineStatsResponse;
import com.relayforge.service.EventService;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;

import java.util.Map;

import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@WebMvcTest(MonitoringController.class)
class MonitoringControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private EventService eventService;

    @Test
    @DisplayName("GET /api/v1/stats - Returns 200 OK with runtime engine metrics")
    void getStats_ReturnsOk() throws Exception {
        EngineStatsResponse stats = new EngineStatsResponse(
            1500L,
            1450L,
            30L,
            120L,
            20L,
            0L,
            0L,
            0L,
            2540.5,
            0.85,
            Map.of("p50", 0.05, "p95", 0.12, "p99", 0.45),
            Map.of("recovered_events", 0, "recovery_duration_ms", 1.2)
        );

        when(eventService.getEngineStatistics()).thenReturn(stats);

        mockMvc.perform(get("/api/v1/stats"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.events_received_total").value(1500))
            .andExpect(jsonPath("$.events_success_total").value(1450))
            .andExpect(jsonPath("$.throughput_eps").value(2540.5))
            .andExpect(jsonPath("$.latency_ms.p50").value(0.05));
    }

    @Test
    @DisplayName("GET /api/v1/metrics - Returns Prometheus text formatted output")
    void getMetrics_ReturnsPrometheusText() throws Exception {
        String prometheusText = """
            # HELP relayforge_events_received_total Total events submitted to RelayForge
            # TYPE relayforge_events_received_total counter
            relayforge_events_received_total 1500
            # HELP relayforge_events_success_total Events processed successfully
            # TYPE relayforge_events_success_total counter
            relayforge_events_success_total 1450
            """;

        when(eventService.getRawPrometheusMetrics()).thenReturn(prometheusText);

        mockMvc.perform(get("/api/v1/metrics"))
            .andExpect(status().isOk())
            .andExpect(content().contentType("text/plain; version=0.0.4; charset=utf-8"))
            .andExpect(content().string(org.hamcrest.Matchers.containsString("relayforge_events_received_total 1500")));
    }

    @Test
    @DisplayName("GET /health - Returns 200 OK with hybrid health diagnostics")
    void getHealth_ReturnsHealthReport() throws Exception {
        EngineHealthResponse health = new EngineHealthResponse(
            "UP",
            "UP",
            "http://localhost:8080",
            Map.of("wal_mode", "wal", "workers", 8)
        );

        when(eventService.getHealthDiagnostics()).thenReturn(health);

        mockMvc.perform(get("/health"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.gateway").value("UP"))
            .andExpect(jsonPath("$.backend_core").value("UP"))
            .andExpect(jsonPath("$.backend_url").value("http://localhost:8080"));
    }
}
