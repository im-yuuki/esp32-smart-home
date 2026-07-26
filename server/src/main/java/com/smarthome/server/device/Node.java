package com.smarthome.server.device;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;

import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;

import jakarta.persistence.CascadeType;
import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.OneToMany;
import jakarta.persistence.Table;
import lombok.Getter;
import lombok.Setter;

@Entity
@Table(name = "nodes")
@Getter
@Setter
public class Node {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "node_id", nullable = false, unique = true)
    private String nodeId;

    @Column(nullable = false)
    private String room;

    @Column(name = "fw_version")
    private String fwVersion;

    /**
     * Schema column is {@code inet} (V1 SQL is a roadmap invariant); Hibernate 7 binds a
     * String attribute through {@link SqlTypes#INET} via PGobject.
     */
    @JdbcTypeCode(SqlTypes.INET)
    @Column(columnDefinition = "inet")
    private String ip;

    @Column(nullable = false)
    private boolean online;

    @Column(name = "last_seen")
    private Instant lastSeen;

    /** DB-generated ({@code DEFAULT now()}); never written by the application. */
    @Column(name = "created_at", nullable = false, insertable = false, updatable = false)
    private Instant createdAt;

    @OneToMany(mappedBy = "node", cascade = CascadeType.ALL, orphanRemoval = true)
    private List<Capability> capabilities = new ArrayList<>();
}
