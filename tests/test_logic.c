// Host-compiled unit tests for src/c/logic.c. No Pebble SDK required:
//   gcc -std=c11 -Wall -Wextra -o build/test_logic tests/test_logic.c src/c/logic.c
//   ./build/test_logic
#include <stdio.h>
#include "../src/c/logic.h"

static int g_failures = 0;
static int g_checks = 0;

#define EXPECT_EQ_INT(actual, expected, message)                                     \
  do {                                                                                \
    g_checks++;                                                                      \
    int actual_val = (actual);                                                       \
    int expected_val = (expected);                                                   \
    if (actual_val != expected_val) {                                                \
      g_failures++;                                                                  \
      printf("FAIL %s:%d: %s - expected %d, got %d\n", __FILE__, __LINE__, message,  \
             expected_val, actual_val);                                              \
    }                                                                                 \
  } while (0)

#define EXPECT_TRUE(actual, message) EXPECT_EQ_INT((actual) ? 1 : 0, 1, message)
#define EXPECT_FALSE(actual, message) EXPECT_EQ_INT((actual) ? 1 : 0, 0, message)

static void test_is_night_disabled_is_always_false(void) {
  EXPECT_FALSE(logic_is_night(23, false, 22, 6), "disabled at 23:00");
  EXPECT_FALSE(logic_is_night(2, false, 22, 6), "disabled at 02:00");
}

static void test_is_night_normal_window(void) {
  // A window that does not wrap past midnight, e.g. 13:00-17:00.
  EXPECT_FALSE(logic_is_night(12, true, 13, 17), "before window");
  EXPECT_TRUE(logic_is_night(13, true, 13, 17), "at window start");
  EXPECT_TRUE(logic_is_night(16, true, 13, 17), "inside window");
  EXPECT_FALSE(logic_is_night(17, true, 13, 17), "at window end (exclusive)");
}

static void test_is_night_wrapping_window(void) {
  // The default 22:00-06:00 window wraps past midnight.
  EXPECT_TRUE(logic_is_night(22, true, 22, 6), "at wrap start");
  EXPECT_TRUE(logic_is_night(23, true, 22, 6), "late evening");
  EXPECT_TRUE(logic_is_night(0, true, 22, 6), "midnight");
  EXPECT_TRUE(logic_is_night(5, true, 22, 6), "just before wrap end");
  EXPECT_FALSE(logic_is_night(6, true, 22, 6), "at wrap end (exclusive)");
  EXPECT_FALSE(logic_is_night(12, true, 22, 6), "midday");
}

static void test_is_night_equal_bounds_is_always_false(void) {
  EXPECT_FALSE(logic_is_night(22, true, 22, 22), "start == end never matches");
}

static void test_shift_disabled_is_always_zero(void) {
  ScreenShift shift = logic_shift_for_day(42, false);
  EXPECT_EQ_INT(shift.x, 0, "disabled shift x");
  EXPECT_EQ_INT(shift.y, 0, "disabled shift y");
}

static void test_shift_cycles_through_eight_distinct_points(void) {
  ScreenShift seen[8];
  for (int day = 0; day < 8; day++) {
    seen[day] = logic_shift_for_day(day, true);
  }
  for (int i = 0; i < 8; i++) {
    for (int j = i + 1; j < 8; j++) {
      bool same = seen[i].x == seen[j].x && seen[i].y == seen[j].y;
      EXPECT_FALSE(same, "shift ring must not repeat within one lap");
    }
  }
}

static void test_shift_wraps_after_one_lap(void) {
  ScreenShift day0 = logic_shift_for_day(0, true);
  ScreenShift day8 = logic_shift_for_day(8, true);
  EXPECT_EQ_INT(day8.x, day0.x, "wraps to day 0 x after 8 days");
  EXPECT_EQ_INT(day8.y, day0.y, "wraps to day 0 y after 8 days");

  ScreenShift day1 = logic_shift_for_day(1, true);
  ScreenShift day9 = logic_shift_for_day(9, true);
  EXPECT_EQ_INT(day9.x, day1.x, "9 % 8 == 1 x");
  EXPECT_EQ_INT(day9.y, day1.y, "9 % 8 == 1 y");
}

static void test_weekday_for_column_sunday_first(void) {
  for (int column = 0; column < 7; column++) {
    EXPECT_EQ_INT(logic_weekday_for_column(column, false), column, "identity when Sunday-first");
  }
}

static void test_weekday_for_column_monday_first(void) {
  int expected[7] = {1, 2, 3, 4, 5, 6, 0};
  for (int column = 0; column < 7; column++) {
    EXPECT_EQ_INT(logic_weekday_for_column(column, true), expected[column],
                  "Monday-first column mapping");
  }
}

static void test_hour_12h(void) {
  EXPECT_EQ_INT(logic_hour_12h(0), 12, "midnight is 12");
  EXPECT_EQ_INT(logic_hour_12h(1), 1, "1am");
  EXPECT_EQ_INT(logic_hour_12h(12), 12, "noon is 12");
  EXPECT_EQ_INT(logic_hour_12h(13), 1, "1pm");
  EXPECT_EQ_INT(logic_hour_12h(23), 11, "11pm");
}

static void test_convert_temperature(void) {
  EXPECT_EQ_INT(logic_convert_temperature(20, false), 20, "celsius passthrough");
  EXPECT_EQ_INT(logic_convert_temperature(0, true), 32, "freezing point");
  EXPECT_EQ_INT(logic_convert_temperature(100, true), 212, "boiling point");
  EXPECT_EQ_INT(logic_convert_temperature(-40, true), -40, "-40 is the crossover point");
  EXPECT_EQ_INT(logic_convert_temperature(-1, true), 30, "-1C floors to 30F, not 31F");
  EXPECT_EQ_INT(logic_convert_temperature(-6, true), 21, "-6C floors to 21F, not 22F");
}

int main(void) {
  test_is_night_disabled_is_always_false();
  test_is_night_normal_window();
  test_is_night_wrapping_window();
  test_is_night_equal_bounds_is_always_false();
  test_shift_disabled_is_always_zero();
  test_shift_cycles_through_eight_distinct_points();
  test_shift_wraps_after_one_lap();
  test_weekday_for_column_sunday_first();
  test_weekday_for_column_monday_first();
  test_hour_12h();
  test_convert_temperature();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
