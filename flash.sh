#!/bin/bash
# Upload echatbox to the ESP32. Default port /dev/ttyUSB0; override with PORT.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
arduino-cli upload --fqbn esp32:esp32:esp32 \
    --port "${PORT:-/dev/ttyUSB0}" \
    --input-dir "$DIR/build" \
    "$DIR/echatbox.ino"
