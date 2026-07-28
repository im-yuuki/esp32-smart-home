package com.smarthome.server.audit;

import java.time.Instant;

import org.springframework.data.domain.Page;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.smarthome.server.common.ApiResponse;

import lombok.RequiredArgsConstructor;

@RestController
@RequestMapping("/api/v1/audit-logs")
@RequiredArgsConstructor
public class AuditController {
    private final AuditService auditService;

    @GetMapping
    public ApiResponse<Page<AuditService.AuditLogDto>> search(
            @RequestParam(required = false) Long folderId,
            @RequestParam(defaultValue = "false") boolean includeDescendants,
            @RequestParam(required = false) String actor,
            @RequestParam(required = false) String node,
            @RequestParam(required = false) String action,
            @RequestParam(required = false) Instant from,
            @RequestParam(required = false) Instant to,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "50") int size) {
        return ApiResponse.ok(auditService.search(folderId, includeDescendants, actor, node, action,
                from, to, page, size));
    }
}
