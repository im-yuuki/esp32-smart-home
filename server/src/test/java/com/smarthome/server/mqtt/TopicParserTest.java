package com.smarthome.server.mqtt;

import static org.assertj.core.api.Assertions.assertThat;

import org.junit.jupiter.api.Test;

import com.smarthome.server.mqtt.TopicParser.ParsedTopic;

class TopicParserTest {

    private static final String BASE = "home/phong-khach/esp32s3-aabbcc";

    @Test
    void parsesStatus() {
        assertThat(TopicParser.parse(BASE + "/status"))
                .isEqualTo(new ParsedTopic.Status("phong-khach", "esp32s3-aabbcc"));
    }

    @Test
    void parsesDiscovery() {
        assertThat(TopicParser.parse(BASE + "/discovery"))
                .isEqualTo(new ParsedTopic.Discovery("phong-khach", "esp32s3-aabbcc"));
    }

    @Test
    void parsesRelayState() {
        assertThat(TopicParser.parse(BASE + "/relay/2/state"))
                .isEqualTo(new ParsedTopic.RelayState("phong-khach", "esp32s3-aabbcc", 2));
    }

    @Test
    void parsesSensorState() {
        assertThat(TopicParser.parse(BASE + "/sensor/state"))
                .isEqualTo(new ParsedTopic.SensorState("phong-khach", "esp32s3-aabbcc"));
    }

    @Test
    void ignoresServerOwnRelaySetEcho() {
        assertThat(TopicParser.parse(BASE + "/relay/1/set"))
                .isEqualTo(new ParsedTopic.Ignored(BASE + "/relay/1/set"));
    }

    @Test
    void ignoresServerOwnCmdEcho() {
        assertThat(TopicParser.parse(BASE + "/cmd"))
                .isEqualTo(new ParsedTopic.Ignored(BASE + "/cmd"));
    }

    @Test
    void ignoresForeignPrefix() {
        assertThat(TopicParser.parse("office/room/node/status"))
                .isInstanceOf(ParsedTopic.Ignored.class);
    }

    @Test
    void ignoresTooShortTopic() {
        assertThat(TopicParser.parse("home/phong-khach"))
                .isInstanceOf(ParsedTopic.Ignored.class);
    }

    @Test
    void ignoresNonNumericRelayChannel() {
        assertThat(TopicParser.parse(BASE + "/relay/one/state"))
                .isInstanceOf(ParsedTopic.Ignored.class);
    }

    @Test
    void ignoresStatusWithTrailingSegments() {
        assertThat(TopicParser.parse(BASE + "/status/extra"))
                .isInstanceOf(ParsedTopic.Ignored.class);
    }

    @Test
    void ignoresRelayWithoutStateSuffix() {
        assertThat(TopicParser.parse(BASE + "/relay/1"))
                .isInstanceOf(ParsedTopic.Ignored.class);
    }
}
