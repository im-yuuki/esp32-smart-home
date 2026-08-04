package com.smarthome.server.config;

import static org.assertj.core.api.Assertions.assertThat;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.Statement;

import org.flywaydb.core.Flyway;
import org.junit.jupiter.api.Test;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;
import org.testcontainers.mysql.MySQLContainer;

@Testcontainers(disabledWithoutDocker = true)
class MySqlMigrationTest {

    @Container
    static final MySQLContainer MYSQL = new MySQLContainer("mysql:8.4");

    @Test
    void baselineMigratesOnMySql84() throws Exception {
        Flyway flyway = Flyway.configure()
                .dataSource(MYSQL.getJdbcUrl(), MYSQL.getUsername(), MYSQL.getPassword())
                .load();

        assertThat(flyway.migrate().migrationsExecuted).isEqualTo(1);

        try (Connection connection = MYSQL.createConnection("");
             Statement statement = connection.createStatement()) {
            assertThat(queryInt(statement,
                    "select count(*) from information_schema.tables where table_schema = database() "
                            + "and table_name not in ('flyway_schema_history')"))
                    .isEqualTo(16);
            assertThat(queryInt(statement, "select count(*) from permissions")).isEqualTo(4);
            assertThat(queryInt(statement, "select count(*) from device_types")).isEqualTo(6);
            assertThat(queryInt(statement,
                    "select count(*) from folder_closure where ancestor_id = descendant_id and depth = 0"))
                    .isEqualTo(1);
        }
    }

    private static int queryInt(Statement statement, String sql) throws Exception {
        try (ResultSet result = statement.executeQuery(sql)) {
            result.next();
            return result.getInt(1);
        }
    }
}
