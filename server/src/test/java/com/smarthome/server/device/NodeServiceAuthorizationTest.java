package com.smarthome.server.device;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.inOrder;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;

import java.util.List;
import java.util.Optional;
import java.util.Set;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import com.smarthome.server.account.AppUser;
import com.smarthome.server.audit.AuditService;
import com.smarthome.server.authorization.AuthorizationService;
import com.smarthome.server.authorization.Folder;
import com.smarthome.server.authorization.FolderRepository;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.common.dto.NodeDto;
import com.smarthome.server.mqtt.MqttGateway;

import tools.jackson.databind.json.JsonMapper;

class NodeServiceAuthorizationTest {

    private final NodeRepository nodeRepository = mock(NodeRepository.class);
    private final MqttGateway mqttGateway = mock(MqttGateway.class);
    private final AuthorizationService authorizationService = mock(AuthorizationService.class);
    private final FolderRepository folderRepository = mock(FolderRepository.class);
    private final AuditService auditService = mock(AuditService.class);
    private final JsonMapper jsonMapper = JsonMapper.builder().build();
    private NodeService service;
    private AppUser user;

    @BeforeEach
    void setUp() {
        service = new NodeService(nodeRepository, mqttGateway, jsonMapper, authorizationService,
                folderRepository, auditService);
        user = new AppUser();
        user.setId(7L);
        user.setUsername("operator");
        when(authorizationService.requireReadyUser()).thenReturn(user);
    }

    @Test
    void relayCommandKeepsCentralIdentityOutOfMqttPayload() {
        Node node = approvedNode();
        Capability relay = capability(node, "relay", 1, "Light", "{\"state\":\"OFF\"}");
        node.getCapabilities().add(relay);
        when(nodeRepository.findApprovedWithCapabilitiesByNodeId("esp32s3-aabbcc"))
                .thenReturn(Optional.of(node));

        service.sendRelayCommand("esp32s3-aabbcc", 1, "ON");

        verify(authorizationService).requireNodePermission(user, "esp32s3-aabbcc", Permission.NODE_CONTROL);
        var ordered = inOrder(auditService, mqttGateway);
        ordered.verify(auditService).recordControl(eq(user), eq("CONTROL_REQUESTED"),
                eq("esp32s3-aabbcc"), anyString(), anyString(), eq(null), any(Capability.class));
        ordered.verify(mqttGateway).publish("home/phong-khach/esp32s3-aabbcc/relay/1/set",
                "{\"state\":\"ON\"}");
        ordered.verify(auditService).recordControl(eq(user), eq("CONTROL_DISPATCHED"),
                eq("esp32s3-aabbcc"), anyString(), anyString(), eq(null), any(Capability.class));
    }

    @Test
    void nodeViewWithoutTelemetryPermissionOmitsSensorCapability() {
        Node node = approvedNode();
        node.getCapabilities().add(capability(node, "relay", 1, "Light", "{\"state\":\"OFF\"}"));
        node.getCapabilities().add(capability(node, "sensor", 1, "Climate",
                "{\"temperature\":28.5,\"humidity\":60}"));
        when(nodeRepository.findApprovedWithCapabilitiesByNodeId(node.getNodeId()))
                .thenReturn(Optional.of(node));
        when(authorizationService.permissionsForNode(user, node.getNodeId()))
                .thenReturn(Set.of(Permission.NODE_VIEW));

        NodeDto dto = service.getNode(node.getNodeId());

        assertThat(dto.capabilities()).extracting("type").containsExactly("relay");
        assertThat(dto.permissions()).containsExactly(Permission.NODE_VIEW);
    }

    private static Node approvedNode() {
        Node node = new Node();
        node.setId(11L);
        node.setNodeId("esp32s3-aabbcc");
        node.setRoom("phong-khach");
        node.setApprovalStatus(ApprovalStatus.APPROVED);
        Folder folder = new Folder();
        folder.setId(3L);
        node.setFolder(folder);
        return node;
    }

    private static Capability capability(Node node, String type, int channel, String name,
                                         String lastState) {
        Capability capability = new Capability();
        capability.setNode(node);
        capability.setType(type);
        capability.setChannel(channel);
        capability.setDiscoveryName(name);
        capability.setMeta("{}");
        capability.setLastState(lastState);
        return capability;
    }
}
