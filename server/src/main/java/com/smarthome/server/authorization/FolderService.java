package com.smarthome.server.authorization;

import static com.smarthome.server.authorization.FolderDtos.*;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.UUID;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.account.AppUser;
import com.smarthome.server.audit.AuditService;
import com.smarthome.server.common.NotFoundException;
import com.smarthome.server.device.Capability;
import com.smarthome.server.device.DeviceTypeRepository;
import com.smarthome.server.device.Node;
import com.smarthome.server.device.NodeRepository;
import com.smarthome.server.device.NodeService;
import com.smarthome.server.device.PlacementRepository;
import com.smarthome.server.device.TagRepository;
import com.smarthome.server.mqtt.MqttGateway;

import lombok.RequiredArgsConstructor;
import tools.jackson.databind.json.JsonMapper;

@Service
@RequiredArgsConstructor
public class FolderService {
    private final FolderRepository folderRepository;
    private final FolderClosureRepository closureRepository;
    private final PlacementRepository placementRepository;
    private final DeviceTypeRepository deviceTypeRepository;
    private final TagRepository tagRepository;
    private final NodeRepository nodeRepository;
    private final BulkOperationRepository bulkOperationRepository;
    private final AuthorizationService authorizationService;
    private final NodeService nodeService;
    private final AuditService auditService;
    private final MqttGateway mqttGateway;
    private final JsonMapper jsonMapper;

    @Transactional(readOnly = true)
    public List<FolderDto> folders() {
        AppUser user = authorizationService.requireReadyUser();
        List<Folder> folders = folderRepository.findAllOrdered();
        if (user.isSystemAdmin()) {
            return folders.stream().map(folder -> toDto(folder,
                    authorizationService.permissionsForFolder(user, folder.getId()))).toList();
        }
        java.util.Map<Long, Set<String>> permissions = new HashMap<>();
        Set<Long> visible = new HashSet<>();
        for (Folder folder : folders) {
            Set<String> effective = authorizationService.permissionsForFolder(user, folder.getId());
            permissions.put(folder.getId(), effective);
            if (!effective.isEmpty()) {
                visible.add(folder.getId());
                visible.addAll(closureRepository.findAncestorIds(folder.getId()));
            }
        }
        return folders.stream().filter(folder -> visible.contains(folder.getId()))
                .map(folder -> toDto(folder, permissions.getOrDefault(folder.getId(), Set.of())))
                .toList();
    }

    @Transactional(readOnly = true)
    public FolderMapDto map(Long folderId) {
        AppUser user = authorizationService.requireReadyUser();
        authorizationService.requireFolderPermission(user, folderId, Permission.NODE_VIEW);
        Folder folder = requireFolder(folderId);
        List<Node> nodes = nodeRepository.findApprovedByFolderIdsWithDetails(List.of(folderId));
        return new FolderMapDto(toDto(folder, authorizationService.permissionsForFolder(user, folderId)),
                nodes.stream().map(node -> nodeService.toDto(node, user)).toList(),
                placementRepository.findByFolderIdOrderBySortOrderAscIdAsc(folderId).stream()
                        .filter(p -> p.getCapability() == null || nodes.stream().anyMatch(n ->
                                n.getCapabilities().contains(p.getCapability())))
                        .map(p -> new PlacementDto(p.getId(), p.getCapability() == null ? null
                                : p.getCapability().getId(), p.getLabel(), p.getX(), p.getY(),
                                p.getWidth(), p.getHeight(), p.getSortOrder(), p.getConfig())).toList());
    }

    @Transactional(readOnly = true)
    public List<com.smarthome.server.common.dto.CapabilityDto.DeviceTypeDto> deviceTypes() {
        authorizationService.requireReadyUser();
        return deviceTypeRepository.findAllByOrderByNameAsc().stream()
                .map(type -> new com.smarthome.server.common.dto.CapabilityDto.DeviceTypeDto(
                        type.getId(), type.getName(), type.getDescription())).toList();
    }

    @Transactional(readOnly = true)
    public List<com.smarthome.server.common.dto.CapabilityDto.TagDto> tags() {
        authorizationService.requireReadyUser();
        return tagRepository.findAllByOrderByNameAsc().stream()
                .map(tag -> new com.smarthome.server.common.dto.CapabilityDto.TagDto(
                        tag.getId(), tag.getName(), tag.getColor())).toList();
    }

    @Transactional(readOnly = true)
    public BulkPreviewDto preview(Long folderId, BulkActionRequest request) {
        AppUser user = authorizationService.requireReadyUser();
        requireFolder(folderId);
        List<Target> targets = selectTargets(folderId, request, user);
        List<BulkTargetDto> results = targets.stream().map(target -> target.dto(
                target.node().isOnline() ? "READY" : "SKIPPED_OFFLINE")).toList();
        int dispatchable = (int) targets.stream().filter(target -> target.node().isOnline()).count();
        return new BulkPreviewDto(targets.size(), dispatchable, targets.size() - dispatchable, results);
    }

