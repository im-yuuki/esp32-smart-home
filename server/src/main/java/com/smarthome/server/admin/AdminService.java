package com.smarthome.server.admin;

import static com.smarthome.server.admin.AdminDtos.*;

import java.time.Instant;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.account.AppUser;
import com.smarthome.server.account.AppUserRepository;
import com.smarthome.server.audit.AuditService;
import com.smarthome.server.authorization.AuthorizationService;
import com.smarthome.server.authorization.GroupMembership;
import com.smarthome.server.authorization.GroupMembershipRepository;
import com.smarthome.server.authorization.GroupRole;
import com.smarthome.server.authorization.GroupRoleRepository;
import com.smarthome.server.authorization.NodeGroup;
import com.smarthome.server.authorization.NodeGroupMembership;
import com.smarthome.server.authorization.NodeGroupMembershipRepository;
import com.smarthome.server.authorization.NodeGroupRepository;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.authorization.PermissionRepository;
import com.smarthome.server.common.ForbiddenException;
import com.smarthome.server.common.NotFoundException;
import com.smarthome.server.device.ApprovalStatus;
import com.smarthome.server.device.Node;
import com.smarthome.server.device.NodeRepository;
import com.smarthome.server.security.SessionRevocationService;

import lombok.RequiredArgsConstructor;
import tools.jackson.databind.json.JsonMapper;

@Service
@RequiredArgsConstructor
public class AdminService {

    private final AuthorizationService authorizationService;
    private final AppUserRepository userRepository;
    private final NodeGroupRepository groupRepository;
    private final GroupRoleRepository roleRepository;
    private final PermissionRepository permissionRepository;
    private final GroupMembershipRepository membershipRepository;
    private final NodeGroupMembershipRepository nodeMembershipRepository;
    private final NodeRepository nodeRepository;
    private final PasswordEncoder passwordEncoder;
    private final AuditService auditService;
    private final JsonMapper jsonMapper;
    private final SessionRevocationService sessionRevocationService;

    @Transactional(readOnly = true)
    public List<UserDto> users() {
        authorizationService.requireSystemAdmin();
        return userRepository.findAll().stream().map(AdminService::toUserDto).toList();
    }

    @Transactional
    public UserDto createUser(CreateUserRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        if (userRepository.findByUsernameIgnoreCase(request.username().trim()).isPresent()) {
            throw new ForbiddenException("username already exists");
        }
        AppUser user = new AppUser();
        user.setUsername(request.username().trim());
        user.setDisplayName(request.displayName().trim());
        user.setPasswordHash(passwordEncoder.encode(request.temporaryPassword()));
        user.setEnabled(true);
        user.setSystemAdmin(request.systemAdmin());
        user.setMustChangePassword(true);
        user = userRepository.save(user);
        auditService.record(actor, "USER_CREATED", "USER", user.getId().toString(),
                jsonMapper.createObjectNode().put("username", user.getUsername())
                        .put("systemAdmin", user.isSystemAdmin()).toString());
        return toUserDto(user);
    }

    @Transactional
    public UserDto setEnabled(Long userId, boolean enabled) {
        AppUser actor = authorizationService.requireSystemAdmin();
        userRepository.lockEnabledSystemAdmins();
        AppUser user = userRepository.findByIdForUpdate(userId)
                .orElseThrow(() -> new NotFoundException("user not found"));
        if (!enabled && user.isSystemAdmin() && user.isEnabled()
                && userRepository.countBySystemAdminTrueAndEnabledTrue() <= 1) {
            throw new ForbiddenException("cannot disable the last system administrator");
        }
        user.setEnabled(enabled);
        user.setSecurityVersion(user.getSecurityVersion() + 1);
        user.setUpdatedAt(Instant.now());
        sessionRevocationService.expireUserSessions(user.getId());
        auditService.record(actor, enabled ? "USER_ENABLED" : "USER_DISABLED", "USER",
                userId.toString(), "{}");
        return toUserDto(user);
    }

    @Transactional(readOnly = true)
    public List<PermissionDto> permissions() {
        authorizationService.requireSystemAdmin();
        return permissionRepository.findAll().stream()
                .map(permission -> new PermissionDto(permission.getCode())).toList();
    }

    @Transactional(readOnly = true)
    public List<GroupDto> groups() {
        authorizationService.requireSystemAdmin();
        return groupRepository.findAll().stream().map(this::toGroupDto).toList();
    }

