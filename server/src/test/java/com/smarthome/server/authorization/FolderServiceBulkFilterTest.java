package com.smarthome.server.authorization;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.List;
import java.util.Set;

import org.junit.jupiter.api.Test;

import com.smarthome.server.account.AppUser;
import com.smarthome.server.audit.AuditService;
import com.smarthome.server.authorization.FolderDtos.BulkActionRequest;
import com.smarthome.server.authorization.FolderDtos.BulkState;
import com.smarthome.server.authorization.FolderDtos.TagMatch;
import com.smarthome.server.device.Capability;
import com.smarthome.server.device.DeviceType;
import com.smarthome.server.device.DeviceTypeRepository;
import com.smarthome.server.device.Node;
import com.smarthome.server.device.NodeRepository;
import com.smarthome.server.device.NodeService;
import com.smarthome.server.device.PlacementRepository;
import com.smarthome.server.device.Tag;
import com.smarthome.server.device.TagRepository;
import com.smarthome.server.mqtt.MqttGateway;

import tools.jackson.databind.json.JsonMapper;

class FolderServiceBulkFilterTest {
    @Test
    void selectsOnlyAuthorizedRelaysMatchingAllTagsAndDeviceType() {
        FolderClosureRepository closures = mock(FolderClosureRepository.class);
        NodeRepository nodes = mock(NodeRepository.class);
        AuthorizationService authorization = mock(AuthorizationService.class);
        FolderService service = new FolderService(mock(FolderRepository.class), closures,
                mock(PlacementRepository.class), mock(DeviceTypeRepository.class),
                mock(TagRepository.class), nodes, mock(BulkOperationRepository.class), authorization,
                mock(NodeService.class), mock(AuditService.class), mock(MqttGateway.class),
                JsonMapper.builder().build());

        AppUser user = new AppUser();
        user.setId(7L);
        Folder folder = new Folder();
        folder.setId(2L);
        Node node = new Node();
        node.setNodeId("node-a");
        node.setFolder(folder);
        DeviceType light = new DeviceType();
        light.setId(5L);
        Tag ceiling = new Tag();
        ceiling.setId(9L);
        Tag publicArea = new Tag();
        publicArea.setId(10L);
        Capability matching = relay(node, 1, light, Set.of(ceiling, publicArea));
        Capability missingTag = relay(node, 2, light, Set.of(ceiling));
        node.getCapabilities().addAll(List.of(matching, missingTag));

        when(closures.findDescendantIds(1L)).thenReturn(List.of(1L, 2L));
        when(nodes.findApprovedByFolderIdsWithDetails(List.of(1L, 2L))).thenReturn(List.of(node));
        when(authorization.permissionsForFolder(user, 2L)).thenReturn(Set.of(Permission.NODE_CONTROL));
        BulkActionRequest request = new BulkActionRequest(true, Set.of(5L), Set.of(9L, 10L),
                TagMatch.ALL, BulkState.ON, "request-1");

        assertThat(service.selectTargets(1L, request, user))
                .extracting(target -> target.capability().getChannel())
                .containsExactly(1);
    }

    private static Capability relay(Node node, int channel, DeviceType type, Set<Tag> tags) {
        Capability capability = new Capability();
        capability.setId((long) channel);
        capability.setNode(node);
        capability.setType("relay");
        capability.setChannel(channel);
        capability.setDeviceType(type);
        capability.setTags(tags);
        return capability;
    }
}
