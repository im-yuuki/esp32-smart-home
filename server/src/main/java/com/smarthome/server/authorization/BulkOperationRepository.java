package com.smarthome.server.authorization;

import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;

public interface BulkOperationRepository extends JpaRepository<BulkOperation, Long> {
    Optional<BulkOperation> findByActorIdAndIdempotencyKey(Long actorId, String idempotencyKey);
}
