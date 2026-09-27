#include "draw.h"
#include "complication.h"
#include "logic.h"
#include "screen_care.h"
#include "settings.h"

// Layout for emery (200x228). Content stays within a 6-8px margin so the
// anti-burn-in shift (draw_sync_window) never crops anything.
#define SCREEN_WIDTH  200
#define SCREEN_HEIGHT 228

// Two thin horizontal lines instead of a full perimeter box - fewer
// permanently-lit pixels than a full bezel frame.
#define ACCENT_LINE_Y_TOP        10
#define ACCENT_LINE_Y_BOTTOM     218
#define ACCENT_LINE_INSET        10
#define ACCENT_LINE_STROKE_WIDTH 2
// Shared by the weekday row's non-today outline and the battery bar's
// unlit-segment outline - both use the same thin, faint stroke weight.
#define OUTLINE_STROKE_WIDTH 1

// GRects below are sized from measured text content, not guessed from font
// point size - GTextOverflowModeFill truncates to "…" instead of clipping
// when a box is too small (see docs/design.md).
#define DATE_RECT     \
  (GRect) {           \
    {6, 18}, {94, 28} \
  }

#define COMPLICATION_BOX_RADIUS       6
#define COMPLICATION_BOX_STROKE_WIDTH 2
#define COMPLICATION_BOX \
  (GRect) {              \
    {104, 14}, {90, 32}  \
  }
#define COMPLICATION_TEXT \
  (GRect) {               \
    {108, 18}, {80, 24}   \
  }

// All three complication-mode icons draw in a row so it reads as a 3-way
// mode indicator, not just an unexplained icon floating above the time.
#define ICON_ROW_Y 62
#define ICON_STEPS_CENTER \
  (GPoint) {              \
    112, ICON_ROW_Y       \
  }
#define ICON_HEART_CENTER \
  (GPoint) {              \
    149, ICON_ROW_Y       \
  }
#define ICON_TEMP_CENTER \
  (GPoint) {             \
    186, ICON_ROW_Y      \
  }

// Fixed accent for permanent chrome (bezel lines, complication box outline)
// - never used on data that changes, so the face still reads as a plain
// monochrome LCD rather than a status-color scheme.
#define ACCENT_COLOR GColorFromRGB(0xAA, 0x00, 0x00)

// Red has no natural "inverted" counterpart, so night mode swaps it for
// white instead of leaving a dim, muddy red on the black background.
static GColor accent_color(bool night) {
  return night ? GColorWhite : ACCENT_COLOR;
}

// The visual anchor of the face - sized as wide as the screen comfortably
// allows, since DSEG7 digits are wide relative to their height.
#define TIME_RECT      \
  (GRect) {            \
    {6, 78}, {188, 54} \
  }

// Small subscript below the time, shown only when Seconds Display is on.
#define SECONDS_RECT     \
  (GRect) {              \
    {152, 134}, {38, 22} \
  }

// No "BAT" text label: neither DSEG7 nor DSEG14 render it legibly. A
// segmented bar is self-explanatory as a battery gauge without a caption.
// Kept short and gapped so it doesn't visually outweigh the time.
#define BATTERY_RECT_FULL \
  (GRect) {               \
    {9, 140}, {182, 12}   \
  }
#define BATTERY_SEGMENT_COUNT_FULL 10
// Narrows to 7 segments when seconds are shown so the bar doesn't run under
// SECONDS_RECT. Lit-segment count always scales to whichever count is
// active, so "full bar = 100%" holds either way.
#define BATTERY_RECT_NARROW \
  (GRect) {                 \
    {9, 140}, {137, 12}     \
  }
#define BATTERY_SEGMENT_COUNT_NARROW 7
#define BATTERY_SEGMENT_GAP          4
#define BATTERY_SEGMENT_RADIUS       2

// Only today's block is filled; every other day is a faint outline.
#define WEEKDAY_COLUMN_WIDTH 26
#define WEEKDAY_COLUMNS      7
#define WEEKDAY_ROW_X        ((SCREEN_WIDTH - (WEEKDAY_COLUMNS * WEEKDAY_COLUMN_WIDTH)) / 2)
// A gap between block and letter keeps them reading as two distinct shapes.
#define WEEKDAY_BLOCK_Y       161
#define WEEKDAY_BLOCK_SIZE    14
#define WEEKDAY_BLOCK_RADIUS  5
#define WEEKDAY_LETTER_Y      178
#define WEEKDAY_LETTER_HEIGHT 32

