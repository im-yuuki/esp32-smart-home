package com.smarthome.server.admin;

import java.time.Instant;
import java.util.List;
import java.util.Set;

import com.fasterxml.jackson.annotation.JsonRawValue;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotEmpty;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Size;

public final class AdminDtos {
    private AdminDtos() {}

    public record UserDto(Long id, String username, String displayName, boolean enabled,
                          boolean systemAdmin, boolean mustChangePassword, Instant createdAt) {}
    public record CreateUserRequest(@NotBlank @Size(max = 100) String username,
                                    @NotBlank @Size(max = 200) String displayName,
                                    @NotBlank @Size(min = 12, max = 200) String temporaryPassword,
                                    boolean systemAdmin) {}
    public record SetEnabledRequest(boolean enabled) {}
    public record PermissionDto(String code) {}

    public record RoleDto(Long id, String name, Set<String> permissions) {}
    public record SaveRoleRequest(@NotBlank @Size(max = 100) String name,
                                  @NotEmpty Set<String> permissions) {}
    public record AssignRoleRequest(@NotNull Long roleId) {}
    public record MemberDto(Long userId, String username, String displayName,
                            Long roleId, String roleName) {}
    public record AdminFolderDto(Long id, Long parentId, String name, String templateType,
                                 @JsonRawValue String templateConfig, int sortOrder,
                                 List<RoleDto> roles, List<MemberDto> members,
                                 List<String> nodeIds) {}
    public record SaveFolderRequest(Long parentId, @NotBlank String name, int sortOrder) {}
    public record MoveFolderRequest(Long parentId) {}
    public record TemplateRequest(String templateType, String templateConfig) {}

    public record NodeApprovalDto(String nodeId, String discoveryName, String displayName,
                                  String room, String fwVersion, String ip, boolean online,
                                  Instant createdAt, String approvalStatus, Long folderId) {}
    public record ApproveNodeRequest(@NotNull Long folderId) {}
    public record SetNodeFolderRequest(@NotNull Long folderId) {}
    public record DisplayNameRequest(String displayName) {}
    public record CapabilityMetadataRequest(String displayName, Long deviceTypeId, Set<Long> tagIds) {}

    public record PlacementRequest(Long id, Long capabilityId, String label, double x, double y,
                                   double width, double height, int sortOrder, String config) {}
    public record DeviceTypeRequest(@NotBlank String name, String description) {}
    public record TagRequest(@NotBlank String name, String color) {}
}
