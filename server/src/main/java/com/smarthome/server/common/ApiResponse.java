package com.smarthome.server.common;

/**
 * Uniform REST envelope: {@code {"data": ..., "error": null}} on success,
 * {@code {"data": null, "error": {"code": ..., "message": ...}}} on failure.
 */
public record ApiResponse<T>(T data, ErrorBody error) {

    public record ErrorBody(String code, String message) {}

    public static <T> ApiResponse<T> ok(T data) {
        return new ApiResponse<>(data, null);
    }

    public static <T> ApiResponse<T> error(String code, String message) {
        return new ApiResponse<>(null, new ErrorBody(code, message));
    }
}
