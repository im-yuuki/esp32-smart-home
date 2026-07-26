package com.smarthome.server.device;

import java.util.List;
import java.util.Map;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.ResponseStatus;
import org.springframework.web.bind.annotation.RestController;

import com.smarthome.server.common.ApiResponse;
import com.smarthome.server.common.dto.NodeDto;
import com.smarthome.server.common.dto.RelayCommandRequest;

import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;

@RestController
@RequestMapping("/api/v1/nodes")
@RequiredArgsConstructor
public class DeviceController {

    private final NodeService nodeService;

    @GetMapping
    public ApiResponse<List<NodeDto>> listNodes() {
        return ApiResponse.ok(nodeService.getAllNodes());
    }

    @GetMapping("/{nodeId}")
    public ApiResponse<NodeDto> getNode(@PathVariable String nodeId) {
        return ApiResponse.ok(nodeService.getNode(nodeId));
    }

    /**
     * 202 Accepted immediately — the command is only published to {@code .../relay/{ch}/set};
     * the actual state change arrives later as a RELAY_STATE WebSocket event once the node
     * reports on its state topic (state-topic-is-truth).
     */
    @PostMapping("/{nodeId}/relays/{channel}/command")
    @ResponseStatus(HttpStatus.ACCEPTED)
    public ApiResponse<Map<String, Object>> sendCommand(@PathVariable String nodeId,
                                                        @PathVariable int channel,
                                                        @Valid @RequestBody RelayCommandRequest request) {
        nodeService.sendRelayCommand(nodeId, channel, request.state());
        return ApiResponse.ok(Map.of("accepted", true, "state", request.state()));
    }
}
