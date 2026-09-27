#pragma once
#include <pebble.h>

typedef struct {
  Layer *root; // Shiftable container - moved daily by the anti-burn-in offset.
  Layer *face; // Everything: date, time, complication, weekday row, battery, accent lines.
} FaceLayers;

FaceLayers draw_create_layers(Layer *window_layer, GRect bounds);
void draw_destroy_layers(FaceLayers *layers);

// Feeds the latest tick/battery state to the drawing code. Call before
// marking the face dirty.
void draw_set_time(const struct tm *now);
void draw_set_battery(BatteryChargeState state);

// Only consulted in SECONDS_POWER_SAVE mode. See main.c's backlight_handler.
void draw_set_backlight_active(bool active);

// Repositions the root layer per the current anti-burn-in shift for `now`
// and keeps the window's own background color in sync with night mode, so
// neither the shift's 1px seam nor a day/night transition ever exposes the
// wrong background color at the screen edge. Cheap - call on every tick,
// not just once a day, since night mode starts/ends at a configurable hour.
void draw_sync_window(FaceLayers *layers, Window *window, const struct tm *now);

void draw_mark_face_dirty(FaceLayers *layers);
