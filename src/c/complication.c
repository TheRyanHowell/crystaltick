#include "complication.h"
#include "logic.h"

static ComplicationUpdatedCallback s_callback;
static bool s_heart_rate_subscribed = false;
static int32_t s_last_temperature_c = 0;
static bool s_has_temperature = false;
static int32_t s_last_notified_bpm = -1;

// Subscribing (not just peeking) keeps the sensor driven and the value
// fresh. Only notifies on an actual BPM change, since this can fire with
// the same reading repeatedly (a steady resting heart rate).
static void health_event_handler(HealthEventType event, void *context) {
  if (event != HealthEventHeartRateUpdate) {
    return;
  }
  int32_t bpm = (int32_t)health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm == s_last_notified_bpm) {
    return;
  }
  s_last_notified_bpm = bpm;
  if (s_callback) {
    s_callback();
  }
}

void complication_init(ComplicationUpdatedCallback callback) {
  s_callback = callback;
}

void complication_deinit(void) {
  if (s_heart_rate_subscribed) {
    health_service_events_unsubscribe();
    s_heart_rate_subscribed = false;
  }
}

static void request_weather(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

void complication_set_active(ComplicationType type) {
  bool want_heart_rate = (type == COMPLICATION_HEART_RATE);
  if (want_heart_rate && !s_heart_rate_subscribed) {
    // Sample period left at the system default - not an aggressive
    // workout-app rate.
    s_heart_rate_subscribed = health_service_events_subscribe(health_event_handler, NULL);
  } else if (!want_heart_rate && s_heart_rate_subscribed) {
    health_service_events_unsubscribe();
    s_heart_rate_subscribed = false;
    s_last_notified_bpm = -1;
  }

  if (type == COMPLICATION_TEMPERATURE) {
    request_weather();
  }
}

int32_t complication_get_steps(bool *available) {
  time_t now = time(NULL);
  HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(HealthMetricStepCount, now, now);
  *available = (mask & HealthServiceAccessibilityMaskAvailable) != 0;
  if (!*available) {
    return 0;
  }
  return (int32_t)health_service_peek_current_value(HealthMetricStepCount);
}

int32_t complication_get_heart_rate_bpm(bool *available) {
  time_t now = time(NULL);
  HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(HealthMetricHeartRateBPM, now, now);
  if (!(mask & HealthServiceAccessibilityMaskAvailable)) {
    *available = false;
    return 0;
  }

  int32_t bpm = (int32_t)health_service_peek_current_value(HealthMetricHeartRateBPM);
  *available = bpm > 0;
  return bpm;
}

int32_t complication_get_temperature(bool *available) {
  *available = s_has_temperature;
  if (!s_has_temperature) {
    return 0;
  }
  return logic_convert_temperature(s_last_temperature_c, settings.temperature_fahrenheit);
}

void complication_handle_inbox(const DictionaryIterator *iter) {
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_TEMPERATURE);
  // Unlike settings.c's Clay-driven keys, this one is sent directly as a
  // plain number from src/pkjs/index.js (not a Clay `select`), so it always
  // arrives as TUPLE_INT - but check the type rather than trust it blindly.
  if (!tuple || tuple->type != TUPLE_INT) {
    return;
  }
  s_last_temperature_c = tuple->value->int32;
  s_has_temperature = true;
  if (s_callback) {
    s_callback();
  }
}
