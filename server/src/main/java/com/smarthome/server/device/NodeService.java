package com.smarthome.server.device;

import java.time.Instant;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.common.NotFoundException;
import com.smarthome.server.common.dto.CapabilityDto;
import com.smarthome.server.common.dto.NodeDto;
import com.smarthome.server.mqtt.DiscoveryPayload;
import com.smarthome.server.mqtt.MqttGateway;

import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import tools.jackson.databind.json.JsonMapper;

@Service
@RequiredArgsConstructor
@Slf4j
public class NodeService {

    private static final Set<String> CAPABILITY_FIXED_KEYS = Set.of("type", "channel", "name");

    private final NodeRepository nodeRepository;
    private final MqttGateway mqttGateway;
    private final JsonMapper jsonMapper;

    // ------------------------------------------------------------------ queries

    @Transactional(readOnly = true)
    public List<NodeDto> getAllNodes() {
        return nodeRepository.findAllWithCapabilities().stream().map(this::toDto).toList();
    }

    @Transactional(readOnly = true)
    public NodeDto getNode(String nodeId) {
        return toDto(requireNodeWithCapabilities(nodeId));
    }

    // ------------------------------------------------------------------ MQTT-driven writes

    /**
     * The status topic is the only owner of {@code online}. If the node is unknown, a stub
     * row (nodeId + room from the topic) is created instead of dropping the message: the
     * live boot sequence publishes retained {@code status} <em>before</em> {@code discovery},
     * so a strict drop would leave a brand-new node offline until its next status publish.
     * Discovery fills in fw/ip/capabilities milliseconds later.
     */
    @Transactional
    public Node updateStatus(String room, String nodeId, boolean online) {
        Node node = nodeRepository.findByNodeId(nodeId).orElseGet(() -> {
            log.info("status for unknown node {} — creating stub row (discovery pending)", nodeId);
            Node stub = new Node();
            stub.setNodeId(nodeId);
            stub.setRoom(room);
            return stub;
        });
        node.setOnline(online);
        node.setLastSeen(Instant.now());
        return nodeRepository.save(node);
    }

    /**
     * Serial MQTT delivery (DirectChannel) means no concurrent upserts, so plain
     * read-modify-write is safe. Never touches {@code online} — the status topic owns it.
     * Capabilities are reconciled by key {@code (type, channel)}: names/meta updated in
     * place (preserving {@code last_state}), missing ones inserted, DB rows absent from the
     * payload deleted via {@code orphanRemoval} (node reflashed with fewer channels).
     */
    @Transactional
    public Node upsertFromDiscovery(DiscoveryPayload payload, String topicRoom) {
        Node node = nodeRepository.findWithCapabilitiesByNodeId(payload.nodeId()).orElseGet(() -> {
            Node created = new Node();
            created.setNodeId(payload.nodeId());
            return created;
        });
        node.setRoom(payload.room() != null ? payload.room() : topicRoom);
        node.setFwVersion(payload.fwVersion());
        node.setIp(payload.ip());

        List<Map<String, Object>> incoming = payload.capabilities() != null
                ? payload.capabilities() : List.of();
        Set<String> seenKeys = new HashSet<>();
        for (Map<String, Object> raw : incoming) {
            if (!(raw.get("type") instanceof String type)
                    || !(raw.get("channel") instanceof Number channelNumber)) {
                log.warn("discovery capability without type/channel for {} — skipped: {}",
                        payload.nodeId(), raw);
                continue;
            }
            int channel = channelNumber.intValue();
            seenKeys.add(capabilityKey(type, channel));

            Capability capability = node.getCapabilities().stream()
                    .filter(c -> c.getType().equals(type) && c.getChannel() == channel)
                    .findFirst()
                    .orElseGet(() -> {
                        Capability created = new Capability();
                        created.setNode(node);
                        created.setType(type);
                        created.setChannel(channel);
                        node.getCapabilities().add(created);
                        return created;
                    });
            capability.setName(raw.get("name") instanceof String name ? name : null);
            capability.setMeta(extractMeta(raw));
            // last_state deliberately preserved
        }
        node.getCapabilities()
                .removeIf(c -> !seenKeys.contains(capabilityKey(c.getType(), c.getChannel())));

        return nodeRepository.save(node);
    }

    /** @return false when the node or the (relay, channel) capability is unknown — caller drops. */
    @Transactional
    public boolean updateRelayState(String nodeId, int channel, String rawJson) {
        return nodeRepository.findWithCapabilitiesByNodeId(nodeId)
                .flatMap(node -> node.getCapabilities().stream()
                        .filter(c -> "relay".equals(c.getType()) && c.getChannel() == channel)
                        .findFirst())
                .map(capability -> {
                    capability.setLastState(rawJson);
                    return true;
                })
                .orElse(false);
    }

    // ------------------------------------------------------------------ commands

    /**
     * Publishes the relay command and returns immediately (async publisher — the REST layer
     * answers 202). State-topic-is-truth: the resulting state is never waited for or faked;
     * {@code last_state} changes only when the node reports back on {@code .../relay/{ch}/state}.
     */
    @Transactional(readOnly = true)
    public void sendRelayCommand(String nodeId, int channel, String state) {
        Node node = requireNodeWithCapabilities(nodeId);
        boolean hasRelay = node.getCapabilities().stream()
                .anyMatch(c -> "relay".equals(c.getType()) && c.getChannel() == channel);
        if (!hasRelay) {
            throw new NotFoundException(
                    "node %s has no relay capability on channel %d".formatted(nodeId, channel));
        }
        String topic = "home/%s/%s/relay/%d/set".formatted(node.getRoom(), nodeId, channel);
        String payload = "{\"state\":\"%s\"}".formatted(state);
        mqttGateway.publish(topic, payload);
        log.info("command published to {}: {}", topic, payload);
    }

    // ------------------------------------------------------------------ helpers

    private Node requireNodeWithCapabilities(String nodeId) {
        return nodeRepository.findWithCapabilitiesByNodeId(nodeId)
                .orElseThrow(() -> new NotFoundException("node %s not found".formatted(nodeId)));
    }

    private static String capabilityKey(String type, int channel) {
        return type + "#" + channel;
    }

    /** Everything except type/channel/name goes to meta — unknown firmware fields survive. */
    private String extractMeta(Map<String, Object> raw) {
        Map<String, Object> meta = new LinkedHashMap<>(raw);
        meta.keySet().removeAll(CAPABILITY_FIXED_KEYS);
        return jsonMapper.writeValueAsString(meta);
    }

    private NodeDto toDto(Node node) {
        List<CapabilityDto> capabilities = node.getCapabilities().stream()
                .sorted(Comparator.comparing(Capability::getType)
                        .thenComparingInt(Capability::getChannel))
                .map(c -> new CapabilityDto(c.getType(), c.getChannel(), c.getName(),
                        c.getMeta(), c.getLastState()))
                .toList();
        return new NodeDto(node.getNodeId(), node.getRoom(), node.getFwVersion(), node.getIp(),
                node.isOnline(), node.getLastSeen(), capabilities);
    }
}
