# Architecture

## Module graph

```
main.c
  ├─ settings.c    (persisted settings struct; parses Clay AppMessage inbox)
  ├─ complication.c (steps/heart-rate/temperature values; owns HealthService subscribe lifecycle)
  ├─ draw.c        (all rendering; reads settings/complication/screen_care, owns no state of its own
  │                  beyond the last-seen time/battery it was told about)
  │    ├─ screen_care.c (day/night color decisions, ghost tone - Pebble GColor-dependent)
  │    └─ logic.c       (pure decision functions - no Pebble SDK dependency at all)
  └─ (pkjs) config.js / index.js  — phone-side Clay settings page + Open-Meteo weather fetch
```

Dependency direction is one-way down this list: `draw.c` reads from everything above it but nothing
reads from `draw.c`; `logic.c` depends on nothing Pebble-specific at all, which is what makes it the
one module unit-tested on the host (`tests/test_logic.c`, run via `npm test` with the system `gcc`,
no ARM toolchain or Pebble SDK needed).

## Why `logic.c` is separate from `screen_care.c`

Both are "decision" code, but `screen_care.c`'s functions return `GColor` — a Pebble SDK type — so
they can't be compiled or tested outside the SDK. `logic.c`'s functions take and return plain
integers/booleans/a small `ScreenShift` struct, so the exact same source file compiles under both the
ARM cross-compiler (as part of the real app) and the host `gcc` (as part of `npm test`). When adding
new decision logic, default to putting it in `logic.c` with primitive types; only reach for
`screen_care.c` when the function genuinely needs to return a `GColor` or otherwise touch the SDK.

## Data flow: a tick

1. Pebble's tick timer fires `main.c`'s `tick_handler` (cadence is `MINUTE_UNIT`, or `SECOND_UNIT`
   if Seconds Display is "Show", or "Power Save" while the backlight is on — see
   `sync_tick_subscription`/`desired_tick_units`).
2. `main.c` calls `draw_set_time()` (caches the new time in `draw.c`) and `draw_mark_face_dirty()`.
3. Pebble's compositor invokes `draw.c`'s `face_update_proc`, which:
   - asks `logic_is_night()` whether the current hour falls in the night window,
   - asks `screen_care_foreground_color()`/`background_color()`/`ghost_color()` for the actual
     `GColor`s to use (these read `settings.face_color` and the `night` flag),
   - reads `settings.complication` to decide which of `complication_get_steps()` /
     `complication_get_heart_rate_bpm()` / `complication_get_temperature()` to call,
   - reads `settings.seconds_display`, `settings.date_format_ddmm`, `settings.first_day_monday`,
     etc. directly for the rest of the layout.

`draw.c` never mutates `settings` or asks `complication.c`/`screen_care.c` to change anything — it's
a pure read-and-render step every tick. All state mutation happens in `main.c` (in response to ticks,
battery events, or AppMessage) and `settings.c`/`complication.c` (in response to being told to change
by `main.c`).

## Data flow: a settings change

1. User changes something in the Clay config page on their phone; Clay's `webviewclosed` handler
   (`src/pkjs/index.js`) sends the new values as an AppMessage dictionary.
2. `main.c`'s `inbox_received_handler` receives it and, in order:
   - calls `settings_apply_inbox()`, which parses every recognized key into the global `settings`
     struct and persists it (`persist_write_data`) *only if a key was actually present* — a pure
     `TEMPERATURE` weather reply carries none, so it doesn't trigger a flash write. See
     `docs/design.md` for the `TUPLE_CSTRING` vs `TUPLE_INT` parsing gotcha this has to handle,
   - calls `complication_handle_inbox()`, which separately looks for a `TEMPERATURE` reply (this is
     runtime weather *data*, not a settings key, but travels over the same AppMessage channel/inbox),
   - if the complication mode changed, calls `complication_set_active()` (subscribes/unsubscribes
     HealthService heart-rate events, or requests a fresh weather reading),
   - if the seconds cadence changed, re-subscribes the tick timer at the new rate,
   - if a setting actually changed, re-applies the backlight color,
   - marks the face dirty so the change is visible immediately rather than waiting for the next tick.

## Data flow: heart rate

`complication_set_active()` subscribes to `health_service_events_subscribe()` only while Heart Rate
is the selected complication (and unsubscribes the moment it isn't, and in `deinit`). The event
handler doesn't touch `draw.c` directly — it calls a callback (`complication_updated_handler` in
`main.c`) which just marks the face dirty, so `face_update_proc` re-reads
`complication_get_heart_rate_bpm()` fresh on the next redraw. This event-driven (not polling) design
is what keeps the reading live — see `docs/design.md`'s "Heart rate" section for the full HealthService
integration.

## Persisted state

A single versioned blob (`PersistedSettings` in `settings.c`: a version byte + the `Settings`
struct) under one `persist_write_data`/`persist_read_data` key. There's no migration logic yet since
there's only ever been one version — if a future settings change needs to add/reorder fields in a
way that would misread old persisted data, bump `SETTINGS_VERSION` and add a migration path in
`settings_init()` (currently: version mismatch just falls back to defaults, no migration).
