package com.smarthome.server.config;

import static org.assertj.core.api.Assertions.assertThat;

import java.io.InputStream;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.junit.jupiter.api.Test;
import org.springframework.core.annotation.AnnotatedElementUtils;
import org.springframework.web.bind.annotation.RequestMapping;
import org.yaml.snakeyaml.Yaml;

import com.smarthome.server.account.AuthController;
import com.smarthome.server.admin.AdminController;
import com.smarthome.server.audit.AuditController;
import com.smarthome.server.authorization.FolderController;
import com.smarthome.server.device.DeviceController;
import com.smarthome.server.telemetry.TelemetryController;

class OpenApiContractTest {

    private static final List<Class<?>> CONTROLLERS = List.of(
            AuthController.class,
            AdminController.class,
            AuditController.class,
            FolderController.class,
            DeviceController.class,
            TelemetryController.class);

    @Test
    void documentsEveryBackendHttpOperation() {
        Set<String> documented = documentedOperations();
        Set<String> implemented = controllerOperations();
        implemented.add("POST /api/v1/auth/login");
        implemented.add("POST /api/v1/auth/logout");
        implemented.add("GET /actuator/health");

        assertThat(documented).hasSize(42);
        assertThat(normalize(documented)).isEqualTo(normalize(implemented));
    }

    @SuppressWarnings("unchecked")
    private static Set<String> documentedOperations() {
        try (InputStream input = OpenApiContractTest.class.getResourceAsStream("/static/openapi.yaml")) {
            Map<String, Object> document = new Yaml().load(input);
            Map<String, Map<String, Object>> paths = (Map<String, Map<String, Object>>) document.get("paths");
            Set<String> operations = new HashSet<>();
            paths.forEach((path, item) -> item.keySet().stream()
                    .filter(OpenApiContractTest::isHttpMethod)
                    .forEach(method -> operations.add(method.toUpperCase() + " " + path)));
            return operations;
        } catch (Exception exception) {
            throw new IllegalStateException("cannot read OpenAPI contract", exception);
        }
    }

    private static Set<String> controllerOperations() {
        Set<String> operations = new HashSet<>();
        for (Class<?> controller : CONTROLLERS) {
            RequestMapping root = AnnotatedElementUtils.findMergedAnnotation(controller, RequestMapping.class);
            String prefix = root == null || root.path().length == 0 ? "" : root.path()[0];
            for (Method method : controller.getDeclaredMethods()) {
                RequestMapping mapping = AnnotatedElementUtils.findMergedAnnotation(method, RequestMapping.class);
                if (mapping == null) {
                    continue;
                }
                String suffix = mapping.path().length == 0 ? "" : mapping.path()[0];
                for (var httpMethod : mapping.method()) {
                    operations.add(httpMethod.name() + " " + prefix + suffix);
                }
            }
        }
        return operations;
    }

    private static boolean isHttpMethod(String value) {
        return Set.of("get", "post", "put", "patch", "delete").contains(value);
    }

    private static Set<String> normalize(Set<String> operations) {
        return operations.stream().map(value -> value.replaceAll("\\{[^}]+}", "{}"))
                .collect(java.util.stream.Collectors.toSet());
    }
}
