package com.smarthome.server.security;

import java.io.IOException;

import org.springframework.http.MediaType;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.AuthenticationException;
import org.springframework.security.web.authentication.UsernamePasswordAuthenticationFilter;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import tools.jackson.databind.json.JsonMapper;

public class JsonLoginFilter extends UsernamePasswordAuthenticationFilter {

    private final JsonMapper jsonMapper;

    public JsonLoginFilter(JsonMapper jsonMapper, AuthenticationManager authenticationManager) {
        this.jsonMapper = jsonMapper;
        setAuthenticationManager(authenticationManager);
        setFilterProcessesUrl("/api/v1/auth/login");
    }

    @Override
    public Authentication attemptAuthentication(HttpServletRequest request,
                                                HttpServletResponse response)
            throws AuthenticationException {
        MediaType contentType = request.getContentType() == null
                ? null : MediaType.parseMediaType(request.getContentType());
        if (contentType == null || !MediaType.APPLICATION_JSON.isCompatibleWith(contentType)) {
            return super.attemptAuthentication(request, response);
        }
        try {
            LoginBody body = jsonMapper.readValue(request.getInputStream(), LoginBody.class);
            String username = body.username() == null ? "" : body.username().trim();
            String password = body.password() == null ? "" : body.password();
            UsernamePasswordAuthenticationToken token =
                    UsernamePasswordAuthenticationToken.unauthenticated(username, password);
            setDetails(request, token);
            return getAuthenticationManager().authenticate(token);
        } catch (IOException e) {
            throw new InvalidLoginRequestException("invalid login request", e);
        }
    }

    private record LoginBody(String username, String password) {
    }
}
