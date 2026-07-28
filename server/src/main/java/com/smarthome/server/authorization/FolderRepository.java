package com.smarthome.server.authorization;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface FolderRepository extends JpaRepository<Folder, Long> {
    Optional<Folder> findByParentIdAndNameIgnoreCase(Long parentId, String name);

    @Query("select f from Folder f where f.parent is null and lower(f.name) = lower(:name)")
    Optional<Folder> findRootByNameIgnoreCase(String name);

    @Query("select f from Folder f order by f.sortOrder, f.name, f.id")
    List<Folder> findAllOrdered();
}
