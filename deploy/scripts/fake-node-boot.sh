#!/bin/sh
# Simulates the MQTT connect sequence of a node: retained status + discovery + relay states.
# Run inside the mosquitto container:  docker compose exec mosquitto sh /scripts/fake-node-boot.sh
U="-u ${MQTT_SERVER_USERNAME:-server} -P ${MQTT_SERVER_PASSWORD:-change-me-mqtt}"
B="home/phong-khach/esp32s3-aabbcc"
mosquitto_pub $U -t "$B/status" -m online -q 1 -r
mosquitto_pub $U -t "$B/discovery" -q 1 -r -m '{"node_id":"esp32s3-aabbcc","room":"phong-khach","fw_version":"1.0.0","ip":"192.168.1.51","capabilities":[{"type":"relay","channel":1,"name":"Den tran"},{"type":"relay","channel":2,"name":"Den ban"},{"type":"sensor","channel":1,"kind":"temperature_humidity","model":"SHT31","interval_s":30}]}'
mosquitto_pub $U -t "$B/relay/1/state" -q 1 -r -m '{"state":"OFF","source":"boot"}'
mosquitto_pub $U -t "$B/relay/2/state" -q 1 -r -m '{"state":"OFF","source":"boot"}'
echo "fake node esp32s3-aabbcc booted (room phong-khach)"
