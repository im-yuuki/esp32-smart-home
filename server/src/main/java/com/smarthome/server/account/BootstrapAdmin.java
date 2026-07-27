package com.smarthome.server.account;

import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

import com.smarthome.server.config.SecurityProperties;

import lombok.RequiredArgsConstructor;

@Component
@RequiredArgsConstructor
public class BootstrapAdmin implements ApplicationRunner {

    private final AppUserRepository userRepository;
    private final PasswordEncoder passwordEncoder;
    private final SecurityProperties properties;

    @Override
    @Transactional
    public void run(ApplicationArguments args) {
        if (userRepository.existsBySystemAdminTrueAndEnabledTrue()) {
            return;
        }
        SecurityProperties.BootstrapAdmin bootstrap = properties.bootstrapAdmin();
        String username = bootstrap != null ? bootstrap.username() : null;
        String password = bootstrap != null ? bootstrap.password() : null;
        if (username == null || username.isBlank() || password == null || password.length() < 12) {
            throw new IllegalStateException(
                    "set BOOTSTRAP_ADMIN_USERNAME and a BOOTSTRAP_ADMIN_PASSWORD of at least 12 characters");
        }
        if (userRepository.count() != 0) {
            throw new IllegalStateException("no enabled system admin exists; restore one explicitly in the database");
        }
        AppUser user = new AppUser();
        user.setUsername(username.trim());
        user.setDisplayName(bootstrap.displayName() == null || bootstrap.displayName().isBlank()
                ? "System Administrator" : bootstrap.displayName().trim());
        user.setPasswordHash(passwordEncoder.encode(password));
        user.setEnabled(true);
        user.setSystemAdmin(true);
        user.setMustChangePassword(true);
        userRepository.save(user);
    }
}
