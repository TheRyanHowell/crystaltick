#include "screen_care.h"
#include "settings.h"

// Fixed pure black across all four tints, like a real LCD's ink - maximizes
// contrast against the ghost segments within the palette's 4-level range.
#define INK_RED   0x00
#define INK_GREEN 0x00
#define INK_BLUE  0x00

static GColor light_tint(void) {
  switch (settings.face_color) {
    case FACE_COLOR_CYAN:
      return GColorFromRGB(0xAA, 0xFF, 0xFF);
    case FACE_COLOR_GREEN:
      return GColorFromRGB(0xAA, 0xFF, 0xAA);
    case FACE_COLOR_AMBER:
      return GColorFromRGB(0xFF, 0xFF, 0xAA);
    case FACE_COLOR_WHITE:
    default:
      return GColorFromRGB(0xFF, 0xFF, 0xFF);
  }
}

static GColor dark_ink(void) {
  return GColorFromRGB(INK_RED, INK_GREEN, INK_BLUE);
}

GColor screen_care_foreground_color(bool night) {
  return night ? light_tint() : dark_ink();
}

GColor screen_care_background_color(bool night) {
  return night ? dark_ink() : light_tint();
}

// 0xAA gives ghost-vs-ink contrast (9.15:1, the real legibility need) at
// the cost of ghost-vs-background contrast (2.3:1) - the palette's 4 gray
// levels can't satisfy both at once. Ghost is decorative texture (WCAG
// 1.4.11 exempt); real values always draw in full-contrast ink on top.
// Day-only - draw.c drops all ghost elements at night instead of recoloring.
GColor screen_care_ghost_color(void) {
  switch (settings.face_color) {
    case FACE_COLOR_CYAN:
      return GColorFromRGB(0x55, 0xAA, 0xAA);
    case FACE_COLOR_GREEN:
      return GColorFromRGB(0x55, 0xAA, 0x55);
    case FACE_COLOR_AMBER:
      return GColorFromRGB(0xAA, 0xAA, 0x55);
    case FACE_COLOR_WHITE:
    default:
      return GColorFromRGB(0xAA, 0xAA, 0xAA);
  }
}
