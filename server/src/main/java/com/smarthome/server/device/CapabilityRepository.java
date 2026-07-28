package com.smarthome.server.device;

import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.EntityGraph;

public interface CapabilityRepository extends JpaRepository<Capability, Long> {

    Optional<Capability> findByNodeAndTypeAndChannel(Node node, String type, int channel);

    @EntityGraph(attributePaths = {"node", "node.folder", "deviceType", "tags"})
    Optional<Capability> findDetailedById(Long id);
}
