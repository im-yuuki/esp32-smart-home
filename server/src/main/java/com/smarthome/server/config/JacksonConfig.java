package com.smarthome.server.config;

import org.springframework.boot.jackson.autoconfigure.JsonMapperBuilderCustomizer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import tools.jackson.databind.DeserializationFeature;

/**
 * Jackson 3 customization for the auto-configured {@code tools.jackson.databind.json.JsonMapper}
 * bean. Unknown properties are tolerated so firmware payloads can evolve (new discovery
 * fields) without breaking the server.
 */
@Configuration
public class JacksonConfig {

    @Bean
    public JsonMapperBuilderCustomizer smartHomeJsonMapperCustomizer() {
        return builder -> builder.disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES);
    }
}
