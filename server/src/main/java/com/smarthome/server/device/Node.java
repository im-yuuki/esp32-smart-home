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
import jakarta.persistence.EnumType;
import jakarta.persistence.Enumerated;
import jakarta.persistence.FetchType;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.ManyToOne;
import jakarta.persistence.OneToMany;
import jakarta.persistence.Table;
import lombok.Getter;
import lombok.Setter;
import com.smarthome.server.account.AppUser;

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

    @Enumerated(EnumType.STRING)
    @Column(name = "approval_status", nullable = false)
    private ApprovalStatus approvalStatus = ApprovalStatus.PENDING;

    @Column(name = "approved_at")
    private Instant approvedAt;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "approved_by")
    private AppUser approvedBy;

    @OneToMany(mappedBy = "node", cascade = CascadeType.ALL, orphanRemoval = true)
    private List<Capability> capabilities = new ArrayList<>();
}