// COMPLICATION_TEXT/the "8888" ghost are sized for 4 digits.
#define MAX_DISPLAYED_STEPS 9999
// Temperature renders as "<value><unit letter>" (e.g. "-40C"), so the
// numeric part (with sign) must fit in 3 characters to leave room for the
// unit letter within the same 4-digit box.
#define MIN_DISPLAYED_TEMPERATURE -99
#define MAX_DISPLAYED_TEMPERATURE 999

// Three dots along a diagonal (small/mid/large, mid and large sharing the
// same radius) - spaced to read as a trail, not a fused blob.
#define STEP_DOT_SMALL_RADIUS 2
#define STEP_DOT_MID_RADIUS   3
#define STEP_DOT_LARGE_RADIUS 3
#define STEP_DOT_OFFSET       7

#define HEART_LOBE_RADIUS      4
#define HEART_LOBE_OFFSET_X    4
#define HEART_LOBE_OFFSET_Y    2
#define HEART_WEDGE_HALF_WIDTH 8
#define HEART_WEDGE_HEIGHT     8
#define TEMP_BULB_RADIUS       4
#define TEMP_STEM_WIDTH        4
#define TEMP_STEM_HEIGHT       12
#define TEMP_STEM_ABOVE_CENTER 7

static const char *const WEEKDAY_LETTERS[7] = {"S", "M", "T", "W", "T", "F", "S"};

static struct tm s_now;
static bool s_night;
static BatteryChargeState s_battery;
static bool s_backlight_active;

// Loaded once per window lifetime (draw_create_layers/draw_destroy_layers),
// not per redraw - fonts_load_custom_font is too expensive to repeat every
// tick, especially in SECONDS_SHOW/POWER_SAVE modes where redraws happen
// every second.
static GFont s_time_font;
static GFont s_small_font;
static GFont s_letter_font;

// Heart icon's wedge is the only shape needing a GPath (a filled triangle
// has no simpler primitive); built once here rather than gpath_create/
// gpath_destroy every redraw. GPath stores the points pointer, not a copy,
// so the backing array must outlive it - hence `static`, not a local.
static GPoint s_heart_wedge_points[3];
static GPath *s_heart_wedge;

// Computed once here rather than separately by face_update_proc and
// draw_sync_window - both need it for the exact same instant, and deriving
// it twice from two independently-passed struct tm values only works
// because every caller happens to pass the same one; a single cached value
// makes that a guarantee instead of a convention.
void draw_set_time(const struct tm *now) {
  s_now = *now;
  s_night = logic_is_night(s_now.tm_hour, settings.night_invert_enabled, settings.night_start_hour,
                           settings.night_end_hour);
}

void draw_set_battery(BatteryChargeState state) {
  s_battery = state;
}

void draw_set_backlight_active(bool active) {
  s_backlight_active = active;
}

static void format_date(char *buffer, size_t buffer_len) {
  int month = s_now.tm_mon + 1;
  int day = s_now.tm_mday;
  if (settings.date_format_ddmm) {
    snprintf(buffer, buffer_len, "%02d-%02d", day, month);
  } else {
    snprintf(buffer, buffer_len, "%02d-%02d", month, day);
  }
}

// Shared by draw_date/draw_time/draw_seconds/draw_complication: ghost
// placeholder (dropped at night - see draw_time), then the real value.
static void draw_ghost_then_value(GContext *ctx, GFont font, GRect rect, GTextAlignment alignment,
                                  const char *ghost_text, const char *value, GColor fg,
                                  GColor ghost, bool night) {
  if (!night) {
    graphics_context_set_text_color(ctx, ghost);
    graphics_draw_text(ctx, ghost_text, font, rect, GTextOverflowModeFill, alignment, NULL);
  }
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, value, font, rect, GTextOverflowModeFill, alignment, NULL);
}

static void draw_date(GContext *ctx, GFont font, GColor fg, GColor ghost, bool night) {
  // GCC's -Wformat-truncation can't see tm_mon/tm_mday are always small, so
  // it sizes against a worst-case %d; size the buffer to match.
  char buffer[24];
  format_date(buffer, sizeof(buffer));
  draw_ghost_then_value(ctx, font, DATE_RECT, GTextAlignmentLeft, "88-88", buffer, fg, ghost,
                        night);
}

static void format_time(char *buffer, size_t buffer_len) {
  int hour = clock_is_24h_style() ? s_now.tm_hour : logic_hour_12h(s_now.tm_hour);
  snprintf(buffer, buffer_len, "%02d:%02d", hour, s_now.tm_min);
}