    @Transactional
    public BulkResultDto execute(Long folderId, BulkActionRequest request) {
        AppUser user = authorizationService.requireReadyUser();
        String key = request.idempotencyKey().trim();
        var existing = bulkOperationRepository.findByActorIdAndIdempotencyKey(user.getId(), key);
        if (existing.isPresent() && existing.get().getResponse() != null) {
            return jsonMapper.readValue(existing.get().getResponse(), BulkResultDto.class);
        }
        Folder folder = requireFolder(folderId);
        String batchId = UUID.randomUUID().toString();
        BulkOperation operation = new BulkOperation();
        operation.setActor(user);
        operation.setFolder(folder);
        operation.setBatchId(batchId);
        operation.setIdempotencyKey(key);
        bulkOperationRepository.saveAndFlush(operation);

        List<Target> targets = selectTargets(folderId, request, user);
        List<BulkTargetDto> results = new ArrayList<>();
        int dispatched = 0;
        int skipped = 0;
        int failed = 0;
        for (Target target : targets) {
            if (!target.node().isOnline()) {
                auditService.recordControl(user, "CONTROL_SKIPPED_OFFLINE", target.node().getNodeId(),
                        jsonMapper.createObjectNode().put("channel", target.capability().getChannel())
                                .put("state", request.state().name()).toString(),
                        UUID.randomUUID().toString(), batchId, target.capability());
                results.add(target.dto("SKIPPED_OFFLINE"));
                skipped++;
                continue;
            }
            String correlationId = UUID.randomUUID().toString();
            String state = request.state().name();
            String details = jsonMapper.createObjectNode().put("channel", target.capability().getChannel())
                    .put("state", state).toString();
            auditService.recordControl(user, "CONTROL_REQUESTED", target.node().getNodeId(), details,
                    correlationId, batchId, target.capability());
            try {
                mqttGateway.publish("home/%s/%s/relay/%d/set".formatted(target.node().getRoom(),
                        target.node().getNodeId(), target.capability().getChannel()),
                        "{\"state\":\"%s\"}".formatted(state));
                auditService.recordControl(user, "CONTROL_DISPATCHED", target.node().getNodeId(),
                        details, correlationId, batchId, target.capability());
                results.add(target.dto("DISPATCHED"));
                dispatched++;
            } catch (RuntimeException exception) {
                auditService.recordControl(user, "CONTROL_FAILED", target.node().getNodeId(), details,
                        correlationId, batchId, target.capability());
                results.add(target.dto("FAILED"));
                failed++;
            }
        }
        BulkResultDto result = new BulkResultDto(batchId, targets.size(), dispatched, skipped, failed,
                List.copyOf(results));
        operation.setResponse(jsonMapper.writeValueAsString(result));
        return result;
    }

    List<Target> selectTargets(Long folderId, BulkActionRequest request, AppUser user) {
        List<Long> folders = request.includeDescendants()
                ? closureRepository.findDescendantIds(folderId) : List.of(folderId);
        Set<Long> deviceTypes = request.deviceTypeIds() == null ? Set.of() : request.deviceTypeIds();
        Set<Long> tags = request.tagIds() == null ? Set.of() : request.tagIds();
        TagMatch match = request.tagMatch() == null ? TagMatch.ANY : request.tagMatch();
        List<Target> result = new ArrayList<>();
        for (Node node : nodeRepository.findApprovedByFolderIdsWithDetails(folders)) {
            if (!authorizationService.permissionsForFolder(user, node.getFolder().getId())
                    .contains(Permission.NODE_CONTROL)) continue;
            for (Capability capability : node.getCapabilities()) {
                if (!"relay".equals(capability.getType())) continue;
                if (!deviceTypes.isEmpty() && (capability.getDeviceType() == null
                        || !deviceTypes.contains(capability.getDeviceType().getId()))) continue;
                Set<Long> capabilityTags = capability.getTags().stream().map(tag -> tag.getId())
                        .collect(java.util.stream.Collectors.toSet());
                if (!tags.isEmpty() && (match == TagMatch.ALL
                        ? !capabilityTags.containsAll(tags)
                        : java.util.Collections.disjoint(capabilityTags, tags))) continue;
                result.add(new Target(node, capability));
            }
        }
        result.sort(Comparator.comparing((Target target) -> target.node().getNodeId())
                .thenComparing(target -> target.capability().getId()));
        return result;
    }

    private Folder requireFolder(Long folderId) {
        return folderRepository.findById(folderId)
                .orElseThrow(() -> new NotFoundException("folder not found"));
    }

    private static FolderDto toDto(Folder folder, Set<String> permissions) {
        return new FolderDto(folder.getId(), folder.getParent() == null ? null : folder.getParent().getId(),
                folder.getName(), folder.getIcon(), folder.getTemplateType(), folder.getTemplateConfig(),
                folder.getSortOrder(), permissions);
    }

    record Target(Node node, Capability capability) {
        BulkTargetDto dto(String status) {
            return new BulkTargetDto(capability.getId(), node.getNodeId(), capability.getChannel(),
                    node.isOnline(), status);
        }
    }
}
