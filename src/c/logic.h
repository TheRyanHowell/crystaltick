#pragma once
#include <stdbool.h>
#include <stdint.h>

// Pure decision logic with no Pebble SDK dependency, so it can be built and
// unit-tested with the host compiler as well as the ARM toolchain. Keep
// this file free of <pebble.h> and any drawing/AppMessage concerns.

typedef struct {
  int8_t x;
  int8_t y;
} ScreenShift;

// Whether `hour` (0-23) falls inside the configured night window. Handles
// windows that wrap past midnight (e.g. start=22, end=6).
bool logic_is_night(int hour, bool night_invert_enabled, uint8_t start_hour, uint8_t end_hour);

// The deterministic 1px-per-day anti-burn-in drift for a given
// day-of-year (0-365), or {0, 0} when the feature is disabled.
ScreenShift logic_shift_for_day(int day_of_year, bool anti_burn_in_shift);

// Actual weekday (0=Sunday..6=Saturday) shown in weekday-row display
// column `column` (0-6), after applying the "first day is Monday" setting.
int logic_weekday_for_column(int column, bool first_day_monday);

// 12-hour-clock hour (1-12) for a 24-hour `tm_hour` (0-23).
int logic_hour_12h(int tm_hour);

// Converts a Celsius reading to Fahrenheit when requested; otherwise a
// no-op passthrough.
int logic_convert_temperature(int celsius, bool fahrenheit);
