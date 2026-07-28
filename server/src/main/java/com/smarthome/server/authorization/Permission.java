package com.smarthome.server.authorization;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import lombok.Getter;
import lombok.NoArgsConstructor;

@Entity
@Table(name = "permissions")
@Getter
@NoArgsConstructor
public class Permission {

    public static final String NODE_VIEW = "NODE_VIEW";
    public static final String NODE_CONTROL = "NODE_CONTROL";
    public static final String TELEMETRY_VIEW = "TELEMETRY_VIEW";
    public static final String AUDIT_VIEW = "AUDIT_VIEW";

    @Id
    @Column(nullable = false)
    private String code;
}
