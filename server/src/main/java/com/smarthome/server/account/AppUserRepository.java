package com.smarthome.server.account;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Lock;
import org.springframework.data.jpa.repository.Query;

import jakarta.persistence.LockModeType;

public interface AppUserRepository extends JpaRepository<AppUser, Long> {

    @Query("select u from AppUser u where lower(u.username) = lower(:username)")
    Optional<AppUser> findByUsernameIgnoreCase(String username);

    boolean existsBySystemAdminTrueAndEnabledTrue();

    long countBySystemAdminTrueAndEnabledTrue();

    @Query("""
            select u.username from AppUser u
            where u.systemAdmin = true and u.enabled = true and u.mustChangePassword = false
            """)
    List<String> findEnabledSystemAdminUsernames();

    @Lock(LockModeType.PESSIMISTIC_WRITE)
    @Query("select u from AppUser u where u.systemAdmin = true and u.enabled = true order by u.id")
    List<AppUser> lockEnabledSystemAdmins();

    @Lock(LockModeType.PESSIMISTIC_WRITE)
    @Query("select u from AppUser u where u.id = :id")
    Optional<AppUser> findByIdForUpdate(Long id);
}
