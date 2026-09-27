#include "settings.h"
#include <stdlib.h>

#define SETTINGS_PERSIST_KEY 1
#define SETTINGS_VERSION     1

// Default night window mirrors a real Casio's dim "night" look and covers
// typical sleep hours without needing any location/timezone lookup.
#define DEFAULT_NIGHT_START_HOUR 22
#define DEFAULT_NIGHT_END_HOUR   6

typedef struct {
  uint8_t version;
  Settings settings;
} PersistedSettings;

Settings settings;

static void set_defaults(void) {
  settings = (Settings){
    .face_color = FACE_COLOR_WHITE,
    .backlight_color = BACKLIGHT_SYSTEM_DEFAULT,
    .complication = COMPLICATION_STEPS,
    .temperature_fahrenheit = false,
    // Hidden by default so first launch ticks MINUTE_UNIT.
    .seconds_display = SECONDS_HIDE,
    .date_format_ddmm = false,
    .first_day_monday = false,
    .night_invert_enabled = true,
    .night_start_hour = DEFAULT_NIGHT_START_HOUR,
    .night_end_hour = DEFAULT_NIGHT_END_HOUR,
    .anti_burn_in_shift = true,
  };
}

void settings_init(void) {
  set_defaults();

  if (!persist_exists(SETTINGS_PERSIST_KEY)) {
    return;
  }

  PersistedSettings stored;
  int bytes_read = persist_read_data(SETTINGS_PERSIST_KEY, &stored, sizeof(stored));
  if (bytes_read == (int)sizeof(stored) && stored.version == SETTINGS_VERSION) {
    settings = stored.settings;
  }
}

static void settings_save(void) {
  PersistedSettings stored = {.version = SETTINGS_VERSION, .settings = settings};
  persist_write_data(SETTINGS_PERSIST_KEY, &stored, sizeof(stored));
}

// Clay's "select" items are backed by a real HTML <select>, whose DOM
// value is always a string regardless of the declared option type - so
// both TUPLE_CSTRING and TUPLE_INT must be handled here (see docs/design.md).
static uint32_t tuple_to_uint(const Tuple *tuple) {
  if (tuple->type == TUPLE_CSTRING) {
    return (uint32_t)atoi(tuple->value->cstring);
  }
  return (uint32_t)tuple->value->int32;
}

static bool read_bool(const DictionaryIterator *iter, uint32_t key, bool *out) {
  const Tuple *tuple = dict_find(iter, key);
  if (!tuple) {
    return false;
  }
  *out = tuple_to_uint(tuple) != 0;
  return true;
}

static bool read_uint(const DictionaryIterator *iter, uint32_t key, uint32_t *out) {
  const Tuple *tuple = dict_find(iter, key);
  if (!tuple) {
    return false;
  }
  *out = tuple_to_uint(tuple);
  return true;
}

static bool apply_uint_settings(const DictionaryIterator *iter) {
  uint32_t u;
  bool changed = false;

  if (read_uint(iter, MESSAGE_KEY_CentralFaceColor, &u)) {
    settings.face_color = (FaceColor)u;
    changed = true;
  }
  if (read_uint(iter, MESSAGE_KEY_BacklightColor, &u)) {
    settings.backlight_color = (BacklightColorSetting)u;
    changed = true;
  }
  if (read_uint(iter, MESSAGE_KEY_FirstRowComplication, &u)) {
    settings.complication = (ComplicationType)u;
    changed = true;
  }
  if (read_uint(iter, MESSAGE_KEY_SecondsDisplay, &u)) {
    settings.seconds_display = (SecondsDisplayMode)u;
    changed = true;
  }
  if (read_uint(iter, MESSAGE_KEY_NightStartHour, &u)) {
    settings.night_start_hour = (uint8_t)u;
    changed = true;
  }
  if (read_uint(iter, MESSAGE_KEY_NightEndHour, &u)) {
    settings.night_end_hour = (uint8_t)u;
    changed = true;
  }
  return changed;
}

static bool apply_bool_settings(const DictionaryIterator *iter) {
  bool b;
  bool changed = false;

  if (read_bool(iter, MESSAGE_KEY_TemperatureUnit, &b)) {
    settings.temperature_fahrenheit = b;
    changed = true;
  }
  if (read_bool(iter, MESSAGE_KEY_DateFormatDDMM, &b)) {
    settings.date_format_ddmm = b;
    changed = true;
  }
  if (read_bool(iter, MESSAGE_KEY_FirstDayIsMonday, &b)) {
    settings.first_day_monday = b;
    changed = true;
  }
  if (read_bool(iter, MESSAGE_KEY_NightInvertEnabled, &b)) {
    settings.night_invert_enabled = b;
    changed = true;
  }
  if (read_bool(iter, MESSAGE_KEY_AntiBurnInShift, &b)) {
    settings.anti_burn_in_shift = b;
    changed = true;
  }
  return changed;
}

bool settings_apply_inbox(const DictionaryIterator *iter) {
  // Both must run unconditionally (not `||`-short-circuited) since each
  // covers a disjoint set of keys.
  bool uint_changed = apply_uint_settings(iter);
  bool bool_changed = apply_bool_settings(iter);
  bool changed = uint_changed || bool_changed;

  // Avoids a flash write on every AppMessage - a pure TEMPERATURE weather
  // reply, for instance, carries none of these keys.
  if (changed) {
    settings_save();
  }
  return changed;
}
