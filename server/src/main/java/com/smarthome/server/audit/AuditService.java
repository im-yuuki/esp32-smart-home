package com.smarthome.server.audit;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.transaction.annotation.Propagation;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageRequest;
import org.springframework.data.domain.Sort;
import org.springframework.data.jpa.domain.Specification;

import java.time.Instant;
import java.util.List;

import com.smarthome.server.authorization.AuthorizationService;
import com.smarthome.server.authorization.FolderRepository;
import com.smarthome.server.authorization.FolderClosureRepository;
import com.smarthome.server.authorization.Permission;
import com.smarthome.server.device.Capability;

import jakarta.persistence.criteria.Predicate;
import jakarta.servlet.http.HttpServletRequest;

import com.smarthome.server.account.AppUser;

import lombok.RequiredArgsConstructor;

@Service
@RequiredArgsConstructor
public class AuditService {

    private final AuditLogRepository auditLogRepository;
    private final AuthorizationService authorizationService;
    private final FolderRepository folderRepository;
    private final FolderClosureRepository folderClosureRepository;
    private final HttpServletRequest request;

    @Transactional
    public void record(AppUser actor, String action, String targetType, String targetId, String details) {
        save(actor, action, targetType, targetId, details, null, null, null, null);
    }

    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void recordControl(AppUser actor, String action, String targetId, String details,
                              String correlationId, String batchId, Capability capability) {
        save(actor, action, "NODE", targetId, details, correlationId, batchId, capability,
                capability.getNode().getFolder().getId());
    }

    @Transactional(readOnly = true)
    public Page<AuditLogDto> search(Long folderId, boolean includeDescendants, String actor,
                                    String node, String action, Instant from, Instant to,
                                    int page, int size) {
        AppUser user = authorizationService.requireReadyUser();
        List<Long> permittedFolders = user.isSystemAdmin() ? List.of() : folderRepository.findAll().stream()
                .filter(folder -> authorizationService.permissionsForFolder(user, folder.getId())
                        .contains(Permission.AUDIT_VIEW))
                .map(folder -> folder.getId()).toList();
        if (!user.isSystemAdmin() && permittedFolders.isEmpty()) {
            authorizationService.requireFolderPermission(user, folderId == null ? -1 : folderId,
                    Permission.AUDIT_VIEW);
        }
        List<Long> selectedFolders = null;
        if (folderId != null) {
            authorizationService.requireFolderPermission(user, folderId, Permission.AUDIT_VIEW);
            if (!folderRepository.existsById(folderId)) {
                throw new com.smarthome.server.common.NotFoundException("folder not found");
            }
            selectedFolders = includeDescendants
                    ? folderClosureRepository.findDescendantIds(folderId) : List.of(folderId);
        }
        List<Long> folderFilter = selectedFolders;
        Specification<AuditLog> specification = (root, query, cb) -> {
            java.util.ArrayList<Predicate> predicates = new java.util.ArrayList<>();
            if (!user.isSystemAdmin()) {
                predicates.add(root.get("folder").get("id").in(permittedFolders));
            }
            if (folderFilter != null) {
                predicates.add(root.get("folder").get("id").in(folderFilter));
            }
            if (actor != null && !actor.isBlank()) {
                String term = actor.trim().toLowerCase(java.util.Locale.ROOT);
                Predicate identity = term.chars().allMatch(Character::isDigit)
                        ? cb.equal(root.get("actor").get("id"), Long.valueOf(term))
                        : cb.disjunction();
                predicates.add(cb.or(identity,
                        cb.like(cb.lower(root.get("actor").get("username")), "%" + term + "%"),
                        cb.like(cb.lower(root.get("actor").get("displayName")), "%" + term + "%")));
            }
            if (node != null && !node.isBlank()) predicates.add(cb.equal(root.get("targetId"), node));
            if (action != null && !action.isBlank()) predicates.add(cb.equal(root.get("action"), action));
            if (from != null) predicates.add(cb.greaterThanOrEqualTo(root.get("createdAt"), from));
            if (to != null) predicates.add(cb.lessThanOrEqualTo(root.get("createdAt"), to));
            return cb.and(predicates.toArray(Predicate[]::new));
        };
        return auditLogRepository.findAll(specification, PageRequest.of(Math.max(page, 0),
                        Math.min(Math.max(size, 1), 200), Sort.by(Sort.Direction.DESC, "createdAt")))
                .map(AuditService::toDto);
    }

    private void save(AppUser actor, String action, String targetType, String targetId, String details,
                      String correlationId, String batchId, Capability capability, Long folderId) {
        AuditLog log = new AuditLog();
        log.setActor(actor);
        log.setAction(action);
        log.setTargetType(targetType);
        log.setTargetId(targetId);
        log.setDetails(details == null ? "{}" : details);
        log.setCorrelationId(correlationId);
        log.setBatchId(batchId);
        log.setCapability(capability);
        if (folderId != null) log.setFolder(folderRepository.getReferenceById(folderId));
        try {
            log.setIp(request.getRemoteAddr());
        } catch (IllegalStateException ignored) {
            // MQTT/background processing has no servlet request.
        }
        auditLogRepository.saveAndFlush(log);
    }

    private static AuditLogDto toDto(AuditLog log) {
        return new AuditLogDto(log.getId(), log.getActor() == null ? null : log.getActor().getId(),
                log.getActor() == null ? null : log.getActor().getDisplayName(),
                log.getAction(), log.getTargetType(), log.getTargetId(), log.getDetails(),
                log.getCorrelationId(), log.getBatchId(), log.getIp(),
                log.getCapability() == null ? null : log.getCapability().getId(),
                log.getFolder() == null ? null : log.getFolder().getId(), log.getCreatedAt());
    }

    public record AuditLogDto(Long id, Long actorUserId, String actorName, String action, String targetType,
                              String targetId, @com.fasterxml.jackson.annotation.JsonRawValue String details,
                              String correlationId, String batchId,
                              String ip, Long capabilityId, Long folderId, Instant createdAt) {}
}
