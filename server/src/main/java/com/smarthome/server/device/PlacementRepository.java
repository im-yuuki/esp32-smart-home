package com.smarthome.server.device;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;

public interface PlacementRepository extends JpaRepository<Placement, Long> {
    @EntityGraph(attributePaths = "capability")
    List<Placement> findByFolderIdOrderBySortOrderAscIdAsc(Long folderId);
    Optional<Placement> findByFolderIdAndCapabilityId(Long folderId, Long capabilityId);
    void deleteByFolderId(Long folderId);
}
