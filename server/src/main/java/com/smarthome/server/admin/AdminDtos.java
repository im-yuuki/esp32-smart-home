package com.smarthome.server.admin;

import java.time.Instant;
import java.util.List;
import java.util.Set;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotEmpty;
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

    public record MemberDto(Long userId, String username, String displayName,
                            Long roleId, String roleName) {}

    public record GroupDto(Long id, String name, String description,
                           List<RoleDto> roles, List<MemberDto> members, List<String> nodeIds) {}

    public record CreateGroupRequest(@NotBlank @Size(max = 120) String name,
                                     @Size(max = 500) String description) {}

    public record AssignRoleRequest(Long roleId) {}

    public record NodeApprovalDto(String nodeId, String room, String fwVersion, String ip,
                                  boolean online, Instant createdAt, String approvalStatus,
                                  List<Long> groupIds) {}

    public record SetNodeGroupsRequest(@NotEmpty Set<Long> groupIds) {}
}