// Heel drawn larger than the toe so it reads as a footprint, not a stray
// mark or extra colon.
static void draw_steps_icon(GContext *ctx, GPoint center, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(center.x - STEP_DOT_OFFSET, center.y + STEP_DOT_OFFSET),
                       STEP_DOT_LARGE_RADIUS);
  graphics_fill_circle(ctx, center, STEP_DOT_MID_RADIUS);
  graphics_fill_circle(ctx, GPoint(center.x + STEP_DOT_OFFSET, center.y - STEP_DOT_OFFSET),
                       STEP_DOT_SMALL_RADIUS);
}

// Always drawn at ICON_HEART_CENTER - no `center` param, since s_heart_wedge
// is precomputed at that exact position and would misalign otherwise.
static void draw_heart_icon(GContext *ctx, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(
      ctx,
      GPoint(ICON_HEART_CENTER.x - HEART_LOBE_OFFSET_X, ICON_HEART_CENTER.y - HEART_LOBE_OFFSET_Y),
      HEART_LOBE_RADIUS);
  graphics_fill_circle(
      ctx,
      GPoint(ICON_HEART_CENTER.x + HEART_LOBE_OFFSET_X, ICON_HEART_CENTER.y - HEART_LOBE_OFFSET_Y),
      HEART_LOBE_RADIUS);
  gpath_draw_filled(ctx, s_heart_wedge);
}

static void draw_temperature_icon(GContext *ctx, GPoint center, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(center.x, center.y + TEMP_BULB_RADIUS), TEMP_BULB_RADIUS);
  graphics_fill_rect(ctx,
                     GRect(center.x - TEMP_STEM_WIDTH / 2, center.y - TEMP_STEM_ABOVE_CENTER,
                           TEMP_STEM_WIDTH, TEMP_STEM_HEIGHT),
                     0, GCornerNone);
}

// Active complication's icon draws in ink; the other two draw in ghost
// (day) or are omitted entirely (night).
static void draw_complication_icons(GContext *ctx, GColor fg, GColor ghost, bool night) {
  if (night) {
    switch (settings.complication) {
      case COMPLICATION_STEPS:
        draw_steps_icon(ctx, ICON_STEPS_CENTER, fg);
        break;
      case COMPLICATION_HEART_RATE:
        draw_heart_icon(ctx, fg);
        break;
      case COMPLICATION_TEMPERATURE:
      default:
        draw_temperature_icon(ctx, ICON_TEMP_CENTER, fg);
        break;
    }
    return;
  }
  draw_steps_icon(ctx, ICON_STEPS_CENTER, settings.complication == COMPLICATION_STEPS ? fg : ghost);
  draw_heart_icon(ctx, settings.complication == COMPLICATION_HEART_RATE ? fg : ghost);
  draw_temperature_icon(ctx, ICON_TEMP_CENTER,
                        settings.complication == COMPLICATION_TEMPERATURE ? fg : ghost);
}

static void format_complication_value(char *value, size_t value_len) {
  switch (settings.complication) {
    case COMPLICATION_STEPS: {
      bool available;
      int32_t steps = complication_get_steps(&available);
      if (available) {
        // Clamp rather than let a 5+ digit day silently ellipsis.
        if (steps > MAX_DISPLAYED_STEPS) {
          steps = MAX_DISPLAYED_STEPS;
        }
        snprintf(value, value_len, "%ld", (long)steps);
      } else {
        snprintf(value, value_len, "--");
      }
      break;
    }
    case COMPLICATION_HEART_RATE: {
      bool available;
      int32_t bpm = complication_get_heart_rate_bpm(&available);
      if (available) {
        snprintf(value, value_len, "%ld", (long)bpm);
      } else {
        snprintf(value, value_len, "--");
      }
      break;
    }
    case COMPLICATION_TEMPERATURE:
    default: {
      bool available;
      int32_t temp = complication_get_temperature(&available);
      if (available) {
        // Clamp rather than let an extreme reading silently ellipsis.
        if (temp > MAX_DISPLAYED_TEMPERATURE) {
          temp = MAX_DISPLAYED_TEMPERATURE;
        } else if (temp < MIN_DISPLAYED_TEMPERATURE) {
          temp = MIN_DISPLAYED_TEMPERATURE;
        }
        snprintf(value, value_len, "%ld%c", (long)temp,
                 settings.temperature_fahrenheit ? 'F' : 'C');
      } else {
        snprintf(value, value_len, "--");
      }
      break;
    }
  }
}

