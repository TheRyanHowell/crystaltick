#include "logic.h"

#define HOURS_PER_DAY      24
#define HOURS_PER_HALF_DAY 12
#define DAYS_PER_WEEK      7

// An 8-point ring the daily shift walks around, each step 1px in at least
// one axis. A full lap takes 8 days, which is imperceptible day-to-day but
// keeps any single pixel row/column from staying lit (or unlit) for weeks.
static const ScreenShift SHIFT_RING[] = {
  {0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1},
};
#define SHIFT_RING_LEN (int)(sizeof(SHIFT_RING) / sizeof(SHIFT_RING[0]))

// Celsius-to-Fahrenheit conversion factors.
#define FAHRENHEIT_SCALE_NUM 9
#define FAHRENHEIT_SCALE_DEN 5
#define FAHRENHEIT_OFFSET    32

bool logic_is_night(int hour, bool night_invert_enabled, uint8_t start_hour, uint8_t end_hour) {
  if (!night_invert_enabled || start_hour == end_hour) {
    return false;
  }
  if (start_hour < end_hour) {
    return hour >= start_hour && hour < end_hour;
  }
  // Window wraps past midnight, e.g. 22 -> 6.
  return hour >= start_hour || hour < end_hour;
}

ScreenShift logic_shift_for_day(int day_of_year, bool anti_burn_in_shift) {
  if (!anti_burn_in_shift) {
    ScreenShift none = {0, 0};
    return none;
  }
  int index = day_of_year % SHIFT_RING_LEN;
  if (index < 0) {
    index += SHIFT_RING_LEN;
  }
  return SHIFT_RING[index];
}

int logic_weekday_for_column(int column, bool first_day_monday) {
  if (!first_day_monday) {
    return column;
  }
  return (column + 1) % DAYS_PER_WEEK;
}

int logic_hour_12h(int tm_hour) {
  int hour = tm_hour % HOURS_PER_HALF_DAY;
  return hour == 0 ? HOURS_PER_HALF_DAY : hour;
}

int logic_convert_temperature(int celsius, bool fahrenheit) {
  if (!fahrenheit) {
    return celsius;
  }
  int scaled = celsius * FAHRENHEIT_SCALE_NUM;
  // C's / truncates toward zero, which would round negative non-multiples
  // of FAHRENHEIT_SCALE_DEN up by 1 (e.g. -1C -> 31F instead of the correct
  // floor, 30F). Floor explicitly so negative and positive values round the
  // same direction.
  int quotient = scaled / FAHRENHEIT_SCALE_DEN;
  if (scaled % FAHRENHEIT_SCALE_DEN != 0 && scaled < 0) {
    quotient--;
  }
  return quotient + FAHRENHEIT_OFFSET;
}
