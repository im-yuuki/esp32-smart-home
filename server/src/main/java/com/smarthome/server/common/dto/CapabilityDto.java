package com.smarthome.server.common.dto;

import com.fasterxml.jackson.annotation.JsonRawValue;

/**
 * {@code meta} and {@code lastState} are raw JSONB text from the DB, re-emitted verbatim
 * via {@code @JsonRawValue} (annotations stay {@code com.fasterxml} under Jackson 3).
 */
public record CapabilityDto(
        String type,
        int channel,
        String name,
        @JsonRawValue String meta,
        @JsonRawValue String lastState) {
}
