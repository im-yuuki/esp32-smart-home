package com.smarthome.server.account;

import java.util.List;

import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.security.web.csrf.CsrfToken;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import com.smarthome.server.authorization.AuthorizationService;
import com.smarthome.server.common.ApiResponse;

import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;
import lombok.RequiredArgsConstructor;

@RestController
@RequestMapping("/api/v1/auth")
@RequiredArgsConstructor
public class AuthController {

    private final AuthorizationService authorizationService;
    private final AccountService accountService;

    @GetMapping("/csrf")
    public ApiResponse<CsrfDto> csrf(CsrfToken csrfToken) {
        return ApiResponse.ok(new CsrfDto(csrfToken.getToken(), csrfToken.getHeaderName()));
    }

    @GetMapping("/me")
    public ApiResponse<MeDto> me(@AuthenticationPrincipal Object ignored) {
        AppUser user = authorizationService.currentUser();
        List<FolderMembershipDto> folders = authorizationService.foldersFor(user).stream()
                .map(folder -> new FolderMembershipDto(folder.id(), folder.name(), folder.roleName()))
                .toList();
        return ApiResponse.ok(new MeDto(user.getId(), user.getUsername(), user.getDisplayName(),
                user.isSystemAdmin(), user.isMustChangePassword(),
                authorizationService.hasAnyAuditAccess(user), folders));
    }

    @PostMapping("/change-password")
    public ApiResponse<Void> changePassword(@Valid @RequestBody ChangePasswordRequest request) {
        accountService.changeOwnPassword(request.currentPassword(), request.newPassword());
        return ApiResponse.ok(null);
    }

    public record CsrfDto(String token, String headerName) {}
    public record FolderMembershipDto(Long id, String name, String roleName) {}
    public record MeDto(Long id, String username, String displayName, boolean systemAdmin,
                         boolean mustChangePassword, boolean auditAccess,
                          List<FolderMembershipDto> folders) {}
    public record ChangePasswordRequest(@NotBlank String currentPassword,
                                        @NotBlank @Size(min = 12, max = 200) String newPassword) {}
}
