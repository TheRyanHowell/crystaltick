#!/bin/sh
# Static analysis for the watch-side C code. Points cppcheck at the Pebble
# SDK's emery headers so it can resolve <pebble.h> instead of just guessing.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_SDK_INCLUDE="$HOME/.local/share/pebble-sdk/SDKs/4.33.1/sdk-core/pebble/emery/include"
SDK_INCLUDE="${PEBBLE_SDK_EMERY_INCLUDE:-$DEFAULT_SDK_INCLUDE}"

cppcheck \
    --enable=all \
    --inline-suppr \
    --error-exitcode=1 \
    --std=c11 \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    -I "$SDK_INCLUDE" \
    "$ROOT/src/c"
# missingInclude(System) is suppressed because it only ever fires on the
# Pebble SDK's own vendored headers (build-generated headers that don't
# exist until `pebble build` has run, and xsffi.h's quoted system includes)
# - never on our own code.
