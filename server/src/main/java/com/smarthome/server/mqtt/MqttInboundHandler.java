package com.smarthome.server.mqtt;

import java.time.Instant;

import org.springframework.integration.annotation.ServiceActivator;
import org.springframework.integration.mqtt.support.MqttHeaders;
import org.springframework.messaging.Message;
import org.springframework.stereotype.Component;

import com.smarthome.server.device.Node;
import com.smarthome.server.device.NodeService;
import com.smarthome.server.mqtt.TopicParser.ParsedTopic;
import com.smarthome.server.realtime.StatePublisher;
import com.smarthome.server.telemetry.TelemetryService;

import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.json.JsonMapper;

/**
 * Routes every message of the {@code home/#} subscription. The whole body is wrapped in
 * try/catch-log: an exception thrown into the Paho callback must never kill message
 * delivery. Payloads are parsed once into a Jackson 3 {@link JsonNode}; the same node
 * feeds the DB write (raw JSON string for JSON columns) and the WS event.
 */
@Component
@RequiredArgsConstructor
@Slf4j
public class MqttInboundHandler {

    private final JsonMapper jsonMapper;
    private final NodeService nodeService;
    private final TelemetryService telemetryService;
    private final StatePublisher statePublisher;

    /** Sensor payload contract: {@code ts} is epoch seconds; absent ts falls back to now. */
    record SensorPayload(Double temperature, Double humidity, Long ts) {}

    @ServiceActivator(inputChannel = "mqttInboundChannel")
    public void handle(Message<String> message) {
        String topic = (String) message.getHeaders().get(MqttHeaders.RECEIVED_TOPIC);
        String payload = message.getPayload();
        if (topic == null) {
            return;
        }
        try {
            switch (TopicParser.parse(topic)) {
                case ParsedTopic.Status s -> handleStatus(s, payload);
                case ParsedTopic.Discovery d -> handleDiscovery(d, payload);
                case ParsedTopic.RelayState r -> handleRelayState(r, payload);
                case ParsedTopic.SensorState s -> handleSensorState(s, payload);
                case ParsedTopic.Ignored i -> log.trace("ignoring topic {}", i.topic());
            }
        } catch (Exception e) {
            log.error("failed to process MQTT message on {}: {}", topic, e.toString(), e);
        }
    }

    private void handleStatus(ParsedTopic.Status s, String payload) {
        boolean online = "online".equalsIgnoreCase(payload.trim());
        nodeService.updateStatus(s.room(), s.nodeId(), online);
        statePublisher.publish("NODE_STATUS", s.nodeId(), null,
                jsonMapper.createObjectNode().put("online", online));
    }

    private void handleDiscovery(ParsedTopic.Discovery d, String payload) {
        DiscoveryPayload discovery = jsonMapper.readValue(payload, DiscoveryPayload.class);
        if (discovery.nodeId() == null || discovery.nodeId().isBlank()) {
            log.warn("discovery on {} without node_id — dropped", d.nodeId());
            return;
        }
        if (!discovery.nodeId().equals(d.nodeId())) {
            log.warn("discovery topic nodeId {} != payload node_id {} — payload wins",
                    d.nodeId(), discovery.nodeId());
        }
        Node node = nodeService.upsertFromDiscovery(discovery, d.room());
        // optional refresh event so already-connected UIs re-render the (possibly new) node
        statePublisher.publish("NODE_STATUS", node.getNodeId(), null,
                jsonMapper.createObjectNode().put("online", node.isOnline()));
    }

    private void handleRelayState(ParsedTopic.RelayState r, String payload) {
        JsonNode parsed = jsonMapper.readTree(payload);
        boolean updated = nodeService.updateRelayState(r.nodeId(), r.channel(), parsed.toString());
        if (updated) {
            statePublisher.publish("RELAY_STATE", r.nodeId(), r.channel(), parsed);
        } else {
            log.warn("relay state for unknown node/capability {} ch {} — dropped (self-heals via retained replay)",
                    r.nodeId(), r.channel());
        }
    }

    private void handleSensorState(ParsedTopic.SensorState s, String payload) {
        JsonNode parsed = jsonMapper.readTree(payload);
        SensorPayload sensor = jsonMapper.readValue(payload, SensorPayload.class);
        Instant ts = sensor.ts() != null ? Instant.ofEpochSecond(sensor.ts()) : Instant.now();
        boolean recorded = telemetryService.record(s.nodeId(), sensor.temperature(), sensor.humidity(), ts);
        if (recorded) {
            statePublisher.publish("SENSOR_STATE", s.nodeId(), null, parsed);
        } else {
            log.warn("sensor state for unknown node {} — dropped", s.nodeId());
        }
    }
}
