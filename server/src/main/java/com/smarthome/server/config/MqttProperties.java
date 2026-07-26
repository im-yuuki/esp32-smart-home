package com.smarthome.server.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * MQTT connection settings, env-driven via application.yml ({@code smarthome.mqtt.*}).
 */
@ConfigurationProperties(prefix = "smarthome.mqtt")
public record MqttProperties(String uri, String username, String password, String clientId) {
}
