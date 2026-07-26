package com.smarthome.server.device;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.EntityGraph;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface NodeRepository extends JpaRepository<Node, Long> {

    Optional<Node> findByNodeId(String nodeId);

    @EntityGraph(attributePaths = "capabilities")
    @Query("select n from Node n order by n.room, n.nodeId")
    List<Node> findAllWithCapabilities();

    @EntityGraph(attributePaths = "capabilities")
    Optional<Node> findWithCapabilitiesByNodeId(String nodeId);
}
