package com.smarthome.server.realtime;

import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Component;

import lombok.RequiredArgsConstructor;
import tools.jackson.databind.JsonNode;

@Component
@RequiredArgsConstructor
public class StatePublisher {

    public static final String EVENTS_DESTINATION = "/topic/events";

    private final SimpMessagingTemplate template;

    public void publish(String type, String nodeId, Integer channel, JsonNode data) {
        template.convertAndSend(EVENTS_DESTINATION,
                new EventMessage(type, nodeId, channel, data, System.currentTimeMillis()));
    }
}
