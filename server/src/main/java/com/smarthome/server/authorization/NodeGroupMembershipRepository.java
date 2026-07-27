package com.smarthome.server.authorization;

import java.util.List;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface NodeGroupMembershipRepository extends JpaRepository<NodeGroupMembership, Long> {

    @EntityGraph(attributePaths = "group")
    List<NodeGroupMembership> findByNodeId(Long nodeId);

    @EntityGraph(attributePaths = "node")
    List<NodeGroupMembership> findByGroupIdOrderByNodeNodeId(Long groupId);

    void deleteByNodeId(Long nodeId);

    @Query("""
            select distinct p.code
            from NodeGroupMembership ngm, GroupMembership gm
            join gm.role.permissions p
            where ngm.node.nodeId = :nodeId
              and ngm.node.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
              and gm.group = ngm.group
              and gm.user.id = :userId
              and gm.user.enabled = true
              and gm.user.mustChangePassword = false
            """)
    List<String> findPermissionCodes(Long userId, String nodeId);

    @Query("""
            select count(ngm) > 0
            from NodeGroupMembership ngm, GroupMembership gm
            join gm.role.permissions p
            where ngm.node.nodeId = :nodeId
              and ngm.node.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
              and gm.group = ngm.group
              and gm.user.id = :userId
              and gm.user.enabled = true
              and gm.user.mustChangePassword = false
              and p.code = :permission
            """)
    boolean hasPermission(Long userId, String nodeId, String permission);

    @Query("select distinct ngm.group.id from NodeGroupMembership ngm where ngm.node.nodeId = :nodeId")
    List<Long> findGroupIdsByNodeId(String nodeId);

    @Query("""
            select distinct gm.user.username
            from NodeGroupMembership ngm, GroupMembership gm
            join gm.role.permissions p
            where ngm.node.nodeId = :nodeId
              and ngm.node.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
              and gm.group = ngm.group
              and gm.user.enabled = true
              and gm.user.mustChangePassword = false
              and p.code = :permission
            """)
    List<String> findAuthorizedUsernames(String nodeId, String permission);
}
