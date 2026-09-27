#pragma once
#include <pebble.h>

typedef enum {
  FACE_COLOR_WHITE = 0,
  FACE_COLOR_CYAN = 1,
  FACE_COLOR_GREEN = 2,
  FACE_COLOR_AMBER = 3,
} FaceColor;

typedef enum {
  BACKLIGHT_SYSTEM_DEFAULT = 0,
  BACKLIGHT_WHITE = 1,
  BACKLIGHT_BLUE = 2,
  BACKLIGHT_GREEN = 3,
  BACKLIGHT_AMBER = 4,
  BACKLIGHT_RED = 5,
} BacklightColorSetting;

typedef enum {
  COMPLICATION_STEPS = 0,
  COMPLICATION_TEMPERATURE = 1,
  COMPLICATION_HEART_RATE = 2,
} ComplicationType;

// POWER_SAVE only ticks seconds live while the backlight is on - see
// main.c's backlight_handler.
typedef enum {
  SECONDS_HIDE = 0,
  SECONDS_SHOW = 1,
  SECONDS_STATIC = 2,
  SECONDS_POWER_SAVE = 3,
} SecondsDisplayMode;

typedef struct {
  FaceColor face_color;
  BacklightColorSetting backlight_color;
  ComplicationType complication;
  bool temperature_fahrenheit;
  SecondsDisplayMode seconds_display;
  bool date_format_ddmm;
  bool first_day_monday;
  bool night_invert_enabled;
  uint8_t night_start_hour;
  uint8_t night_end_hour;
  bool anti_burn_in_shift;
} Settings;

extern Settings settings;

// Loads persisted settings (or defaults if none/incompatible are stored).
void settings_init(void);

// Reads any recognized MESSAGE_KEY_* tuples out of an AppMessage inbox
// dictionary into `settings`, persisting only if something was actually
// present (a pure weather reply, for instance, carries none of these keys).
// Returns whether anything changed. Unrecognized/missing keys are left
// untouched.
bool settings_apply_inbox(const DictionaryIterator *iter);
