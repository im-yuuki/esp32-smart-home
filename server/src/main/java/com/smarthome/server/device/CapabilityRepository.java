package com.smarthome.server.device;

import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;

public interface CapabilityRepository extends JpaRepository<Capability, Long> {

    Optional<Capability> findByNodeAndTypeAndChannel(Node node, String type, int channel);
}
