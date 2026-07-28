package com.smarthome.server.authorization;

import java.util.List;
import java.util.Set;

import com.fasterxml.jackson.annotation.JsonRawValue;
import com.smarthome.server.common.dto.CapabilityDto.DeviceTypeDto;
import com.smarthome.server.common.dto.CapabilityDto.TagDto;
import com.smarthome.server.common.dto.NodeDto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Size;

public final class FolderDtos {
    private FolderDtos() {}

    public record FolderDto(Long id, Long parentId, String name, String icon, String templateType,
                            @JsonRawValue String templateConfig, int sortOrder,
                            Set<String> permissions) {}

    public record PlacementDto(Long id, Long capabilityId, String label, double x, double y,
                               double width, double height, int sortOrder,
                               @JsonRawValue String config) {}

    public record FolderMapDto(FolderDto folder, List<NodeDto> nodes,
                               List<PlacementDto> placements) {}

    public enum TagMatch { ANY, ALL }
    public enum BulkState { ON, OFF }

    public record BulkActionRequest(boolean includeDescendants, Set<Long> deviceTypeIds,
                                    Set<Long> tagIds, TagMatch tagMatch,
                                    @NotNull BulkState state,
                                    @NotBlank @Size(max = 100) String idempotencyKey) {}

    public record BulkTargetDto(Long capabilityId, String nodeId, int channel, boolean online,
                                String status) {}
    public record BulkPreviewDto(int matched, int dispatchable, int skipped,
                                 List<BulkTargetDto> results) {}
    public record BulkResultDto(String batchId, int matched, int dispatched, int skipped,
                                int failed, List<BulkTargetDto> results) {}

    public record CatalogDto(List<DeviceTypeDto> deviceTypes, List<TagDto> tags) {}
}
