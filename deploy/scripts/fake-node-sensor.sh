#!/bin/sh
# Publishes one sensor reading, or loops every 30s with `loop` as first arg.
# Optional 2nd/3rd args: temperature, humidity (default: mild random walk).
#   docker compose exec mosquitto sh /scripts/fake-node-sensor.sh
#   docker compose exec mosquitto sh /scripts/fake-node-sensor.sh loop
U="-u ${MQTT_SERVER_USERNAME:-server} -P ${MQTT_SERVER_PASSWORD:-change-me-mqtt}"
B="home/phong-khach/esp32s3-aabbcc"

publish() {
  T=${2:-$((26 + $(date +%s) % 5)).$((RANDOM % 10))}
  H=${3:-$((55 + $(date +%s) % 15)).$((RANDOM % 10))}
  mosquitto_pub $U -t "$B/sensor/state" -q 0 -m "{\"temperature\":$T,\"humidity\":$H,\"ts\":$(date +%s)}"
  echo "published T=$T H=$H"
}

if [ "$1" = "loop" ]; then
  while true; do publish "$@"; sleep 30; done
else
  publish "$@"
fi
