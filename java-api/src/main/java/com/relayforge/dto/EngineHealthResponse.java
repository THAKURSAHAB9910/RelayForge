package com.relayforge.dto;

import io.swagger.v3.oas.annotations.media.Schema;
import java.util.Map;

@Schema(description = "RelayForge Hybrid Health Diagnostics")
public record EngineHealthResponse(
    @Schema(description = "Spring Boot Gateway Status", example = "UP")
    String gateway,

    @Schema(description = "C++ Core Engine Status", example = "UP")
    String backend_core,

    @Schema(description = "Configured C++ Engine Remote Endpoint", example = "http://localhost:8080")
    String backend_url,

    @Schema(description = "Engine diagnostic details")
    Map<String, Object> details
) {}
