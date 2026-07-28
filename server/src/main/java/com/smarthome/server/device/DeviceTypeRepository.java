package com.smarthome.server.device;

import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;

public interface DeviceTypeRepository extends JpaRepository<DeviceType, Long> {
    List<DeviceType> findAllByOrderByNameAsc();
}
