package com.smarthome.server.account;

import java.time.Instant;

import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.authorization.AuthorizationService;
import com.smarthome.server.common.ForbiddenException;
import com.smarthome.server.security.SessionRevocationService;

import lombok.RequiredArgsConstructor;

@Service
@RequiredArgsConstructor
public class AccountService {

    private final AppUserRepository userRepository;
    private final AuthorizationService authorizationService;
    private final PasswordEncoder passwordEncoder;
    private final SessionRevocationService sessionRevocationService;

    @Transactional
    public void changeOwnPassword(String currentPassword, String newPassword) {
        AppUser current = authorizationService.currentUser();
        AppUser user = userRepository.findByIdForUpdate(current.getId()).orElseThrow();
        if (!passwordEncoder.matches(currentPassword, user.getPasswordHash())) {
            throw new ForbiddenException("current password is incorrect");
        }
        if (passwordEncoder.matches(newPassword, user.getPasswordHash())) {
            throw new ForbiddenException("new password must be different");
        }
        user.setPasswordHash(passwordEncoder.encode(newPassword));
        user.setMustChangePassword(false);
        user.setSecurityVersion(user.getSecurityVersion() + 1);
        user.setUpdatedAt(Instant.now());
        sessionRevocationService.expireUserSessions(user.getId());
    }
}