    @Transactional
    public GroupDto createGroup(CreateGroupRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        if (groupRepository.findByNameIgnoreCase(request.name().trim()).isPresent()) {
            throw new ForbiddenException("group name already exists");
        }
        NodeGroup group = new NodeGroup();
        group.setName(request.name().trim());
        group.setDescription(trimToNull(request.description()));
        group = groupRepository.save(group);
        createDefaultRole(group, "Viewer", Set.of(Permission.NODE_VIEW, Permission.TELEMETRY_VIEW));
        createDefaultRole(group, "Operator",
                Set.of(Permission.NODE_VIEW, Permission.NODE_CONTROL, Permission.TELEMETRY_VIEW));
        auditService.record(actor, "GROUP_CREATED", "GROUP", group.getId().toString(), "{}");
        return toGroupDto(group);
    }

    @Transactional
    public RoleDto createRole(Long groupId, SaveRoleRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        NodeGroup group = requireGroup(groupId);
        if (roleRepository.findByGroupIdAndNameIgnoreCase(groupId, request.name().trim()).isPresent()) {
            throw new ForbiddenException("role name already exists in this group");
        }
        GroupRole role = new GroupRole();
        role.setGroup(group);
        role.setName(request.name().trim());
        role.setPermissions(resolvePermissions(request.permissions()));
        role = roleRepository.save(role);
        auditService.record(actor, "ROLE_CREATED", "ROLE", role.getId().toString(), "{}");
        return toRoleDto(role);
    }

    @Transactional
    public RoleDto updateRole(Long groupId, Long roleId, SaveRoleRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        GroupRole role = requireRole(groupId, roleId);
        roleRepository.findByGroupIdAndNameIgnoreCase(groupId, request.name().trim())
                .filter(other -> !other.getId().equals(roleId))
                .ifPresent(other -> { throw new ForbiddenException("role name already exists in this group"); });
        role.setName(request.name().trim());
        role.setPermissions(resolvePermissions(request.permissions()));
        role.setUpdatedAt(Instant.now());
        auditService.record(actor, "ROLE_UPDATED", "ROLE", roleId.toString(), "{}");
        return toRoleDto(role);
    }

    @Transactional
    public void assignRole(Long groupId, Long userId, Long roleId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        AppUser user = userRepository.findById(userId)
                .orElseThrow(() -> new NotFoundException("user not found"));
        NodeGroup group = requireGroup(groupId);
        GroupRole role = requireRole(groupId, roleId);
        GroupMembership membership = membershipRepository.findByUserIdAndGroupId(userId, groupId)
                .orElseGet(GroupMembership::new);
        membership.setUser(user);
        membership.setGroup(group);
        membership.setRole(role);
        membershipRepository.save(membership);
        auditService.record(actor, "GROUP_ROLE_ASSIGNED", "USER", userId.toString(),
                jsonMapper.createObjectNode().put("groupId", groupId).put("roleId", roleId).toString());
    }

    @Transactional
    public void removeMember(Long groupId, Long userId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        GroupMembership membership = membershipRepository.findByUserIdAndGroupId(userId, groupId)
                .orElseThrow(() -> new NotFoundException("group membership not found"));
        membershipRepository.delete(membership);
        auditService.record(actor, "GROUP_MEMBER_REMOVED", "USER", userId.toString(),
                jsonMapper.createObjectNode().put("groupId", groupId).toString());
    }

    @Transactional(readOnly = true)
    public List<NodeApprovalDto> nodes(ApprovalStatus status) {
        authorizationService.requireSystemAdmin();
        return nodeRepository.findByApprovalStatusOrderByCreatedAtAsc(status).stream()
                .map(this::toNodeApprovalDto).toList();
    }

