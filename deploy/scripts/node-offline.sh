#!/bin/sh
# Simulates LWT firing (node died): retained offline status.
#   docker compose exec mosquitto sh /scripts/node-offline.sh
U="-u ${MQTT_SERVER_USERNAME:-server} -P ${MQTT_SERVER_PASSWORD:-change-me-mqtt}"
mosquitto_pub $U -t "home/phong-khach/esp32s3-aabbcc/status" -m offline -q 1 -r
echo "fake node marked offline"
