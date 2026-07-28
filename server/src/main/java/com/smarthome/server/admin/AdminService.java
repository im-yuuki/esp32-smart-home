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
import com.smarthome.server.authorization.Folder;
import com.smarthome.server.authorization.FolderClosureRepository;
import com.smarthome.server.authorization.FolderMembership;
import com.smarthome.server.authorization.FolderMembershipRepository;
import com.smarthome.server.authorization.FolderRepository;
import com.smarthome.server.authorization.FolderRole;
import com.smarthome.server.authorization.FolderRoleRepository;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.authorization.PermissionRepository;
import com.smarthome.server.common.ForbiddenException;
import com.smarthome.server.common.NotFoundException;
import com.smarthome.server.common.UnicodeNames;
import com.smarthome.server.device.ApprovalStatus;
import com.smarthome.server.device.Capability;
import com.smarthome.server.device.CapabilityRepository;
import com.smarthome.server.device.DeviceType;
import com.smarthome.server.device.DeviceTypeRepository;
import com.smarthome.server.device.Node;
import com.smarthome.server.device.NodeRepository;
import com.smarthome.server.device.Placement;
import com.smarthome.server.device.PlacementRepository;
import com.smarthome.server.device.Tag;
import com.smarthome.server.device.TagRepository;
import com.smarthome.server.security.SessionRevocationService;

import lombok.RequiredArgsConstructor;
import tools.jackson.databind.json.JsonMapper;

