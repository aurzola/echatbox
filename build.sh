#!/bin/bash
# Compile echatbox for the ESP32 (classic).
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
arduino-cli compile --fqbn esp32:esp32:esp32 \
    --libraries "$DIR/libraries" \
    --build-path "$DIR/build" \
    "$DIR/echatbox.ino" "$@"
