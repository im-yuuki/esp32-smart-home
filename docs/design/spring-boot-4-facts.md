All facts verified. Here is the fact sheet.

# Spring Boot 4.x Fact Sheet (verified 2026-07-26)

## 1. Version & Java
- **Latest stable: Spring Boot 4.1.0** (released 2026-06-10; Maven Central `latest/release` = 4.1.0, no 4.1.1 yet). Built on **Spring Framework 7.0.8**, Spring Integration 7.1.0.
- **Java: minimum 17**, "latest LTS encouraged" (migration guide). **Java 21 is fully fine** (only jOOQ 3.20 support needs 21+). Kotlin 2.2+, GraalVM native-image 25+ if used.

## 2. Starters (Boot 4 renames vs Boot 3)
| Purpose | artifactId (Boot 4) | Notes |
|---|---|---|
| Web MVC | `spring-boot-starter-webmvc` | renamed from `spring-boot-starter-web` (old name still published at 4.1.0 as a deprecated transitional starter that pulls in `spring-boot-webmvc`) |
| WebSocket/STOMP | `spring-boot-starter-websocket` | **unchanged**, exists at 4.1.0 |
| Validation | `spring-boot-starter-validation` | unchanged (Hibernate Validator 9.1 in Boot 4.1) |
| JPA | `spring-boot-starter-data-jpa` | unchanged |
| Actuator | `spring-boot-starter-actuator` | unchanged |
| (also renamed) | `spring-boot-starter-aop`â†’`spring-boot-starter-aspectj`, `-web-services`â†’`-webservices`, `oauth2-*`â†’`security-oauth2-*` | per OpenRewrite Boot4 recipe |

Group is `org.springframework.boot`, version 4.1.0 for all. Auto-configuration is modularized per-starter in Boot 4.

## 3. Jackson 3
- Yes: Boot 4 / Framework 7 use **Jackson 3, groupId `tools.jackson`** (Boot 4.1.0 manages `jackson-bom` **3.1.4**; Central latest databind is 3.2.1). Exception: annotations stay `com.fasterxml.jackson.core:jackson-annotations` (2.21 managed).
- JacksonConfig: define an immutable **`tools.jackson.databind.json.JsonMapper`** bean (built via `JsonMapper.builder()`) instead of `ObjectMapper`; customize via **`JsonMapperBuilderCustomizer`** (replaces `Jackson2ObjectMapperBuilderCustomizer`); `@JsonComponent`â†’`@JacksonComponent`; `JsonObjectSerializer`â†’`ObjectValueSerializer`.
- Converters (Framework 7): HTTP `MappingJackson2HttpMessageConverter`â†’**`org.springframework.http.converter.json.JacksonJsonHttpMessageConverter`**; STOMP/messaging `MappingJackson2MessageConverter`â†’**`org.springframework.messaging.converter.JacksonJsonMessageConverter`** (both take `JsonMapper`, "Since 7.0", verified in 7.0.8 javadoc).
- Escape hatches: `spring.jackson.use-jackson2-defaults=true`; deprecated `spring-boot-jackson2` bridge module.

## 4. Spring Integration MQTT
- **`org.springframework.integration:spring-integration-mqtt:7.1.0`** â€” exactly the version Boot 4.1.0's BOM manages (`spring-integration.version=7.1.0`), so it is NOT awkward under Boot 4; use it with `spring-boot-starter-integration:4.1.0`.
- Adapter classes **unchanged**: `MqttPahoMessageDrivenChannelAdapter` (v3), `Mqttv5PahoMessageDrivenChannelAdapter` (v5). Behavior change in 7.x: v5 adapter/handler now delegate payload conversion to the configured `SmartMessageConverter` via `fromMessage()`/`toMessage()` â€” breaking only for custom converters.
- Paho clients are **`<optional>true</optional>`** in the SI 7.1.0 POM â€” you must add one explicitly: `org.eclipse.paho:org.eclipse.paho.client.mqttv3:1.2.5` (MQTT 3.1.1) or `org.eclipse.paho:org.eclipse.paho.mqttv5.client:1.2.5` (both are the current latest on Central). Direct Paho use is viable but unnecessary here.

## 5. springdoc-openapi
- Boot 4 line is **springdoc 3.x**; latest **3.0.3** (built against Boot 4.0.5; works with 4.x â€” minor caveat: not yet rebuilt against 4.1). Boot 3 line stays 2.8.x.
- Artifact: **`org.springdoc:springdoc-openapi-starter-webmvc-ui:3.0.3`**.

