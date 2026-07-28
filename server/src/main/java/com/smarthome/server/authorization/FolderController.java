package com.smarthome.server.authorization;

import static com.smarthome.server.authorization.FolderDtos.*;

import java.util.List;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.ResponseStatus;
import org.springframework.web.bind.annotation.RestController;

import com.smarthome.server.common.ApiResponse;
import com.smarthome.server.common.dto.CapabilityDto.DeviceTypeDto;
import com.smarthome.server.common.dto.CapabilityDto.TagDto;

import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;

@RestController
@RequestMapping("/api/v1")
@RequiredArgsConstructor
public class FolderController {
    private final FolderService folderService;

    @GetMapping("/folders")
    public ApiResponse<List<FolderDto>> folders() { return ApiResponse.ok(folderService.folders()); }

    @GetMapping("/folders/{id}/map")
    public ApiResponse<FolderMapDto> map(@PathVariable Long id) {
        return ApiResponse.ok(folderService.map(id));
    }

    @GetMapping("/device-types")
    public ApiResponse<List<DeviceTypeDto>> deviceTypes() {
        return ApiResponse.ok(folderService.deviceTypes());
    }

    @GetMapping("/tags")
    public ApiResponse<List<TagDto>> tags() { return ApiResponse.ok(folderService.tags()); }

    @PostMapping("/folders/{id}/bulk-actions/preview")
    public ApiResponse<BulkPreviewDto> preview(@PathVariable Long id,
                                               @Valid @RequestBody BulkActionRequest request) {
        return ApiResponse.ok(folderService.preview(id, request));
    }

    @PostMapping("/folders/{id}/bulk-actions")
    @ResponseStatus(HttpStatus.ACCEPTED)
    public ApiResponse<BulkResultDto> execute(@PathVariable Long id,
                                              @Valid @RequestBody BulkActionRequest request) {
        return ApiResponse.ok(folderService.execute(id, request));
    }
}
