#!/bin/sh
# Compiles and runs the host-side unit tests for src/c/logic.c. No Pebble
# SDK needed - this is plain C11 built with the system compiler.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/build"
gcc -std=c11 -Wall -Wextra -Werror \
    -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wwrite-strings \
    -Wredundant-decls -Wformat=2 -fanalyzer \
    -o "$ROOT/build/test_logic" \
    "$ROOT/tests/test_logic.c" "$ROOT/src/c/logic.c"
"$ROOT/build/test_logic"