@Service
@RequiredArgsConstructor
public class AdminService {
    private final AuthorizationService authorizationService;
    private final AppUserRepository userRepository;
    private final FolderRepository folderRepository;
    private final FolderClosureRepository closureRepository;
    private final FolderRoleRepository roleRepository;
    private final FolderMembershipRepository membershipRepository;
    private final PermissionRepository permissionRepository;
    private final NodeRepository nodeRepository;
    private final CapabilityRepository capabilityRepository;
    private final DeviceTypeRepository deviceTypeRepository;
    private final TagRepository tagRepository;
    private final PlacementRepository placementRepository;
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
        if (userRepository.findByUsernameIgnoreCase(request.username().trim()).isPresent())
            throw new ForbiddenException("username already exists");
        AppUser user = new AppUser();
        user.setUsername(request.username().trim());
        user.setDisplayName(UnicodeNames.normalize(request.displayName(), "displayName"));
        user.setPasswordHash(passwordEncoder.encode(request.temporaryPassword()));
        user.setEnabled(true);
        user.setSystemAdmin(request.systemAdmin());
        user.setMustChangePassword(true);
        user = userRepository.save(user);
        auditService.record(actor, "USER_CREATED", "USER", user.getId().toString(), "{}");
        return toUserDto(user);
    }

    @Transactional
    public UserDto setEnabled(Long userId, boolean enabled) {
        AppUser actor = authorizationService.requireSystemAdmin();
        userRepository.lockEnabledSystemAdmins();
        AppUser user = userRepository.findByIdForUpdate(userId)
                .orElseThrow(() -> new NotFoundException("user not found"));
        if (!enabled && user.isSystemAdmin() && user.isEnabled()
                && userRepository.countBySystemAdminTrueAndEnabledTrue() <= 1)
            throw new ForbiddenException("cannot disable the last system administrator");
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
        return permissionRepository.findAll().stream().map(p -> new PermissionDto(p.getCode())).toList();
    }

    @Transactional(readOnly = true)
    public List<AdminFolderDto> folders() {
        authorizationService.requireSystemAdmin();
        return folderRepository.findAllOrdered().stream().map(this::toFolderDto).toList();
    }

    @Transactional
    public AdminFolderDto createFolder(SaveFolderRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder parent = request.parentId() == null ? null : requireFolder(request.parentId());
        String name = UnicodeNames.normalize(request.name(), "folder name");
        ensureUniqueSibling(null, parent, name);
        Folder folder = new Folder();
        folder.setParent(parent);
        folder.setName(name);
        folder.setSortOrder(request.sortOrder());
        folder = folderRepository.saveAndFlush(folder);
        closureRepository.insertSelf(folder.getId());
        if (parent != null) closureRepository.attachSubtree(folder.getId(), parent.getId());
        createDefaultRole(folder, "Viewer", Set.of(Permission.NODE_VIEW, Permission.TELEMETRY_VIEW));
        createDefaultRole(folder, "Operator", Set.of(Permission.NODE_VIEW, Permission.NODE_CONTROL,
                Permission.TELEMETRY_VIEW));
        auditService.record(actor, "FOLDER_CREATED", "FOLDER", folder.getId().toString(), "{}");
        return toFolderDto(folder);
    }

    @Transactional
    public AdminFolderDto updateFolder(Long id, SaveFolderRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(id);
        Folder parent = request.parentId() == null ? null : requireFolder(request.parentId());
        move(folder, parent);
        String name = UnicodeNames.normalize(request.name(), "folder name");
        ensureUniqueSibling(id, parent, name);
        folder.setName(name);
        folder.setSortOrder(request.sortOrder());
        folder.setUpdatedAt(Instant.now());
        auditService.record(actor, "FOLDER_UPDATED", "FOLDER", id.toString(), "{}");
        return toFolderDto(folder);
    }

    @Transactional
    public AdminFolderDto moveFolder(Long id, Long parentId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(id);
        move(folder, parentId == null ? null : requireFolder(parentId));
        auditService.record(actor, "FOLDER_MOVED", "FOLDER", id.toString(), "{}");
        return toFolderDto(folder);
    }

    @Transactional
    public void deleteFolder(Long id) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(id);
        if (nodeRepository.countByFolderId(id) > 0 || closureRepository.findDescendantIds(id).size() > 1)
            throw new ForbiddenException("folder must be empty before deletion");
        folderRepository.delete(folder);
        auditService.record(actor, "FOLDER_DELETED", "FOLDER", id.toString(), "{}");
    }

    @Transactional
    public AdminFolderDto updateTemplate(Long id, TemplateRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(id);
        String templateType = request.templateType() == null ? null
                : UnicodeNames.normalize(request.templateType(), "templateType").toUpperCase(java.util.Locale.ROOT);
        if (templateType != null && !Set.of("OUTDOOR", "BUILDING", "FLOOR", "CORRIDOR", "ROOM")
                .contains(templateType)) {
            throw new ForbiddenException("unknown folder template type");
        }
        folder.setTemplateType(templateType);
        folder.setTemplateConfig(validJson(request.templateConfig()));
        auditService.record(actor, "FOLDER_TEMPLATE_UPDATED", "FOLDER", id.toString(), "{}");
        return toFolderDto(folder);
    }

    @Transactional
    public void replacePlacements(Long folderId, List<PlacementRequest> requests) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(folderId);
        placementRepository.deleteByFolderId(folderId);
        placementRepository.flush();
        for (PlacementRequest request : requests) {
            Capability capability = request.capabilityId() == null ? null
                    : capabilityRepository.findDetailedById(request.capabilityId())
                            .orElseThrow(() -> new NotFoundException("capability not found"));
            if (capability != null && !capability.getNode().getFolder().getId().equals(folderId))
                throw new ForbiddenException("placement capability must belong to the folder");
            validatePlacement(request);
            Placement placement = new Placement();
            placement.setFolder(folder);
            placement.setCapability(capability);
            placement.setLabel(request.label() == null ? null
                    : UnicodeNames.normalize(request.label(), "placement label"));
            placement.setX(request.x()); placement.setY(request.y());
            placement.setWidth(request.width()); placement.setHeight(request.height());
            placement.setSortOrder(request.sortOrder()); placement.setConfig(validJson(request.config()));
            placementRepository.save(placement);
        }
        auditService.record(actor, "PLACEMENTS_UPDATED", "FOLDER", folderId.toString(), "{}");
    }

    @Transactional
    public void upsertPlacement(Long folderId, Long capabilityId, PlacementRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(folderId);
        Capability capability = capabilityRepository.findDetailedById(capabilityId)
                .orElseThrow(() -> new NotFoundException("capability not found"));
        if (!capability.getNode().getFolder().getId().equals(folderId))
            throw new ForbiddenException("placement capability must belong to the folder");
        validatePlacement(request);
        Placement placement = placementRepository.findByFolderIdAndCapabilityId(folderId, capabilityId)
                .orElseGet(Placement::new);
        placement.setFolder(folder);
        placement.setCapability(capability);
        placement.setLabel(request.label() == null || request.label().isBlank() ? null
                : UnicodeNames.normalize(request.label(), "placement label"));
        placement.setX(request.x());
        placement.setY(request.y());
        placement.setWidth(request.width());
        placement.setHeight(request.height());
        placement.setSortOrder(request.sortOrder());
        placement.setConfig(validJson(request.config()));
        placementRepository.save(placement);
        auditService.record(actor, "PLACEMENT_UPDATED", "CAPABILITY", capabilityId.toString(),
                jsonMapper.createObjectNode().put("folderId", folderId).toString());
    }

    @Transactional
    public RoleDto createRole(Long folderId, SaveRoleRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Folder folder = requireFolder(folderId);
        if (roleRepository.findByFolderIdAndNameIgnoreCase(folderId, request.name().trim()).isPresent())
            throw new ForbiddenException("role name already exists in this folder");
        FolderRole role = new FolderRole(); role.setFolder(folder);
        role.setName(UnicodeNames.normalize(request.name(), "role name"));
        role.setPermissions(resolvePermissions(request.permissions()));
        role = roleRepository.save(role);
        auditService.record(actor, "FOLDER_ROLE_CREATED", "ROLE", role.getId().toString(), "{}");
        return toRoleDto(role);
    }

    @Transactional
    public RoleDto updateRole(Long folderId, Long roleId, SaveRoleRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        FolderRole role = requireRole(folderId, roleId);
        role.setName(UnicodeNames.normalize(request.name(), "role name"));
        role.setPermissions(resolvePermissions(request.permissions()));
        role.setUpdatedAt(Instant.now());
        auditService.record(actor, "FOLDER_ROLE_UPDATED", "ROLE", roleId.toString(), "{}");
        return toRoleDto(role);
    }

    @Transactional
    public void assignRole(Long folderId, Long userId, Long roleId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        AppUser user = userRepository.findById(userId).orElseThrow(() -> new NotFoundException("user not found"));
        Folder folder = requireFolder(folderId); FolderRole role = requireRole(folderId, roleId);
        FolderMembership membership = membershipRepository.findByUserIdAndFolderId(userId, folderId)
                .orElseGet(FolderMembership::new);
        membership.setUser(user); membership.setFolder(folder); membership.setRole(role);
        membershipRepository.save(membership);
        auditService.record(actor, "FOLDER_ROLE_ASSIGNED", "USER", userId.toString(), "{}");
    }

    @Transactional
    public void removeMember(Long folderId, Long userId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        FolderMembership membership = membershipRepository.findByUserIdAndFolderId(userId, folderId)
                .orElseThrow(() -> new NotFoundException("folder membership not found"));
        membershipRepository.delete(membership);
        auditService.record(actor, "FOLDER_MEMBER_REMOVED", "USER", userId.toString(), "{}");
    }

    @Transactional(readOnly = true)
    public List<NodeApprovalDto> nodes(ApprovalStatus status) {
        authorizationService.requireSystemAdmin();
        return nodeRepository.findByApprovalStatusOrderByCreatedAtAsc(status).stream()
                .map(AdminService::toNodeDto).toList();
    }

    @Transactional
    public NodeApprovalDto approveNode(String nodeId, Long folderId) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Node node = requireNode(nodeId); node.setFolder(requireFolder(folderId));
        node.setApprovalStatus(ApprovalStatus.APPROVED); node.setApprovedAt(Instant.now());
        node.setApprovedBy(actor);
        auditService.record(actor, "NODE_APPROVED", "NODE", nodeId,
                jsonMapper.createObjectNode().put("folderId", folderId).toString());
        return toNodeDto(node);
    }

    @Transactional
    public NodeApprovalDto setNodeFolder(String nodeId, Long folderId) {
        AppUser actor = authorizationService.requireSystemAdmin(); Node node = requireNode(nodeId);
        if (node.getApprovalStatus() != ApprovalStatus.APPROVED)
            throw new ForbiddenException("node must be approved first");
        node.setFolder(requireFolder(folderId));
        auditService.record(actor, "NODE_FOLDER_UPDATED", "NODE", nodeId, "{}");
        return toNodeDto(node);
    }

    @Transactional
    public NodeApprovalDto rejectNode(String nodeId) {
        AppUser actor = authorizationService.requireSystemAdmin(); Node node = requireNode(nodeId);
        node.setApprovalStatus(ApprovalStatus.REJECTED); node.setApprovedAt(null); node.setApprovedBy(null);
        auditService.record(actor, "NODE_REJECTED", "NODE", nodeId, "{}"); return toNodeDto(node);
    }

    @Transactional
    public NodeApprovalDto updateNodeDisplayName(String nodeId, String displayName) {
        AppUser actor = authorizationService.requireSystemAdmin(); Node node = requireNode(nodeId);
        node.setDisplayName(normalizeOptional(displayName, "node displayName"));
        auditService.record(actor, "NODE_DISPLAY_NAME_UPDATED", "NODE", nodeId, "{}");
        return toNodeDto(node);
    }

    @Transactional
    public void updateCapability(Long id, CapabilityMetadataRequest request) {
        AppUser actor = authorizationService.requireSystemAdmin();
        Capability capability = capabilityRepository.findDetailedById(id)
                .orElseThrow(() -> new NotFoundException("capability not found"));
        capability.setDisplayName(normalizeOptional(request.displayName(), "capability displayName"));
        capability.setDeviceType(request.deviceTypeId() == null ? null : deviceTypeRepository
                .findById(request.deviceTypeId()).orElseThrow(() -> new NotFoundException("device type not found")));
        Set<Long> ids = request.tagIds() == null ? Set.of() : request.tagIds();
        List<Tag> tags = tagRepository.findAllById(ids);
        if (tags.size() != ids.size()) throw new NotFoundException("one or more tags not found");
        capability.setTags(new HashSet<>(tags));
        auditService.record(actor, "CAPABILITY_METADATA_UPDATED", "CAPABILITY", id.toString(), "{}");
    }

    @Transactional
    public Long createDeviceType(DeviceTypeRequest request) {
        authorizationService.requireSystemAdmin(); DeviceType type = new DeviceType();
        type.setName(UnicodeNames.normalize(request.name(), "device type name"));
        type.setDescription(request.description()); return deviceTypeRepository.save(type).getId();
    }

    @Transactional
    public Long createTag(TagRequest request) {
        authorizationService.requireSystemAdmin(); Tag tag = new Tag();
        tag.setName(UnicodeNames.normalize(request.name(), "tag name")); tag.setColor(request.color());
        return tagRepository.save(tag).getId();
    }

    private void move(Folder folder, Folder parent) {
        if (parent != null && (parent.getId().equals(folder.getId())
                || closureRepository.existsByAncestorIdAndDescendantId(folder.getId(), parent.getId())))
            throw new ForbiddenException("folder move would create a cycle");
        closureRepository.detachSubtree(folder.getId());
        folder.setParent(parent);
        if (parent != null) closureRepository.attachSubtree(folder.getId(), parent.getId());
    }

    private void ensureUniqueSibling(Long id, Folder parent, String name) {
        if (parent == null) folderRepository.findRootByNameIgnoreCase(name)
                .filter(other -> !other.getId().equals(id)).ifPresent(other -> { throw new ForbiddenException("folder name already exists"); });
        else folderRepository.findByParentIdAndNameIgnoreCase(parent.getId(), name)
                .filter(other -> !other.getId().equals(id)).ifPresent(other -> { throw new ForbiddenException("folder name already exists"); });
    }

    private void createDefaultRole(Folder folder, String name, Set<String> codes) {
        FolderRole role = new FolderRole(); role.setFolder(folder); role.setName(name);
        role.setPermissions(resolvePermissions(codes)); roleRepository.save(role);
    }
    private Set<Permission> resolvePermissions(Set<String> codes) {
        Set<Permission> permissions = new HashSet<>(permissionRepository.findAllById(codes));
        if (permissions.size() != codes.size()) throw new ForbiddenException("unknown permission code");
        return permissions;
    }
    private Folder requireFolder(Long id) { return folderRepository.findById(id)
            .orElseThrow(() -> new NotFoundException("folder not found")); }
    private FolderRole requireRole(Long folderId, Long roleId) {
        FolderRole role = roleRepository.findWithPermissionsById(roleId)
                .orElseThrow(() -> new NotFoundException("role not found"));
        if (!role.getFolder().getId().equals(folderId)) throw new NotFoundException("role not found");
        return role;
    }
    private Node requireNode(String nodeId) { return nodeRepository.findByNodeId(nodeId)
            .orElseThrow(() -> new NotFoundException("node not found")); }
    private String validJson(String value) {
        String json = value == null || value.isBlank() ? "{}" : value;
        jsonMapper.readTree(json); return json;
    }
    private static void validatePlacement(PlacementRequest request) {
        if (!Double.isFinite(request.x()) || !Double.isFinite(request.y())
                || request.x() < 0 || request.x() > 100 || request.y() < 0 || request.y() > 100
                || !Double.isFinite(request.width()) || !Double.isFinite(request.height())
                || request.width() <= 0 || request.height() <= 0) {
            throw new ForbiddenException("placement coordinates or size are invalid");
        }
    }
    private static String normalizeOptional(String value, String field) {
        return value == null || value.isBlank() ? null : UnicodeNames.normalize(value, field);
    }
    private AdminFolderDto toFolderDto(Folder folder) {
        List<RoleDto> roles = roleRepository.findByFolderIdOrderByName(folder.getId()).stream()
                .map(AdminService::toRoleDto).toList();
        List<MemberDto> members = membershipRepository.findByFolderIdOrderByUserUsername(folder.getId()).stream()
                .map(m -> new MemberDto(m.getUser().getId(), m.getUser().getUsername(),
                        m.getUser().getDisplayName(), m.getRole().getId(), m.getRole().getName())).toList();
        List<String> nodes = nodeRepository.findApprovedByFolderIdsWithDetails(List.of(folder.getId())).stream()
                .map(Node::getNodeId).toList();
        return new AdminFolderDto(folder.getId(), folder.getParent() == null ? null : folder.getParent().getId(),
                folder.getName(), folder.getTemplateType(), folder.getTemplateConfig(), folder.getSortOrder(),
                roles, members, nodes);
    }
    private static RoleDto toRoleDto(FolderRole role) { return new RoleDto(role.getId(), role.getName(),
            role.getPermissions().stream().map(Permission::getCode).collect(java.util.stream.Collectors.toUnmodifiableSet())); }
    private static UserDto toUserDto(AppUser user) { return new UserDto(user.getId(), user.getUsername(),
            user.getDisplayName(), user.isEnabled(), user.isSystemAdmin(), user.isMustChangePassword(), user.getCreatedAt()); }
    private static NodeApprovalDto toNodeDto(Node node) { return new NodeApprovalDto(node.getNodeId(),
            node.getDiscoveryName(), node.getDisplayName(), node.getRoom(), node.getFwVersion(), node.getIp(),
            node.isOnline(), node.getCreatedAt(), node.getApprovalStatus().name(), node.getFolder().getId()); }
}
