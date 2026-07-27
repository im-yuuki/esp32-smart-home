package com.smarthome.server.security;

import org.springframework.security.core.session.SessionRegistry;
import org.springframework.stereotype.Service;

import lombok.RequiredArgsConstructor;

@Service
@RequiredArgsConstructor
public class SessionRevocationService {

    private final SessionRegistry sessionRegistry;

    public void expireUserSessions(Long userId) {
        for (Object principal : sessionRegistry.getAllPrincipals()) {
            if (principal instanceof SessionPrincipal sessionPrincipal
                    && sessionPrincipal.userId().equals(userId)) {
                sessionRegistry.getAllSessions(principal, false).forEach(session -> session.expireNow());
            }
        }
    }
}
