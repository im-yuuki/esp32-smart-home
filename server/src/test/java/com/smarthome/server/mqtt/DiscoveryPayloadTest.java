package com.smarthome.server.mqtt;

import static org.assertj.core.api.Assertions.assertThat;

import java.util.Map;

import org.junit.jupiter.api.Test;

import tools.jackson.databind.DeserializationFeature;
import tools.jackson.databind.json.JsonMapper;

/**
 * JSON -> record mapping with Jackson 3, built the same way as the runtime mapper
 * (FAIL_ON_UNKNOWN_PROPERTIES disabled for firmware payload evolution).
 */
class DiscoveryPayloadTest {

    private final JsonMapper mapper = JsonMapper.builder()
            .disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)
            .build();

    /** Exactly the payload the fake node (and firmware contract) publishes on boot. */
    private static final String BOOT_DISCOVERY_JSON = """
            {"node_id":"esp32s3-aabbcc","room":"phong-khach","fw_version":"1.0.0",\
            "ip":"192.168.1.51","capabilities":[\
            {"type":"relay","channel":1,"name":"Den tran"},\
            {"type":"relay","channel":2,"name":"Den ban"},\
            {"type":"sensor","channel":1,"kind":"temperature_humidity","model":"SHT31","interval_s":30}]}""";

    @Test
    void mapsBootDiscoveryJson() {
        DiscoveryPayload payload = mapper.readValue(BOOT_DISCOVERY_JSON, DiscoveryPayload.class);

        assertThat(payload.nodeId()).isEqualTo("esp32s3-aabbcc");
        assertThat(payload.room()).isEqualTo("phong-khach");
        assertThat(payload.fwVersion()).isEqualTo("1.0.0");
        assertThat(payload.ip()).isEqualTo("192.168.1.51");
        assertThat(payload.capabilities()).hasSize(3);

        Map<String, Object> relay1 = payload.capabilities().get(0);
        assertThat(relay1.get("type")).isEqualTo("relay");
        assertThat(((Number) relay1.get("channel")).intValue()).isEqualTo(1);
        assertThat(relay1.get("name")).isEqualTo("Den tran");

        Map<String, Object> sensor = payload.capabilities().get(2);
        assertThat(sensor.get("type")).isEqualTo("sensor");
        assertThat(sensor.get("name")).isNull();
        // extra fields survive untouched -> they become the capability's meta JSONB
        assertThat(sensor.get("kind")).isEqualTo("temperature_humidity");
        assertThat(sensor.get("model")).isEqualTo("SHT31");
        assertThat(((Number) sensor.get("interval_s")).intValue()).isEqualTo(30);
    }

    @Test
    void toleratesUnknownTopLevelFields() {
        String json = """
                {"node_id":"esp32s3-ffeedd","room":"bep","future_field":{"nested":true}}""";

        DiscoveryPayload payload = mapper.readValue(json, DiscoveryPayload.class);

        assertThat(payload.nodeId()).isEqualTo("esp32s3-ffeedd");
        assertThat(payload.room()).isEqualTo("bep");
        assertThat(payload.fwVersion()).isNull();
        assertThat(payload.capabilities()).isNull();
    }
}
