package com.smarthome.server.common.dto;

import java.time.Instant;

/** {@code ts} serializes as an ISO-8601 instant (Jackson 3 java.time default). */
public record SensorReadingDto(Double temperature, Double humidity, Instant ts) {
}
