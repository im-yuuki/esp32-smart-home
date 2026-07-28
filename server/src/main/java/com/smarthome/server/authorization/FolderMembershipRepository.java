package com.smarthome.server.authorization;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface FolderMembershipRepository extends JpaRepository<FolderMembership, Long> {
    @EntityGraph(attributePaths = {"folder", "role"})
    @Query("select m from FolderMembership m where m.user.id = :userId order by m.folder.name")
    List<FolderMembership> findByUserIdOrderByFolderName(Long userId);

    @EntityGraph(attributePaths = {"user", "role"})
    List<FolderMembership> findByFolderIdOrderByUserUsername(Long folderId);

    Optional<FolderMembership> findByUserIdAndFolderId(Long userId, Long folderId);

    @Query("""
            select distinct p.code from FolderMembership m
            join m.role.permissions p, FolderClosure c
            where m.user.id = :userId and m.user.enabled = true
              and m.user.mustChangePassword = false
              and c.ancestor = m.folder and c.descendant.id = :folderId
            """)
    List<String> findPermissionCodes(Long userId, Long folderId);

    @Query("""
            select count(m) > 0 from FolderMembership m
            join m.role.permissions p, FolderClosure c
            where m.user.id = :userId and m.user.enabled = true
              and m.user.mustChangePassword = false
              and c.ancestor = m.folder and c.descendant.id = :folderId
              and p.code = :permission
            """)
    boolean hasPermission(Long userId, Long folderId, String permission);

    @Query("""
            select distinct m.user.username from FolderMembership m
            join m.role.permissions p, FolderClosure c, Node n
            where n.nodeId = :nodeId and n.approvalStatus = com.smarthome.server.device.ApprovalStatus.APPROVED
              and c.descendant = n.folder and c.ancestor = m.folder
              and m.user.enabled = true and m.user.mustChangePassword = false
              and p.code = :permission
            """)
    List<String> findAuthorizedUsernames(String nodeId, String permission);
}
