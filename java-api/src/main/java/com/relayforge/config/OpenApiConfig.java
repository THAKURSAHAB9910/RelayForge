package com.relayforge.config;

import io.swagger.v3.oas.annotations.OpenAPIDefinition;
import io.swagger.v3.oas.annotations.info.Contact;
import io.swagger.v3.oas.annotations.info.Info;
import io.swagger.v3.oas.annotations.info.License;
import io.swagger.v3.oas.annotations.servers.Server;
import org.springframework.context.annotation.Configuration;

@Configuration
@OpenAPIDefinition(
    info = @Info(
        title = "RelayForge Application Gateway API",
        version = "1.0.0",
        description = "Java 21 Spring Boot Application-Facing Gateway orchestrating the C++20 Fault-Tolerant Engine",
        contact = @Contact(name = "RelayForge Architecture Team"),
        license = @License(name = "Apache 2.0")
    ),
    servers = {
        @Server(url = "/", description = "Current Gateway Server")
    }
)
public class OpenApiConfig {}
