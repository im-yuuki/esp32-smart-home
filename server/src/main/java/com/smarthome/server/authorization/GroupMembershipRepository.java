package com.smarthome.server.authorization;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;

public interface GroupMembershipRepository extends JpaRepository<GroupMembership, Long> {

    @EntityGraph(attributePaths = {"group", "role", "role.permissions"})
    List<GroupMembership> findByUserIdOrderByGroupName(Long userId);

    Optional<GroupMembership> findByUserIdAndGroupId(Long userId, Long groupId);

    @EntityGraph(attributePaths = {"user", "role", "role.permissions"})
    List<GroupMembership> findByGroupIdOrderByUserUsername(Long groupId);
}
