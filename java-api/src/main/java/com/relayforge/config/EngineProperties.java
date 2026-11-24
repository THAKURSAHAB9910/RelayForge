package com.relayforge.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "relayforge.engine")
public record EngineProperties(
    String baseUrl,
    int connectTimeoutMs,
    int readTimeoutMs,
    int maxConnections,
    int maxConnectionsPerRoute,
    Retry retry
) {
    public record Retry(int maxAttempts, long backoffMs) {}
}
