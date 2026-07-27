package com.smarthome.server.security;

import org.springframework.security.core.AuthenticationException;

public class InvalidLoginRequestException extends AuthenticationException {
    public InvalidLoginRequestException(String message, Throwable cause) {
        super(message, cause);
    }
}
