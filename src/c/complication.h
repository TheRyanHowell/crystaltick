#pragma once
#include <pebble.h>
#include "settings.h"

// Called whenever a new value is available for the currently-active
// complication (heart-rate event, or a weather reply arriving) so the
// caller can mark the relevant layer dirty without polling.
typedef void (*ComplicationUpdatedCallback)(void);

void complication_init(ComplicationUpdatedCallback callback);
void complication_deinit(void);

// Subscribes/unsubscribes HealthService heart-rate events and requests a
// weather refresh as needed for the newly-active complication. Call this
// once at startup and again every time settings.complication changes.
void complication_set_active(ComplicationType type);

// `available` is set to false when there's no usable reading yet (no
// permission, not supported, or no sample recorded) so the caller can show
// a placeholder instead of a misleading zero.
int32_t complication_get_steps(bool *available);
int32_t complication_get_heart_rate_bpm(bool *available);

// Value already converted to the unit selected in settings.
int32_t complication_get_temperature(bool *available);

// Reads a TEMPERATURE reply out of an AppMessage inbox dictionary, if
// present.
void complication_handle_inbox(const DictionaryIterator *iter);