## 6. Flyway
- Boot 4.1.0 manages **Flyway 12.4.0** (Central latest is 13.0.0 â€” stick to the Boot-managed 12.4.0). Need **both** `org.flywaydb:flyway-core` and `org.flywaydb:flyway-database-postgresql` (versions from Boot BOM). PostgreSQL verified range 9.2â€“18, so **PostgreSQL 16 is supported**.

## 7. Hibernate / JSONB / INET
- Boot 4.1.0 manages **Hibernate ORM 7.4.1.Final** (`org.hibernate.orm:hibernate-core`; ignore 8.0.0.Beta1 on Central). `hibernate-jpamodelgen` renamed `hibernate-processor`.
- JSONB idiom (unchanged in 7.x): `@JdbcTypeCode(SqlTypes.JSON)` on the attribute (optionally `@Column(columnDefinition = "jsonb")`; PostgreSQLDialect defaults JSON to jsonb). Works with POJO/Map types; format mapper auto-detected.
- INET: `SqlTypes.INET` exists but adds cast/driver friction â€” **recommend mapping IP as plain `String` to a TEXT/varchar column** for simplicity.
- JDBC driver managed: `org.postgresql:postgresql:42.7.11`.

## 8. Lombok
- **Works.** Boot 4.1.0's BOM manages **Lombok 1.18.46** (also current latest on Central); compatible with Java 21 toolchain.

## 9. Docker images (tags verified active on Docker Hub today)
- Build: **`maven:3.9.11-eclipse-temurin-21`** (or floating `maven:3.9-eclipse-temurin-21`).
- Runtime: **`eclipse-temurin:21-jre`** (~115 MB) or **`eclipse-temurin:21-jre-alpine`** (~74 MB).

## Sources
- [Spring Boot 4.1.0 available now (spring.io blog)](https://spring.io/blog/2026/06/10/spring-boot-4/)
- [Spring Boot 4.0 Migration Guide (GitHub wiki)](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-4.0-Migration-Guide)
- [Spring Boot 4.1 Release Notes (GitHub wiki)](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-4.1-Release-Notes)
- [spring-boot-dependencies-4.1.0.pom (Maven Central â€” authoritative managed versions)](https://repo1.maven.org/maven2/org/springframework/boot/spring-boot-dependencies/4.1.0/spring-boot-dependencies-4.1.0.pom)
- [OpenRewrite: Rename Spring Boot 4.0 starters](https://docs.openrewrite.org/recipes/java/spring/boot4/renamedeprecatedstartersmanagedversions)
- [Introducing Jackson 3 support in Spring (spring.io blog)](https://spring.io/blog/2025/10/07/introducing-jackson-3-support-in-spring/)
- [JacksonJsonMessageConverter javadoc (Framework 7.0.8)](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/messaging/converter/JacksonJsonMessageConverter.html) / [JacksonJsonHttpMessageConverter javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/http/converter/json/JacksonJsonHttpMessageConverter.html)
- [Spring Integration What's New (7.1)](https://docs.spring.io/spring-integration/reference/whats-new.html) / [spring-integration-mqtt-7.1.0.pom](https://repo1.maven.org/maven2/org/springframework/integration/spring-integration-mqtt/7.1.0/spring-integration-mqtt-7.1.0.pom)
- [springdoc-openapi releases](https://github.com/springdoc/springdoc-openapi/releases) / [springdoc.org](https://springdoc.org/)
- [Flyway PostgreSQL database reference (Redgate)](https://documentation.red-gate.com/flyway/reference/database-driver-reference/postgresql-database)
- Maven Central maven-metadata.xml for spring-boot-starter-webmvc/-websocket/-validation/-data-jpa/-actuator/-integration/-web (all 4.1.0), spring-integration-mqtt (7.1.0), tools.jackson.core:jackson-databind (3.2.1), Paho v3/v5 (1.2.5), Lombok (1.18.46), flyway-core (13.0.0), hibernate-core (8.0.0.Beta1), spring-webmvc (7.0.8)
- Docker Hub API tag checks: `library/maven:3.9-eclipse-temurin-21`, `3.9.11-eclipse-temurin-21`, `library/eclipse-temurin:21-jre`, `21-jre-alpine`