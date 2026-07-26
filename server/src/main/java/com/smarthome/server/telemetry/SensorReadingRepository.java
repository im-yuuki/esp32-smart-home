package com.smarthome.server.telemetry;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;

import com.smarthome.server.device.Node;

public interface SensorReadingRepository extends JpaRepository<SensorReading, Long> {

    Optional<SensorReading> findFirstByNodeOrderByTsDesc(Node node);

    List<SensorReading> findByNodeAndTsBetween(Node node, Instant from, Instant to, Pageable pageable);
}
