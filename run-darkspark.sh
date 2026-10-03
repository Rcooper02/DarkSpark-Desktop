#!/usr/bin/env bash

ROOT="$(cd "$(dirname "$0")" && pwd)"

# Qt WebEngine: avoid RADV/Vulkan lockups during embedded media playback.
export QTWEBENGINE_CHROMIUM_FLAGS="${QTWEBENGINE_CHROMIUM_FLAGS:---disable-vulkan --disable-features=Vulkan}"
# HAL wake-word runtime
export DARKSPARK_HAL_WAKE="${DARKSPARK_HAL_WAKE:-1}"
export DARKSPARK_WAKE_MODEL="${DARKSPARK_WAKE_MODEL:-$ROOT/models/wake/hey_hal.onnx}"
export DARKSPARK_WAKE_THRESHOLD="${DARKSPARK_WAKE_THRESHOLD:-0.35}"
export DARKSPARK_MICROPHONE="${DARKSPARK_MICROPHONE:-all}"
export DARKSPARK_TIMEZONE="${DARKSPARK_TIMEZONE:-America/New_York}"
export DARKSPARK_LOCATION="${DARKSPARK_LOCATION:-Royersford, Pennsylvania, United States}"


export DARKSPARK_AI_PROVIDER=ollama
export DARKSPARK_AI_MODEL='qwen3:8b'
export DARKSPARK_AI_ENDPOINT='http://127.0.0.1:11435/api/chat'

# Leave microphone unset so HAL uses all available microphones.

exec "$(dirname "$0")/build/bin/darkspark-desktop"
