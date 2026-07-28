package com.smarthome.server.device;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface NodeRepository extends JpaRepository<Node, Long> {

    Optional<Node> findByNodeId(String nodeId);

    @Query("select n.folder.id from Node n where n.nodeId = :nodeId and n.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED")
    Optional<Long> findFolderIdByNodeId(String nodeId);

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
                 select fm.id
                 from FolderMembership fm
                 join fm.role.permissions p, FolderClosure fc
                 where fc.descendant = n.folder and fc.ancestor = fm.folder
                   and fm.user.id = :userId and fm.user.enabled = true
                   and fm.user.mustChangePassword = false and p.code = 'NODE_VIEW'
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

    long countByFolderId(Long folderId);

    @EntityGraph(attributePaths = {"capabilities", "capabilities.deviceType", "capabilities.tags", "folder"})
    @Query("select distinct n from Node n where n.folder.id in :folderIds and n.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED order by n.nodeId")
    List<Node> findApprovedByFolderIdsWithDetails(List<Long> folderIds);
}
