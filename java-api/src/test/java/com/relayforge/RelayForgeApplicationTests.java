package com.relayforge;

import com.relayforge.client.RelayForgeEngineClient;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.mock.mockito.MockBean;

@SpringBootTest
class RelayForgeApplicationTests {

    @MockBean
    private RelayForgeEngineClient engineClient;

    @Test
    @DisplayName("Verify Spring Boot context boots cleanly with configurations")
    void contextLoads() {
        // Confirms that all beans, controllers, services, configs, and OpenAPI metadata initialize properly
    }
}
