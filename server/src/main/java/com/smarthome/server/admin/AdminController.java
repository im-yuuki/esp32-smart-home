package com.smarthome.server.admin;

import static com.smarthome.server.admin.AdminDtos.*;

import java.util.List;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.PatchMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.PutMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.smarthome.server.common.ApiResponse;
import com.smarthome.server.device.ApprovalStatus;

import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;

@RestController
@RequestMapping("/api/v1/admin")
@RequiredArgsConstructor
public class AdminController {

    private final AdminService adminService;

    @GetMapping("/users")
    public ApiResponse<List<UserDto>> users() {
        return ApiResponse.ok(adminService.users());
    }

    @PostMapping("/users")
    public ApiResponse<UserDto> createUser(@Valid @RequestBody CreateUserRequest request) {
        return ApiResponse.ok(adminService.createUser(request));
    }

    @PatchMapping("/users/{userId}/enabled")
    public ApiResponse<UserDto> setEnabled(@PathVariable Long userId,
                                           @RequestBody SetEnabledRequest request) {
        return ApiResponse.ok(adminService.setEnabled(userId, request.enabled()));
    }

    @GetMapping("/permissions")
    public ApiResponse<List<PermissionDto>> permissions() {
        return ApiResponse.ok(adminService.permissions());
    }

    @GetMapping("/groups")
    public ApiResponse<List<GroupDto>> groups() {
        return ApiResponse.ok(adminService.groups());
    }

    @PostMapping("/groups")
    public ApiResponse<GroupDto> createGroup(@Valid @RequestBody CreateGroupRequest request) {
        return ApiResponse.ok(adminService.createGroup(request));
    }

    @PostMapping("/groups/{groupId}/roles")
    public ApiResponse<RoleDto> createRole(@PathVariable Long groupId,
                                           @Valid @RequestBody SaveRoleRequest request) {
        return ApiResponse.ok(adminService.createRole(groupId, request));
    }

    @PutMapping("/groups/{groupId}/roles/{roleId}")
    public ApiResponse<RoleDto> updateRole(@PathVariable Long groupId, @PathVariable Long roleId,
                                           @Valid @RequestBody SaveRoleRequest request) {
        return ApiResponse.ok(adminService.updateRole(groupId, roleId, request));
    }

    @PutMapping("/groups/{groupId}/members/{userId}")
    public ApiResponse<Void> assignRole(@PathVariable Long groupId, @PathVariable Long userId,
                                        @RequestBody AssignRoleRequest request) {
        adminService.assignRole(groupId, userId, request.roleId());
        return ApiResponse.ok(null);
    }

    @DeleteMapping("/groups/{groupId}/members/{userId}")
    public ApiResponse<Void> removeMember(@PathVariable Long groupId, @PathVariable Long userId) {
        adminService.removeMember(groupId, userId);
        return ApiResponse.ok(null);
    }

    @GetMapping("/nodes")
    public ApiResponse<List<NodeApprovalDto>> nodes(
            @RequestParam(defaultValue = "PENDING") ApprovalStatus status) {
        return ApiResponse.ok(adminService.nodes(status));
    }

    @PostMapping("/nodes/{nodeId}/approve")
    public ApiResponse<NodeApprovalDto> approveNode(@PathVariable String nodeId,
                                                    @Valid @RequestBody SetNodeGroupsRequest request) {
        return ApiResponse.ok(adminService.approveNode(nodeId, request.groupIds()));
    }

    @PostMapping("/nodes/{nodeId}/reject")
    public ApiResponse<NodeApprovalDto> rejectNode(@PathVariable String nodeId) {
        return ApiResponse.ok(adminService.rejectNode(nodeId));
    }

    @PutMapping("/nodes/{nodeId}/groups")
    public ApiResponse<NodeApprovalDto> setNodeGroups(@PathVariable String nodeId,
                                                      @Valid @RequestBody SetNodeGroupsRequest request) {
        return ApiResponse.ok(adminService.setNodeGroups(nodeId, request.groupIds()));
    }
}
