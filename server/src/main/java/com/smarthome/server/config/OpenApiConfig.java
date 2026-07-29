package com.smarthome.server.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import io.swagger.v3.oas.models.OpenAPI;
import io.swagger.v3.oas.models.info.Info;

/**
 * Minimal springdoc setup: one {@link OpenAPI} bean with title/version and the
 * two API-wide conventions that annotations cannot express well — the
 * {@code ApiResponse} envelope and session-cookie auth. Endpoint documentation
 * comes from what springdoc infers off the controllers; no per-operation
 * annotations (deliberate — keep the controllers clean).
 */
@Configuration
public class OpenApiConfig {

    @Bean
    OpenAPI smartHomeOpenApi() {
        return new OpenAPI().info(new Info()
                .title("Smart Home Server API")
                .version("v1")
                .description("""
                        REST API of the ESP32 smart-home backend.

                        Every response is wrapped in a uniform envelope: \
                        `{"data": ..., "error": null}` on success, \
                        `{"data": null, "error": {"code": ..., "message": ...}}` on failure.

                        Authentication is session-based: `POST /api/v1/auth/login` sets a \
                        session cookie; mutating requests additionally require the CSRF \
                        token from `GET /api/v1/auth/csrf` in the `X-XSRF-TOKEN` header."""));
    }
}
