package com.relayforge.client;

import com.relayforge.dto.*;
import java.util.Map;

public interface RelayForgeEngineClient {

    IngestResponse submitEvent(EventSubmitRequest request);

    EventResponse getEvent(String eventId);

    Map<String, Object> getDLQ(int limit, int offset);

    DLQReplayResponse replayDLQ(String eventId);

    EngineStatsResponse getStats();

    String getPrometheusMetrics();

    Map<String, Object> checkHealth();
}
