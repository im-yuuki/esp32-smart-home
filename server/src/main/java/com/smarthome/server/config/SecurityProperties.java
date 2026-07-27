package com.smarthome.server.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "smarthome.security")
public record SecurityProperties(BootstrapAdmin bootstrapAdmin) {

    public record BootstrapAdmin(String username, String password, String displayName) {
    }
}
