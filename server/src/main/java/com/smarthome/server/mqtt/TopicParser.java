package com.smarthome.server.mqtt;

/**
 * Parses MQTT topics of the {@code home/{room}/{nodeId}/...} contract into a sealed
 * {@link ParsedTopic}. Everything that is not one of the four inbound message kinds —
 * including the server's <b>own</b> {@code .../set} and {@code .../cmd} publishes echoed
 * back by the broker (MQTT v3 has no noLocal) and malformed topics — lands in
 * {@link ParsedTopic.Ignored}.
 */
public final class TopicParser {

    private TopicParser() {
    }

    public sealed interface ParsedTopic {
        record Status(String room, String nodeId) implements ParsedTopic {}
        record Discovery(String room, String nodeId) implements ParsedTopic {}
        record RelayState(String room, String nodeId, int channel) implements ParsedTopic {}
        record SensorState(String room, String nodeId) implements ParsedTopic {}
        record Ignored(String topic) implements ParsedTopic {}
    }

    public static ParsedTopic parse(String topic) {
        String[] p = topic.split("/");
        if (p.length < 4 || !"home".equals(p[0])) {
            return new ParsedTopic.Ignored(topic);
        }
        return switch (p[3]) {
            case "status" -> p.length == 4
                    ? new ParsedTopic.Status(p[1], p[2])
                    : new ParsedTopic.Ignored(topic);
            case "discovery" -> p.length == 4
                    ? new ParsedTopic.Discovery(p[1], p[2])
                    : new ParsedTopic.Ignored(topic);
            case "sensor" -> p.length == 5 && "state".equals(p[4])
                    ? new ParsedTopic.SensorState(p[1], p[2])
                    : new ParsedTopic.Ignored(topic);
            case "relay" -> parseRelay(p, topic);
            default -> new ParsedTopic.Ignored(topic); // catches /set, /cmd, malformed
        };
    }

    private static ParsedTopic parseRelay(String[] p, String topic) {
        if (p.length == 6 && "state".equals(p[5])) {
            try {
                return new ParsedTopic.RelayState(p[1], p[2], Integer.parseInt(p[4]));
            } catch (NumberFormatException e) {
                // non-numeric channel -> malformed -> ignored
            }
        }
        return new ParsedTopic.Ignored(topic);
    }
}