    @Transactional
    public NodeApprovalDto approveNode(String nodeId, Set<Long> groupIds) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Node node = requireNode(nodeId);
        replaceNodeGroups(node, groupIds);
        node.setApprovalStatus(ApprovalStatus.APPROVED);
        node.setApprovedAt(Instant.now());
        node.setApprovedBy(actor);
        auditService.record(actor, "NODE_APPROVED", "NODE", nodeId,
                jsonMapper.createObjectNode().putPOJO("groupIds", groupIds).toString());
        return toNodeApprovalDto(node);
    }

    @Transactional
    public NodeApprovalDto setNodeGroups(String nodeId, Set<Long> groupIds) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Node node = requireNode(nodeId);
        if (node.getApprovalStatus() != ApprovalStatus.APPROVED) {
            throw new ForbiddenException("node must be approved first");
        }
        replaceNodeGroups(node, groupIds);
        auditService.record(actor, "NODE_GROUPS_UPDATED", "NODE", nodeId,
                jsonMapper.createObjectNode().putPOJO("groupIds", groupIds).toString());
        return toNodeApprovalDto(node);
    }

    @Transactional
    public NodeApprovalDto rejectNode(String nodeId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Node node = requireNode(nodeId);
        nodeMembershipRepository.deleteByNodeId(node.getId());
        node.setApprovalStatus(ApprovalStatus.REJECTED);
        node.setApprovedAt(null);
        node.setApprovedBy(null);
        auditService.record(actor, "NODE_REJECTED", "NODE", nodeId, "{}");
        return toNodeApprovalDto(node);
    }

    private void replaceNodeGroups(Node node, Set<Long> groupIds) {
        if (groupIds == null || groupIds.isEmpty()) {
            throw new ForbiddenException("at least one group is required");
        }
        List<NodeGroup> groups = groupRepository.findAllById(groupIds);
        if (groups.size() != groupIds.size()) {
            throw new NotFoundException("one or more groups do not exist");
        }
        nodeMembershipRepository.deleteByNodeId(node.getId());
        nodeMembershipRepository.flush();
        for (NodeGroup group : groups) {
            NodeGroupMembership membership = new NodeGroupMembership();
            membership.setNode(node);
            membership.setGroup(group);
            nodeMembershipRepository.save(membership);
        }
    }

    private void createDefaultRole(NodeGroup group, String name, Set<String> codes) {
        GroupRole role = new GroupRole();
        role.setGroup(group);
        role.setName(name);
        role.setPermissions(resolvePermissions(codes));
        roleRepository.save(role);
    }

    private Set<Permission> resolvePermissions(Set<String> codes) {
        Set<Permission> permissions = new HashSet<>(permissionRepository.findAllById(codes));
        if (permissions.size() != codes.size()) {
            throw new ForbiddenException("unknown permission code");
        }
        return permissions;
    }

    private NodeGroup requireGroup(Long id) {
        return groupRepository.findById(id).orElseThrow(() -> new NotFoundException("group not found"));
    }

    private GroupRole requireRole(Long groupId, Long roleId) {
        GroupRole role = roleRepository.findWithPermissionsById(roleId)
                .orElseThrow(() -> new NotFoundException("role not found"));
        if (!role.getGroup().getId().equals(groupId)) {
            throw new NotFoundException("role not found");
        }
        return role;
    }

    private Node requireNode(String nodeId) {
        return nodeRepository.findByNodeId(nodeId)
                .orElseThrow(() -> new NotFoundException("node not found"));
    }

    private GroupDto toGroupDto(NodeGroup group) {
        List<RoleDto> roles = roleRepository.findByGroupIdOrderByName(group.getId()).stream()
                .map(AdminService::toRoleDto).toList();
        List<MemberDto> members = membershipRepository.findByGroupIdOrderByUserUsername(group.getId()).stream()
                .map(m -> new MemberDto(m.getUser().getId(), m.getUser().getUsername(),
                        m.getUser().getDisplayName(), m.getRole().getId(), m.getRole().getName()))
                .toList();
        List<String> nodeIds = nodeMembershipRepository.findByGroupIdOrderByNodeNodeId(group.getId()).stream()
                .map(m -> m.getNode().getNodeId()).toList();
        return new GroupDto(group.getId(), group.getName(), group.getDescription(), roles, members, nodeIds);
    }

    private NodeApprovalDto toNodeApprovalDto(Node node) {
        return new NodeApprovalDto(node.getNodeId(), node.getRoom(), node.getFwVersion(), node.getIp(),
                node.isOnline(), node.getCreatedAt(), node.getApprovalStatus().name(),
                nodeMembershipRepository.findGroupIdsByNodeId(node.getNodeId()));
    }

    private static UserDto toUserDto(AppUser user) {
        return new UserDto(user.getId(), user.getUsername(), user.getDisplayName(), user.isEnabled(),
                user.isSystemAdmin(), user.isMustChangePassword(), user.getCreatedAt());
    }

    private static RoleDto toRoleDto(GroupRole role) {
        return new RoleDto(role.getId(), role.getName(), role.getPermissions().stream()
                .map(Permission::getCode).collect(java.util.stream.Collectors.toUnmodifiableSet()));
    }

    private static String trimToNull(String value) {
        return value == null || value.isBlank() ? null : value.trim();
    }
}
