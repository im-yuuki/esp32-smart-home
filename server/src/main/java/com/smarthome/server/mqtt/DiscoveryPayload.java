package com.smarthome.server.mqtt;

import java.util.List;
import java.util.Map;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Mirror of the firmware discovery JSON (snake_case on the wire; annotations stay
 * {@code com.fasterxml} under Jackson 3).
 *
 * <p>Capabilities are kept as raw maps on purpose: each entry carries the fixed keys
 * {@code type}/{@code channel}/{@code name} plus arbitrary extra fields ({@code kind},
 * {@code model}, {@code interval_s}, future keys) that flow into the {@code meta} JSON
 * column — forward-compatible with unknown firmware fields by construction.</p>
 */
public record DiscoveryPayload(
        @JsonProperty("node_id") String nodeId,
        String room,
        @JsonProperty("fw_version") String fwVersion,
        String ip,
        List<Map<String, Object>> capabilities) {
}
