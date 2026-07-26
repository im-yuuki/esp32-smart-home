# ADR-0002: Spring Boot 4.1 instead of the roadmap's 3.x

**Status:** accepted (2026-07-26, user decision)

## Context
The roadmap locks Spring Boot 3.x / Java 21. The 3.5 line received its final OSS release (3.5.16, 2026-06-25) and reached open-source EOL on 2026-06-30. Starting a greenfield backend on an EOL line was rejected by the user.

## Decision
Spring Boot **4.1.0**, Java 21. Deltas absorbed (verified facts in `docs/design/spring-boot-4-facts.md`):
- Starter rename: `spring-boot-starter-webmvc` (not `-web`).
- Jackson 3 (`tools.jackson.*`): `JsonMapper` + `JsonMapperBuilderCustomizer`; Jackson annotations remain `com.fasterxml`. STOMP converter is `JacksonJsonMessageConverter`.
- `spring-integration-mqtt` 7.1.0 is BOM-managed, but the Paho client is `<optional>` → explicit `org.eclipse.paho:org.eclipse.paho.client.mqttv3:1.2.5`.
- Hibernate 7.4's JSON format-mapper targets Jackson 2 (absent) → JSONB columns mapped as `String` + `@JdbcTypeCode(SqlTypes.JSON)`, re-emitted with `@JsonRawValue`.
- springdoc-openapi (roadmap dependency) is **deferred to Phase 2**: springdoc 3.0.3 is not yet rebuilt against Boot 4.1.

## Consequences
Phase 2+ roadmap instructions written for Boot 3 (security starter, jjwt, etc.) need coordinate/API adaptation when implemented.
