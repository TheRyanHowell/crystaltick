#include <pebble.h>
#include "complication.h"
#include "draw.h"
#include "settings.h"

static Window *s_window;
static FaceLayers s_layers;

static void refresh_backlight_color(void) {
  switch (settings.backlight_color) {
    case BACKLIGHT_WHITE:
      light_set_color(GColorWhite);
      break;
    case BACKLIGHT_BLUE:
      light_set_color(GColorBlue);
      break;
    case BACKLIGHT_GREEN:
      light_set_color(GColorGreen);
      break;
    case BACKLIGHT_AMBER:
      light_set_color(GColorFromRGB(0xFF, 0xAA, 0x00));
      break;
    case BACKLIGHT_RED:
      light_set_color(GColorRed);
      break;
    case BACKLIGHT_SYSTEM_DEFAULT:
    default:
      // No explicit light_set_color() call: leaves the system's own default
      // backlight tint in effect.
      break;
  }
}

// cppcheck-suppress constParameterCallback ; must match TickHandler exactly
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)units_changed;
  draw_set_time(tick_time);
  // Every tick, not just day rollover: night mode starts/ends at a
  // configurable hour, and both calls inside are cheap enough not to gate.
  draw_sync_window(&s_layers, s_window, tick_time);
  draw_mark_face_dirty(&s_layers);
}

// POWER_SAVE only wants SECOND_UNIT while the backlight is on.
static TimeUnits desired_tick_units(void) {
  if (settings.seconds_display == SECONDS_SHOW) {
    return SECOND_UNIT;
  }
  if (settings.seconds_display == SECONDS_POWER_SAVE && light_is_on()) {
    return SECOND_UNIT;
  }
  return MINUTE_UNIT;
}

// Safe to call anywhere, including window_load - unsubscribing with no
// active subscription is a no-op.
static void sync_tick_subscription(void) {
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(desired_tick_units(), tick_handler);
}

static void battery_handler(BatteryChargeState state) {
  draw_set_battery(state);
  draw_mark_face_dirty(&s_layers);
}

static void complication_updated_handler(void) {
  draw_mark_face_dirty(&s_layers);
}

static void request_temperature_refresh_if_needed(void) {
  complication_set_active(settings.complication);
}

static void backlight_handler(bool on) {
  draw_set_backlight_active(on);
  if (settings.seconds_display == SECONDS_POWER_SAVE) {
    sync_tick_subscription();
    draw_mark_face_dirty(&s_layers);
  }
}

// cppcheck-suppress constParameterCallback ; must match AppMessageInboxReceived exactly
static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  (void)context;
  ComplicationType previous_complication = settings.complication;
  TimeUnits previous_units = desired_tick_units();

  bool settings_changed = settings_apply_inbox(iterator);
  complication_handle_inbox(iterator);

  if (settings.complication != previous_complication) {
    request_temperature_refresh_if_needed();
  }

  TimeUnits new_units = desired_tick_units();
  if (new_units != previous_units) {
    sync_tick_subscription();
  }

  if (settings_changed) {
    refresh_backlight_color();
  }

  time_t now = time(NULL);
  const struct tm *now_tm = localtime(&now);
  draw_set_time(now_tm);
  draw_sync_window(&s_layers, s_window, now_tm);
  draw_mark_face_dirty(&s_layers);
}

// cppcheck-suppress constParameterCallback ; must match WindowHandler exactly
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_layers = draw_create_layers(window_layer, bounds);

  time_t now = time(NULL);
  const struct tm *now_tm = localtime(&now);
  draw_set_time(now_tm);
  draw_sync_window(&s_layers, s_window, now_tm);

  BatteryChargeState battery = battery_state_service_peek();
  draw_set_battery(battery);
  draw_set_backlight_active(light_is_on());

  refresh_backlight_color();
  sync_tick_subscription();
  battery_state_service_subscribe(battery_handler);
  backlight_service_subscribe(backlight_handler);
}

static void window_unload(Window *window) {
  (void)window;
  backlight_service_unsubscribe();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  draw_destroy_layers(&s_layers);
}

static void init(void) {
  settings_init();
  complication_init(complication_updated_handler);

  // Must open AppMessage before complication_set_active(), which can send a
  // weather request via app_message_outbox_begin() if Temperature is active.
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  complication_set_active(settings.complication);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                         .load = window_load,
                                         .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  complication_deinit();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
