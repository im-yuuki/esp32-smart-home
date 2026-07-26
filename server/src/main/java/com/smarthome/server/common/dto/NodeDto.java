package com.smarthome.server.common.dto;

import java.time.Instant;
import java.util.List;

public record NodeDto(
        String nodeId,
        String room,
        String fwVersion,
        String ip,
        boolean online,
        Instant lastSeen,
        List<CapabilityDto> capabilities) {
}
