package com.smarthome.server.common.dto;

import java.time.Instant;
import java.util.List;
import java.util.Set;

public record NodeDto(
        String nodeId,
        String room,
        String fwVersion,
        String ip,
        boolean online,
        Instant lastSeen,
        List<CapabilityDto> capabilities,
        List<Long> groupIds,
        Set<String> permissions) {
}
