#!/bin/sh
# Watches server-issued commands addressed to the fake node (Ctrl-C to stop).
#   docker compose exec mosquitto sh /scripts/watch-node-commands.sh
U="-u ${MQTT_SERVER_USERNAME:-server} -P ${MQTT_SERVER_PASSWORD:-change-me-mqtt}"
B="home/phong-khach/esp32s3-aabbcc"
exec mosquitto_sub $U -v -t "$B/relay/+/set" -t "$B/cmd"
