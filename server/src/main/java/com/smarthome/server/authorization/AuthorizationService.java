package com.smarthome.server.authorization;

import java.util.List;
import java.util.Set;

import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.account.AppUser;
import com.smarthome.server.account.AppUserRepository;
import com.smarthome.server.common.ForbiddenException;
import com.smarthome.server.common.NotFoundException;
import com.smarthome.server.common.UnauthenticatedException;
import com.smarthome.server.security.SessionPrincipal;

import lombok.RequiredArgsConstructor;

@Service
@RequiredArgsConstructor
public class AuthorizationService {

    private final AppUserRepository userRepository;
    private final GroupMembershipRepository membershipRepository;
    private final NodeGroupMembershipRepository nodeGroupMembershipRepository;

    @Transactional(readOnly = true)
    public AppUser currentUser() {
        Authentication authentication = SecurityContextHolder.getContext().getAuthentication();
        if (authentication == null || !authentication.isAuthenticated()
                || "anonymousUser".equals(authentication.getPrincipal())) {
            throw new ForbiddenException("authentication required");
        }
        AppUser user = userRepository.findByUsernameIgnoreCase(authentication.getName())
                .orElseThrow(() -> new ForbiddenException("account is unavailable"));
        if (!user.isEnabled()) {
            throw new UnauthenticatedException("session expired");
        }
        if (authentication.getPrincipal() instanceof SessionPrincipal principal
                && principal.securityVersion() != user.getSecurityVersion()) {
            throw new UnauthenticatedException("session expired");
        }
        return user;
    }

    @Transactional(readOnly = true)
    public AppUser requireReadyUser() {
        AppUser user = currentUser();
        if (user.isMustChangePassword()) {
            throw new ForbiddenException("password change required");
        }
        return user;
    }

    @Transactional(readOnly = true)
    public AppUser requireSystemAdmin() {
        AppUser user = requireReadyUser();
        if (!user.isSystemAdmin()) {
            throw new ForbiddenException("system administrator permission required");
        }
        return user;
    }

    @Transactional(readOnly = true)
    public List<GroupAccess> groupsFor(AppUser user) {
        return membershipRepository.findByUserIdOrderByGroupName(user.getId()).stream()
                .map(m -> new GroupAccess(m.getGroup().getId(), m.getGroup().getName(), m.getRole().getName()))
                .toList();
    }

    @Transactional(readOnly = true)
    public Set<String> permissionsForNode(AppUser user, String nodeId) {
        if (user.isSystemAdmin()) {
            return Set.of(Permission.NODE_VIEW, Permission.NODE_CONTROL, Permission.TELEMETRY_VIEW);
        }
        return Set.copyOf(nodeGroupMembershipRepository.findPermissionCodes(user.getId(), nodeId));
    }

    @Transactional(readOnly = true)
    public void requireNodePermission(AppUser user, String nodeId, String permission) {
        if (!user.isSystemAdmin()
                && !nodeGroupMembershipRepository.hasPermission(user.getId(), nodeId, permission)) {
            throw new NotFoundException("node %s not found".formatted(nodeId));
        }
    }

    public record GroupAccess(Long id, String name, String roleName) {}
}
