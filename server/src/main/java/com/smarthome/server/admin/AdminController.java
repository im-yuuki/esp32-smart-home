package com.smarthome.server.admin;

import static com.smarthome.server.admin.AdminDtos.*;

import java.util.List;

import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
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

    @GetMapping("/users") public ApiResponse<List<UserDto>> users() { return ApiResponse.ok(adminService.users()); }
    @PostMapping("/users") public ApiResponse<UserDto> createUser(@Valid @RequestBody CreateUserRequest r) { return ApiResponse.ok(adminService.createUser(r)); }
    @PatchMapping("/users/{id}/enabled") public ApiResponse<UserDto> enabled(@PathVariable Long id, @RequestBody SetEnabledRequest r) { return ApiResponse.ok(adminService.setEnabled(id, r.enabled())); }
    @GetMapping("/permissions") public ApiResponse<List<PermissionDto>> permissions() { return ApiResponse.ok(adminService.permissions()); }

    @GetMapping("/folders") public ApiResponse<List<AdminFolderDto>> folders() { return ApiResponse.ok(adminService.folders()); }
    @PostMapping("/folders") public ApiResponse<AdminFolderDto> createFolder(@Valid @RequestBody SaveFolderRequest r) { return ApiResponse.ok(adminService.createFolder(r)); }
    @PutMapping("/folders/{id}") public ApiResponse<AdminFolderDto> updateFolder(@PathVariable Long id, @Valid @RequestBody SaveFolderRequest r) { return ApiResponse.ok(adminService.updateFolder(id, r)); }
    @PostMapping("/folders/{id}/move") public ApiResponse<AdminFolderDto> moveFolder(@PathVariable Long id, @RequestBody MoveFolderRequest r) { return ApiResponse.ok(adminService.moveFolder(id, r.parentId())); }
    @DeleteMapping("/folders/{id}") public ApiResponse<Void> deleteFolder(@PathVariable Long id) { adminService.deleteFolder(id); return ApiResponse.ok(null); }
    @PutMapping("/folders/{id}/template") public ApiResponse<AdminFolderDto> template(@PathVariable Long id, @RequestBody TemplateRequest r) { return ApiResponse.ok(adminService.updateTemplate(id, r)); }
    @PutMapping("/folders/{id}/placements") public ApiResponse<Void> placements(@PathVariable Long id, @RequestBody List<PlacementRequest> r) { adminService.replacePlacements(id, r); return ApiResponse.ok(null); }
    @PutMapping("/folders/{id}/placements/{capabilityId}") public ApiResponse<Void> placement(@PathVariable Long id, @PathVariable Long capabilityId, @RequestBody PlacementRequest r) { adminService.upsertPlacement(id, capabilityId, r); return ApiResponse.ok(null); }
    @PostMapping("/folders/{id}/roles") public ApiResponse<RoleDto> createRole(@PathVariable Long id, @Valid @RequestBody SaveRoleRequest r) { return ApiResponse.ok(adminService.createRole(id, r)); }
    @PutMapping("/folders/{id}/roles/{roleId}") public ApiResponse<RoleDto> updateRole(@PathVariable Long id, @PathVariable Long roleId, @Valid @RequestBody SaveRoleRequest r) { return ApiResponse.ok(adminService.updateRole(id, roleId, r)); }
    @PutMapping("/folders/{id}/members/{userId}") public ApiResponse<Void> assign(@PathVariable Long id, @PathVariable Long userId, @Valid @RequestBody AssignRoleRequest r) { adminService.assignRole(id, userId, r.roleId()); return ApiResponse.ok(null); }
    @DeleteMapping("/folders/{id}/members/{userId}") public ApiResponse<Void> remove(@PathVariable Long id, @PathVariable Long userId) { adminService.removeMember(id, userId); return ApiResponse.ok(null); }

    @GetMapping("/nodes") public ApiResponse<List<NodeApprovalDto>> nodes(@RequestParam(defaultValue = "PENDING") ApprovalStatus status) { return ApiResponse.ok(adminService.nodes(status)); }
    @PostMapping("/nodes/{nodeId}/approve") public ApiResponse<NodeApprovalDto> approve(@PathVariable String nodeId, @Valid @RequestBody ApproveNodeRequest r) { return ApiResponse.ok(adminService.approveNode(nodeId, r.folderId())); }
    @PostMapping("/nodes/{nodeId}/reject") public ApiResponse<NodeApprovalDto> reject(@PathVariable String nodeId) { return ApiResponse.ok(adminService.rejectNode(nodeId)); }
    @PutMapping("/nodes/{nodeId}/folder") public ApiResponse<NodeApprovalDto> setFolder(@PathVariable String nodeId, @Valid @RequestBody SetNodeFolderRequest r) { return ApiResponse.ok(adminService.setNodeFolder(nodeId, r.folderId())); }
    @PatchMapping("/nodes/{nodeId}/display-name") public ApiResponse<NodeApprovalDto> nodeName(@PathVariable String nodeId, @RequestBody DisplayNameRequest r) { return ApiResponse.ok(adminService.updateNodeDisplayName(nodeId, r.displayName())); }
    @PatchMapping("/capabilities/{id}") public ApiResponse<Void> capability(@PathVariable Long id, @RequestBody CapabilityMetadataRequest r) { adminService.updateCapability(id, r); return ApiResponse.ok(null); }
    @PostMapping("/device-types") public ApiResponse<Long> deviceType(@Valid @RequestBody DeviceTypeRequest r) { return ApiResponse.ok(adminService.createDeviceType(r)); }
    @PostMapping("/tags") public ApiResponse<Long> tag(@Valid @RequestBody TagRequest r) { return ApiResponse.ok(adminService.createTag(r)); }
}
