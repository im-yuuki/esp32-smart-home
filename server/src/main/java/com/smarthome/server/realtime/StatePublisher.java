package com.smarthome.server.realtime;

import java.util.HashSet;
import java.util.Set;

import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Component;

import com.smarthome.server.account.AppUserRepository;
import com.smarthome.server.authorization.NodeGroupMembershipRepository;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.device.ApprovalStatus;
import com.smarthome.server.device.NodeRepository;

import lombok.RequiredArgsConstructor;
import tools.jackson.databind.JsonNode;

@Component
@RequiredArgsConstructor
public class StatePublisher {

    public static final String EVENTS_DESTINATION = "/queue/events";

    private final SimpMessagingTemplate template;
    private final NodeRepository nodeRepository;
    private final NodeGroupMembershipRepository membershipRepository;
    private final AppUserRepository userRepository;

    public void publish(String type, String nodeId, Integer channel, JsonNode data) {
        if (!nodeRepository.existsByNodeIdAndApprovalStatus(nodeId, ApprovalStatus.APPROVED)) {
            return;
        }
        String requiredPermission = "SENSOR_STATE".equals(type)
                ? Permission.TELEMETRY_VIEW : Permission.NODE_VIEW;
        Set<String> recipients = new HashSet<>(
                membershipRepository.findAuthorizedUsernames(nodeId, requiredPermission));
        recipients.addAll(userRepository.findEnabledSystemAdminUsernames());
        EventMessage event = new EventMessage(type, nodeId, channel, data, System.currentTimeMillis());
        recipients.forEach(username -> template.convertAndSendToUser(username, EVENTS_DESTINATION, event));
    }
}
