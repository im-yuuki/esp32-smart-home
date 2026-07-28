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
import com.smarthome.server.device.NodeRepository;

import lombok.RequiredArgsConstructor;

@Service
@RequiredArgsConstructor
public class AuthorizationService {

    private final AppUserRepository userRepository;
    private final FolderMembershipRepository membershipRepository;
    private final NodeRepository nodeRepository;

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
    public List<FolderAccess> foldersFor(AppUser user) {
        return membershipRepository.findByUserIdOrderByFolderName(user.getId()).stream()
                .map(m -> new FolderAccess(m.getFolder().getId(), m.getFolder().getName(),
                        m.getRole().getName()))
                .toList();
    }

    @Transactional(readOnly = true)
    public Set<String> permissionsForNode(AppUser user, String nodeId) {
        if (user.isSystemAdmin()) {
            return Set.of(Permission.NODE_VIEW, Permission.NODE_CONTROL, Permission.TELEMETRY_VIEW,
                    Permission.AUDIT_VIEW);
        }
        Long folderId = nodeRepository.findFolderIdByNodeId(nodeId)
                .orElseThrow(() -> new NotFoundException("node %s not found".formatted(nodeId)));
        return permissionsForFolder(user, folderId);
    }

    @Transactional(readOnly = true)
    public void requireNodePermission(AppUser user, String nodeId, String permission) {
        Long folderId = nodeRepository.findFolderIdByNodeId(nodeId)
                .orElseThrow(() -> new NotFoundException("node %s not found".formatted(nodeId)));
        if (!user.isSystemAdmin() && !membershipRepository.hasPermission(user.getId(), folderId, permission)) {
            throw new NotFoundException("node %s not found".formatted(nodeId));
        }
    }

    @Transactional(readOnly = true)
    public Set<String> permissionsForFolder(AppUser user, Long folderId) {
        if (user.isSystemAdmin()) {
            return Set.of(Permission.NODE_VIEW, Permission.NODE_CONTROL, Permission.TELEMETRY_VIEW,
                    Permission.AUDIT_VIEW);
        }
        return Set.copyOf(membershipRepository.findPermissionCodes(user.getId(), folderId));
    }

    @Transactional(readOnly = true)
    public void requireFolderPermission(AppUser user, Long folderId, String permission) {
        if (!user.isSystemAdmin() && !membershipRepository.hasPermission(user.getId(), folderId, permission)) {
            throw new NotFoundException("folder %d not found".formatted(folderId));
        }
    }

    @Transactional(readOnly = true)
    public boolean hasAnyAuditAccess(AppUser user) {
        return user.isSystemAdmin() || foldersFor(user).stream().anyMatch(folder ->
                membershipRepository.hasPermission(user.getId(), folder.id(), Permission.AUDIT_VIEW));
    }

    public record FolderAccess(Long id, String name, String roleName) {}
}
