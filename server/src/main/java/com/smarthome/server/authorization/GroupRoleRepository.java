package com.smarthome.server.authorization;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface GroupRoleRepository extends JpaRepository<GroupRole, Long> {

    @EntityGraph(attributePaths = "permissions")
    List<GroupRole> findByGroupIdOrderByName(Long groupId);

    @EntityGraph(attributePaths = "permissions")
    @Query("select r from GroupRole r where r.id = :id")
    Optional<GroupRole> findWithPermissionsById(Long id);

    Optional<GroupRole> findByGroupIdAndNameIgnoreCase(Long groupId, String name);
}