// Ghost "8888" placeholder mirrors the time's ghost look; dropped at night.
// Sized for 4 digits, which covers every real value (steps, BPM, "-40F").
static void draw_complication(GContext *ctx, GFont font, GColor fg, GColor ghost, bool night) {
  // Structural chrome, not data - fixed accent color, not ink.
  graphics_context_set_stroke_color(ctx, accent_color(night));
  graphics_context_set_stroke_width(ctx, COMPLICATION_BOX_STROKE_WIDTH);
  graphics_draw_round_rect(ctx, COMPLICATION_BOX, COMPLICATION_BOX_RADIUS);

  char value[16];
  format_complication_value(value, sizeof(value));
  draw_ghost_then_value(ctx, font, COMPLICATION_TEXT, GTextAlignmentRight, "8888", value, fg, ghost,
                        night);
  draw_complication_icons(ctx, fg, ghost, night);
}

// Ghost digits are dropped (not recolored) at night so every pixel fully
// swaps polarity for burn-in mitigation, and reads more clearly.
static void draw_time(GContext *ctx, GFont font, GColor fg, GColor ghost, bool night) {
  char buffer[8];
  format_time(buffer, sizeof(buffer));
  draw_ghost_then_value(ctx, font, TIME_RECT, GTextAlignmentCenter, "88:88", buffer, fg, ghost,
                        night);
}

static void draw_seconds(GContext *ctx, GFont font, GColor fg, GColor ghost, bool night) {
  if (settings.seconds_display == SECONDS_HIDE) {
    return;
  }
  // Power Save only ticks live while the backlight is on; otherwise reads
  // like Static so the layout never jumps between the two.
  bool live = settings.seconds_display == SECONDS_SHOW ||
              (settings.seconds_display == SECONDS_POWER_SAVE && s_backlight_active);
  char buffer[4];
  if (live) {
    snprintf(buffer, sizeof(buffer), "%02d", s_now.tm_sec);
  } else {
    snprintf(buffer, sizeof(buffer), "00");
  }
  draw_ghost_then_value(ctx, font, SECONDS_RECT, GTextAlignmentRight, "88", buffer, fg, ghost,
                        night);
}

// Only today's block is filled; others show a faint outline by day, and
// nothing at night (see draw_time's comment on why).
static void draw_weekday_row(GContext *ctx, GFont font, GColor fg, GColor ghost, bool night) {
  for (int column = 0; column < WEEKDAY_COLUMNS; column++) {
    int weekday = logic_weekday_for_column(column, settings.first_day_monday);
    int cell_x = WEEKDAY_ROW_X + column * WEEKDAY_COLUMN_WIDTH;
    GRect block = GRect(cell_x + (WEEKDAY_COLUMN_WIDTH - WEEKDAY_BLOCK_SIZE) / 2, WEEKDAY_BLOCK_Y,
                        WEEKDAY_BLOCK_SIZE, WEEKDAY_BLOCK_SIZE);

    if (weekday == s_now.tm_wday) {
      graphics_context_set_fill_color(ctx, fg);
      graphics_fill_rect(ctx, block, WEEKDAY_BLOCK_RADIUS, GCornersAll);
    } else if (!night) {
      graphics_context_set_stroke_color(ctx, ghost);
      graphics_context_set_stroke_width(ctx, OUTLINE_STROKE_WIDTH);
      graphics_draw_round_rect(ctx, block, WEEKDAY_BLOCK_RADIUS);
    }

    GRect letter_cell =
        GRect(cell_x, WEEKDAY_LETTER_Y, WEEKDAY_COLUMN_WIDTH, WEEKDAY_LETTER_HEIGHT);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, WEEKDAY_LETTERS[weekday], font, letter_cell, GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
  }
}

// Filled block for charge held, faint outline for the rest (day only) -
// matches the weekday row's block language instead of a battery-shaped icon.
static void draw_battery(GContext *ctx, GColor fg, GColor ghost, bool night) {
  bool narrow = settings.seconds_display != SECONDS_HIDE;
  GRect rect = narrow ? BATTERY_RECT_NARROW : BATTERY_RECT_FULL;
  int segment_count = narrow ? BATTERY_SEGMENT_COUNT_NARROW : BATTERY_SEGMENT_COUNT_FULL;

  int lit_segments = (s_battery.charge_percent * segment_count + 99) / 100;
  int segment_width = (rect.size.w - (segment_count - 1) * BATTERY_SEGMENT_GAP) / segment_count;
  // Integer division leaves a few px of slack (segment_width rounds down);
  // center the whole row in it rather than anchoring flush-left, so it
  // doesn't fall short of the weekday row's shared right edge below it.
  int drawn_width = segment_count * segment_width + (segment_count - 1) * BATTERY_SEGMENT_GAP;
  int start_x = rect.origin.x + (rect.size.w - drawn_width) / 2;
  for (int i = 0; i < segment_count; i++) {
    int x = start_x + i * (segment_width + BATTERY_SEGMENT_GAP);
    GRect segment = GRect(x, rect.origin.y, segment_width, rect.size.h);
    if (i < lit_segments) {
      graphics_context_set_fill_color(ctx, fg);
      graphics_fill_rect(ctx, segment, BATTERY_SEGMENT_RADIUS, GCornersAll);
    } else if (!night) {
      graphics_context_set_stroke_color(ctx, ghost);
      graphics_context_set_stroke_width(ctx, OUTLINE_STROKE_WIDTH);
      graphics_draw_round_rect(ctx, segment, BATTERY_SEGMENT_RADIUS);
    }
  }
}

