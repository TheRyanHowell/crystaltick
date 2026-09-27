#pragma once
#include <pebble.h>

// Simulates a passive reflective LCD (tinted glass, dark ink) rather than a
// backlit display - see docs/design.md. "night" swaps foreground/background,
// both for a usable dim look and to periodically flip pixel polarity for
// burn-in mitigation.
GColor screen_care_foreground_color(bool night);
GColor screen_care_background_color(bool night);

// Muted variant of the theme's tint, for ghost/unlit-segment texture and
// the faint (non-today) weekday blocks. Day-only - draw.c drops all ghost
// elements at night instead of recoloring them.
GColor screen_care_ghost_color(void);
