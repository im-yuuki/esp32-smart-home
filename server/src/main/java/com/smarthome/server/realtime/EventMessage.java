package com.smarthome.server.realtime;

import tools.jackson.databind.JsonNode;

/**
 * The normalized per-user event payload. Types: {@code RELAY_STATE},
 * {@code SENSOR_STATE}, {@code NODE_STATUS}. {@code channel} is null for events without a
 * channel dimension. {@code ts} is epoch <b>millis</b> (cross-plan contract with the web UI).
 */
public record EventMessage(String type, String nodeId, Integer channel, JsonNode data, long ts) {
}
