package com.smarthome.server.telemetry;

import java.time.Duration;
import java.time.Instant;
import java.util.List;

import org.springframework.data.domain.PageRequest;
import org.springframework.data.domain.Sort;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.common.NotFoundException;
import com.smarthome.server.common.dto.SensorReadingDto;
import com.smarthome.server.account.AppUser;
import com.smarthome.server.authorization.AuthorizationService;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.device.Node;
import com.smarthome.server.device.NodeRepository;

import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;

@Service
@RequiredArgsConstructor
@Slf4j
public class TelemetryService {

    /** Phase 1 cap: raw history rows per request. */
    private static final int MAX_HISTORY_ROWS = 10_000;

    private final NodeRepository nodeRepository;
    private final SensorReadingRepository readingRepository;
    private final AuthorizationService authorizationService;

    /** @return false when the node is unknown — caller drops the reading. */
    @Transactional
    public boolean record(String nodeId, Double temperature, Double humidity, Instant ts) {
        return nodeRepository.findByNodeId(nodeId).map(node -> {
            SensorReading reading = new SensorReading();
            reading.setNode(node);
            reading.setTemperature(temperature);
            reading.setHumidity(humidity);
            reading.setTs(ts);
            readingRepository.save(reading);
            return true;
        }).orElse(false);
    }

    /** 404 if the node is unknown; {@code null} data when the node has no readings yet. */
    @Transactional(readOnly = true)
    public SensorReadingDto latest(String nodeId) {
        AppUser user = authorizationService.requireReadyUser();
        authorizationService.requireNodePermission(user, nodeId, Permission.TELEMETRY_VIEW);
        Node node = requireNode(nodeId);
        return readingRepository.findFirstByNodeOrderByTsDesc(node)
                .map(TelemetryService::toDto)
                .orElse(null);
    }

    /**
     * Raw rows ascending, {@code from} defaulting to now-24h and {@code to} to now, capped
     * at {@value #MAX_HISTORY_ROWS} rows. {@code bucket} is accepted-and-ignored (Phase 1
     * contract: raw only; aggregation arrives in Phase 2).
     */
    @Transactional(readOnly = true)
    public List<SensorReadingDto> history(String nodeId, Instant from, Instant to, String bucket) {
        AppUser user = authorizationService.requireReadyUser();
        authorizationService.requireNodePermission(user, nodeId, Permission.TELEMETRY_VIEW);
        Node node = requireNode(nodeId);
        if (bucket != null && !bucket.isBlank()) {
            log.info("history bucket='{}' accepted but ignored — Phase 1 returns raw rows", bucket);
        }
        Instant effectiveTo = to != null ? to : Instant.now();
        Instant effectiveFrom = from != null ? from : effectiveTo.minus(Duration.ofHours(24));
        return readingRepository
                .findByNodeAndTsBetween(node, effectiveFrom, effectiveTo,
                        PageRequest.of(0, MAX_HISTORY_ROWS, Sort.by(Sort.Direction.ASC, "ts")))
                .stream()
                .map(TelemetryService::toDto)
                .toList();
    }

    private Node requireNode(String nodeId) {
        return nodeRepository.findByNodeIdAndApprovalStatus(nodeId,
                        com.smarthome.server.device.ApprovalStatus.APPROVED)
                .orElseThrow(() -> new NotFoundException("node %s not found".formatted(nodeId)));
    }

    private static SensorReadingDto toDto(SensorReading reading) {
        return new SensorReadingDto(reading.getTemperature(), reading.getHumidity(), reading.getTs());
    }
}
