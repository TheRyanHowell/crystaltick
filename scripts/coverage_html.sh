#!/bin/sh
# Nicer local coverage report as HTML (needs lcov + genhtml).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/coverage"
mkdir -p "$OUT"
gcc -std=c11 -Wall -Wextra --coverage -O0 \
    -o "$OUT/test_logic" \
    "$ROOT/tests/test_logic.c" "$ROOT/src/c/logic.c"
(cd "$OUT" && ./test_logic)
lcov --capture --directory "$OUT" --base-directory "$ROOT" --output-file "$OUT/coverage.info" \
    --rc branch_coverage=1
genhtml "$OUT/coverage.info" --output-directory "$OUT/html" --branch-coverage
echo "Report: $OUT/html/index.html"
