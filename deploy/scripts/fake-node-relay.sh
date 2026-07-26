#!/bin/sh
# Simulates the node EXECUTING a relay command (publishes the resulting state).
#   docker compose exec mosquitto sh /scripts/fake-node-relay.sh ON 1
#   args: [ON|OFF] [channel] [source]
U="-u ${MQTT_SERVER_USERNAME:-server} -P ${MQTT_SERVER_PASSWORD:-change-me-mqtt}"
B="home/phong-khach/esp32s3-aabbcc"
mosquitto_pub $U -t "$B/relay/${2:-1}/state" -q 1 -r -m "{\"state\":\"${1:-ON}\",\"source\":\"${3:-mqtt}\"}"
echo "relay ${2:-1} -> ${1:-ON}"
