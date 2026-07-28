package com.smarthome.server.authorization;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;

public interface FolderRoleRepository extends JpaRepository<FolderRole, Long> {
    @EntityGraph(attributePaths = "permissions")
    Optional<FolderRole> findWithPermissionsById(Long id);
    Optional<FolderRole> findByFolderIdAndNameIgnoreCase(Long folderId, String name);
    List<FolderRole> findByFolderIdOrderByName(Long folderId);
}
