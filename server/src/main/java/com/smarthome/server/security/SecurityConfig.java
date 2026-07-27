package com.smarthome.server.security;

import java.io.IOException;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.config.annotation.authentication.configuration.AuthenticationConfiguration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.crypto.factory.PasswordEncoderFactories;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.UsernamePasswordAuthenticationFilter;
import org.springframework.security.web.authentication.session.CompositeSessionAuthenticationStrategy;
import org.springframework.security.web.context.HttpSessionSecurityContextRepository;
import org.springframework.security.web.authentication.session.ChangeSessionIdAuthenticationStrategy;
import org.springframework.security.web.authentication.session.RegisterSessionAuthenticationStrategy;
import org.springframework.security.core.session.SessionRegistry;
import org.springframework.security.core.session.SessionRegistryImpl;
import org.springframework.security.web.session.HttpSessionEventPublisher;

import com.smarthome.server.common.ApiResponse;

import jakarta.servlet.http.HttpServletResponse;
import tools.jackson.databind.json.JsonMapper;

@Configuration
public class SecurityConfig {

    @Bean
    PasswordEncoder passwordEncoder() {
        return PasswordEncoderFactories.createDelegatingPasswordEncoder();
    }

    @Bean
    AuthenticationManager authenticationManager(AuthenticationConfiguration configuration) throws Exception {
        return configuration.getAuthenticationManager();
    }

    @Bean
    SessionRegistry sessionRegistry() {
        return new SessionRegistryImpl();
    }

    @Bean
    HttpSessionEventPublisher httpSessionEventPublisher() {
        return new HttpSessionEventPublisher();
    }

    @Bean
    SecurityFilterChain securityFilterChain(HttpSecurity http,
                                            AuthenticationManager authenticationManager,
                                            SessionRegistry sessionRegistry,
                                            JsonMapper jsonMapper) throws Exception {
        JsonLoginFilter loginFilter = new JsonLoginFilter(jsonMapper, authenticationManager);
        loginFilter.setSecurityContextRepository(new HttpSessionSecurityContextRepository());
        loginFilter.setSessionAuthenticationStrategy(new CompositeSessionAuthenticationStrategy(
                java.util.List.of(new ChangeSessionIdAuthenticationStrategy(),
                        new RegisterSessionAuthenticationStrategy(sessionRegistry))));
        loginFilter.setAuthenticationSuccessHandler((request, response, authentication) -> {
            write(response, HttpStatus.OK, ApiResponse.ok(java.util.Map.of("authenticated", true)), jsonMapper);
        });
        loginFilter.setAuthenticationFailureHandler((request, response, exception) -> {
            write(response, HttpStatus.UNAUTHORIZED,
                    ApiResponse.error("UNAUTHORIZED", "invalid username or password"), jsonMapper);
        });

        http
                .csrf(csrf -> csrf
                        .spa()
                        .ignoringRequestMatchers("/ws/**"))
                .authorizeHttpRequests(authorize -> authorize
                        .requestMatchers("/actuator/health", "/api/v1/auth/csrf",
                                "/api/v1/auth/login", "/ws/**").permitAll()
                        .anyRequest().authenticated())
                .sessionManagement(session -> session
                        .sessionFixation(fixation -> fixation.changeSessionId())
                        .maximumSessions(-1)
                        .sessionRegistry(sessionRegistry))
                .exceptionHandling(errors -> errors
                        .authenticationEntryPoint((request, response, exception) ->
                                write(response, HttpStatus.UNAUTHORIZED,
                                        ApiResponse.error("UNAUTHORIZED", "authentication required"), jsonMapper))
                        .accessDeniedHandler((request, response, exception) ->
                                write(response, HttpStatus.FORBIDDEN,
                                        ApiResponse.error("FORBIDDEN", "access denied"), jsonMapper)))
                .logout(logout -> logout
                        .logoutUrl("/api/v1/auth/logout")
                        .logoutSuccessHandler((request, response, authentication) ->
                                write(response, HttpStatus.OK,
                                        ApiResponse.ok(java.util.Map.of("loggedOut", true)), jsonMapper)))
                .addFilterAt(loginFilter, UsernamePasswordAuthenticationFilter.class);
        return http.build();
    }

    private static void write(HttpServletResponse response, HttpStatus status, Object body,
                              JsonMapper jsonMapper) throws IOException {
        response.setStatus(status.value());
        response.setContentType(MediaType.APPLICATION_JSON_VALUE);
        jsonMapper.writeValue(response.getWriter(), body);
    }
}
