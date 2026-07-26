package com.smarthome.server.telemetry;

import java.time.Instant;
import java.util.List;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.smarthome.server.common.ApiResponse;
import com.smarthome.server.common.dto.SensorReadingDto;

import lombok.RequiredArgsConstructor;

@RestController
@RequestMapping("/api/v1/nodes/{nodeId}/sensors")
@RequiredArgsConstructor
public class TelemetryController {

    private final TelemetryService telemetryService;

    @GetMapping("/latest")
    public ApiResponse<SensorReadingDto> latest(@PathVariable String nodeId) {
        return ApiResponse.ok(telemetryService.latest(nodeId));
    }

    /** {@code from}/{@code to} are ISO-8601 instants; bad values yield a 400 envelope. */
    @GetMapping("/history")
    public ApiResponse<List<SensorReadingDto>> history(@PathVariable String nodeId,
                                                       @RequestParam(required = false) Instant from,
                                                       @RequestParam(required = false) Instant to,
                                                       @RequestParam(required = false) String bucket) {
        return ApiResponse.ok(telemetryService.history(nodeId, from, to, bucket));
    }
}
