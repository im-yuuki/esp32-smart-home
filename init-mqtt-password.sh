#!/bin/sh
set -eu

: "${MQTT_SERVER_USERNAME:?MQTT_SERVER_USERNAME is required}"
: "${MQTT_SERVER_PASSWORD:?MQTT_SERVER_PASSWORD is required}"

password_file=/mosquitto/passwd/passwd

if [ -s "$password_file" ]; then
    mosquitto_passwd -b "$password_file" "$MQTT_SERVER_USERNAME" "$MQTT_SERVER_PASSWORD"
else
    mosquitto_passwd -b -c "$password_file" "$MQTT_SERVER_USERNAME" "$MQTT_SERVER_PASSWORD"
fi

chown mosquitto:mosquitto "$password_file"
chmod 600 "$password_file"