static void draw_accent_lines(GContext *ctx, bool night) {
  graphics_context_set_stroke_color(ctx, accent_color(night));
  graphics_context_set_stroke_width(ctx, ACCENT_LINE_STROKE_WIDTH);
  graphics_draw_line(ctx, GPoint(ACCENT_LINE_INSET, ACCENT_LINE_Y_TOP),
                     GPoint(SCREEN_WIDTH - ACCENT_LINE_INSET, ACCENT_LINE_Y_TOP));
  graphics_draw_line(ctx, GPoint(ACCENT_LINE_INSET, ACCENT_LINE_Y_BOTTOM),
                     GPoint(SCREEN_WIDTH - ACCENT_LINE_INSET, ACCENT_LINE_Y_BOTTOM));
}

// cppcheck-suppress constParameterCallback ; must match LayerUpdateProc exactly
static void face_update_proc(Layer *layer, GContext *ctx) {
  bool night = s_night;
  GColor fg = screen_care_foreground_color(night);
  GColor bg = screen_care_background_color(night);
  GColor ghost = screen_care_ghost_color();

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  draw_accent_lines(ctx, night);

  draw_date(ctx, s_small_font, fg, ghost, night);
  draw_complication(ctx, s_small_font, fg, ghost, night);
  draw_time(ctx, s_time_font, fg, ghost, night);
  draw_seconds(ctx, s_small_font, fg, ghost, night);
  draw_weekday_row(ctx, s_letter_font, fg, ghost, night);
  draw_battery(ctx, fg, ghost, night);
}

FaceLayers draw_create_layers(Layer *window_layer, GRect bounds) {
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_52));
  s_small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SMALL_22));
  // Weekday letters use a normal bold font, not the segmented style -
  // DSEG14 has a known M/W ambiguity. System fonts aren't app-owned
  // resources, so this one is never unloaded, unlike the two above.
  s_letter_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  s_heart_wedge_points[0] =
      GPoint(ICON_HEART_CENTER.x - HEART_WEDGE_HALF_WIDTH, ICON_HEART_CENTER.y);
  s_heart_wedge_points[1] =
      GPoint(ICON_HEART_CENTER.x + HEART_WEDGE_HALF_WIDTH, ICON_HEART_CENTER.y);
  s_heart_wedge_points[2] = GPoint(ICON_HEART_CENTER.x, ICON_HEART_CENTER.y + HEART_WEDGE_HEIGHT);
  GPathInfo wedge_info = {.num_points = 3, .points = s_heart_wedge_points};
  s_heart_wedge = gpath_create(&wedge_info);

  FaceLayers layers;
  layers.root = layer_create(bounds);
  layer_add_child(window_layer, layers.root);

  layers.face = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_set_update_proc(layers.face, face_update_proc);
  layer_add_child(layers.root, layers.face);

  return layers;
}

void draw_destroy_layers(FaceLayers *layers) {
  layer_destroy(layers->face);
  layer_destroy(layers->root);
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_small_font);
  gpath_destroy(s_heart_wedge);
}

void draw_sync_window(FaceLayers *layers, Window *window, const struct tm *now) {
  ScreenShift shift = logic_shift_for_day(now->tm_yday, settings.anti_burn_in_shift);
  GRect frame = layer_get_frame(layers->root);
  frame.origin.x = shift.x;
  frame.origin.y = shift.y;
  layer_set_frame(layers->root, frame);

  // Relies on draw_set_time(now) having already been called with this same
  // `now` (true at every current call site) so s_night reflects this exact
  // instant rather than re-deriving it independently.
  window_set_background_color(window, screen_care_background_color(s_night));
}

void draw_mark_face_dirty(FaceLayers *layers) {
  layer_mark_dirty(layers->face);
}
