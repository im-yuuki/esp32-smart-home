package com.smarthome.server.device;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface NodeRepository extends JpaRepository<Node, Long> {

    Optional<Node> findByNodeId(String nodeId);

    Optional<Node> findByNodeIdAndApprovalStatus(String nodeId, ApprovalStatus approvalStatus);

    @EntityGraph(attributePaths = "capabilities")
    @Query("select n from Node n order by n.room, n.nodeId")
    List<Node> findAllWithCapabilities();

    @EntityGraph(attributePaths = "capabilities")
    @Query("""
            select distinct n from Node n
            where n.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
            order by n.room, n.nodeId
            """)
    List<Node> findAllApprovedWithCapabilities();

    @EntityGraph(attributePaths = "capabilities")
    @Query("""
            select distinct n from Node n
            where n.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
              and exists (
                select ngm.id
                from NodeGroupMembership ngm, GroupMembership gm
                join gm.role.permissions p
                where ngm.node = n
                  and gm.group = ngm.group
                  and gm.user.id = :userId
                  and gm.user.enabled = true
                  and p.code = 'NODE_VIEW'
              )
            order by n.room, n.nodeId
            """)
    List<Node> findAuthorizedWithCapabilities(Long userId);

    @EntityGraph(attributePaths = "capabilities")
    Optional<Node> findWithCapabilitiesByNodeId(String nodeId);

    @EntityGraph(attributePaths = "capabilities")
    @Query("""
            select n from Node n
            where n.nodeId = :nodeId
              and n.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
            """)
    Optional<Node> findApprovedWithCapabilitiesByNodeId(String nodeId);

    boolean existsByNodeIdAndApprovalStatus(String nodeId, ApprovalStatus approvalStatus);

    List<Node> findByApprovalStatusOrderByCreatedAtAsc(ApprovalStatus approvalStatus);
}
