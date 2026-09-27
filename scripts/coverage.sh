#!/bin/sh
# Builds the unit tests with coverage instrumentation and prints a per-line
# gcov report for src/c/logic.c. Run scripts/coverage_html.sh instead for a
# browsable HTML report (needs lcov/genhtml).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/coverage"
mkdir -p "$OUT"
gcc -std=c11 -Wall -Wextra --coverage -O0 \
    -o "$OUT/test_logic" \
    "$ROOT/tests/test_logic.c" "$ROOT/src/c/logic.c"
(cd "$OUT" && ./test_logic)
(cd "$OUT" && gcov -o . "$ROOT/src/c/logic.c")
