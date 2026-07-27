package com.smarthome.server.authorization;

import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;

public interface NodeGroupRepository extends JpaRepository<NodeGroup, Long> {
    Optional<NodeGroup> findByNameIgnoreCase(String name);
}
