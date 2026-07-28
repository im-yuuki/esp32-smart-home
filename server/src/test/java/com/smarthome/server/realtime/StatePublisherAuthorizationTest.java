package com.smarthome.server.realtime;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.util.List;

import org.junit.jupiter.api.Test;
import org.springframework.messaging.simp.SimpMessagingTemplate;

import com.smarthome.server.account.AppUserRepository;
import com.smarthome.server.authorization.FolderMembershipRepository;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.device.ApprovalStatus;
import com.smarthome.server.device.NodeRepository;

import tools.jackson.databind.json.JsonMapper;

class StatePublisherAuthorizationTest {

    @Test
    void sensorEventsRequireTelemetryPermissionAndDeduplicateRecipients() {
        SimpMessagingTemplate template = mock(SimpMessagingTemplate.class);
        NodeRepository nodeRepository = mock(NodeRepository.class);
        FolderMembershipRepository memberships = mock(FolderMembershipRepository.class);
        AppUserRepository users = mock(AppUserRepository.class);
        StatePublisher publisher = new StatePublisher(template, nodeRepository, memberships, users);
        when(nodeRepository.existsByNodeIdAndApprovalStatus("node-1", ApprovalStatus.APPROVED))
                .thenReturn(true);
        when(memberships.findAuthorizedUsernames("node-1", Permission.TELEMETRY_VIEW))
                .thenReturn(List.of("viewer", "viewer"));
        when(users.findEnabledSystemAdminUsernames()).thenReturn(List.of());

        publisher.publish("SENSOR_STATE", "node-1", null,
                JsonMapper.builder().build().createObjectNode().put("temperature", 28.5));

        verify(memberships).findAuthorizedUsernames("node-1", Permission.TELEMETRY_VIEW);
        verify(template).convertAndSendToUser(eq("viewer"), eq("/queue/events"), any(EventMessage.class));
    }
}
