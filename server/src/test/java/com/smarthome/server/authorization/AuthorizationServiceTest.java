package com.smarthome.server.authorization;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.List;

import org.junit.jupiter.api.Test;

import com.smarthome.server.account.AppUser;
import com.smarthome.server.account.AppUserRepository;
import com.smarthome.server.device.NodeRepository;

class AuthorizationServiceTest {
    @Test
    void resolvesNodePermissionsThroughFolderMembershipRepository() {
        AppUserRepository users = mock(AppUserRepository.class);
        FolderMembershipRepository memberships = mock(FolderMembershipRepository.class);
        NodeRepository nodes = mock(NodeRepository.class);
        AuthorizationService service = new AuthorizationService(users, memberships, nodes);
        AppUser user = new AppUser();
        user.setId(7L);
        when(nodes.findFolderIdByNodeId("node-1")).thenReturn(java.util.Optional.of(12L));
        when(memberships.findPermissionCodes(7L, 12L))
                .thenReturn(List.of(Permission.NODE_VIEW, Permission.NODE_CONTROL));

        assertThat(service.permissionsForNode(user, "node-1"))
                .containsExactlyInAnyOrder(Permission.NODE_VIEW, Permission.NODE_CONTROL);
    }
}
