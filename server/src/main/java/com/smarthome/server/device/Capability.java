package com.smarthome.server.device;

import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.FetchType;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.ManyToOne;
import jakarta.persistence.Table;
import jakarta.persistence.UniqueConstraint;
import lombok.Getter;
import lombok.Setter;

/**
 * JSONB columns are mapped as raw {@code String} + {@code @JdbcTypeCode(SqlTypes.JSON)} on
 * purpose: Hibernate 7.4's auto-detected Jackson format mapper targets Jackson 2, which is
 * not on the Boot 4 classpath. With a String attribute no format mapper is involved —
 * Hibernate passes the text through as jsonb, and DTOs re-emit it verbatim via
 * {@code @JsonRawValue}.
 */
@Entity
@Table(name = "capabilities",
        uniqueConstraints = @UniqueConstraint(columnNames = {"node_pk", "type", "channel"}))
@Getter
@Setter
public class Capability {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "node_pk", nullable = false)
    private Node node;

    /** {@code "relay"} | {@code "sensor"} */
    @Column(nullable = false)
    private String type;

    @Column(nullable = false)
    private int channel;

    private String name;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(columnDefinition = "jsonb", nullable = false)
    private String meta = "{}";

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(name = "last_state", columnDefinition = "jsonb")
    private String lastState;
}
