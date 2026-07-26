package com.smarthome.server.common.dto;

import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Pattern;

public record RelayCommandRequest(
        @NotNull @Pattern(regexp = "ON|OFF") String state) {
}
