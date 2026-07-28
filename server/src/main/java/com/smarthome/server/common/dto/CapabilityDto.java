package com.smarthome.server.common.dto;

import com.fasterxml.jackson.annotation.JsonRawValue;
import java.util.List;

/**
 * {@code meta} and {@code lastState} are raw JSONB text from the DB, re-emitted verbatim
 * via {@code @JsonRawValue} (annotations stay {@code com.fasterxml} under Jackson 3).
 */
public record CapabilityDto(
        Long id,
        String type,
        int channel,
        String displayName,
        String discoveryName,
        DeviceTypeDto deviceType,
        List<TagDto> tags,
        @JsonRawValue String meta,
        @JsonRawValue String lastState) {

    public record DeviceTypeDto(Long id, String name, String description) {}
    public record TagDto(Long id, String name, String color) {}
}
