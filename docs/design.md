# Design notes

CrystalTick is inspired by [Quartz by Dalpek](https://apps.repebble.com/quartz-by-dalpek_31cfe29ecd814df4b5ca8bb4),
a retro Casio/LCD-calculator-style watchface for the Pebble Time 2, but is a fresh implementation
rather than a clone. It does not reproduce Quartz's decorative bezel artwork — the "QUARTZ"/"TIME 2"
title, the "pebble" wordmark, the fake button-arrow labels, or the "HEART RATE MONITOR"/"E-PAPER
DISPLAY" captions — since that's Quartz's own branding (including Pebble's logo) baked into one
background bitmap. CrystalTick uses the full 200×228 screen for functional content instead.

## Heart rate

`PebbleOS/src/fw/applib/health_service.h` explains the key constraint:
`health_service_peek_current_value(HealthMetricHeartRateBPM)` only returns whatever the system last
cached, and without an active subscriber the system samples heart rate very sparsely (or not at all)
to save battery — a `peek()`-on-tick implementation without a subscription would just show a stale
or zero value.

`complication.c`'s approach, applied only while Heart Rate is the active complication (and undone
the moment the user switches away, or on `deinit`):

1. `health_service_events_subscribe()` — subscribing is what engages the sensor as a consumer.
2. The heart-rate sample period is left at the system default (`0`, never set explicitly) rather
   than forcing an aggressive `1`-second period — that's a workout-app pattern that drains battery,
   not appropriate for a glanceable watchface complication.
3. `health_service_metric_accessible()` is checked so "no permission"/"not available" shows `--`
   instead of a silently-stuck `0`.
4. The event handler only marks the face dirty when the peeked BPM actually differs from the last
   one it saw (`s_last_notified_bpm`), since a steady resting heart rate can otherwise fire
   `HealthEventHeartRateUpdate` repeatedly with an unchanged value.

Confirmed working both in QEMU (`qemu_emery` has `CONFIG_HRM_STUB=y`) and on a real Pebble Time 2
via `pebble install --phone <ip>`.

## Color model: a passive LCD, not a backlit display

Early passes got this backwards (dark background, bright text — a backlit look). Real Casio/Quartz
digital watches are **passive reflective LCDs**: light, tinted glass with fixed dark ink, not a glow
on black. The final model (`screen_care.c`):

- **Background** = the selected Central Face Color tint (White/Cyan/Green/Amber) — this is the
  "glass tint," analogous to Quartz's own White/Cyan/Green/Amber option.
- **Ink** ("on" segments/text/icons) = fixed pure black across all four tints, matching how a real
  LCD's ink doesn't change with the glass color. Originally a dark navy; switched to pure black after
  feedback that the display was hard to read — black maximizes contrast against the ghost segments
  within the palette's 4-gray-level constraint (see the WCAG note below).
- **Ghost** ("unlit segment" texture, behind the time/complication/seconds and as the weekday row's
  non-today outline) = a muted variant of the tint, always lighter than a straight gray so it stays
  visually distinct from ink. Day-only — see "Night mode: pure binary, no mid-tones" below.
- **Night Auto-Invert** swaps background and ink during a configurable window (default 22:00–06:00,
  no location lookup needed) — both a genuinely useful dim/negative look at night and a periodic
  full-polarity flip that mitigates LCD burn-in from long-held static elements.

### Night mode: pure binary, no mid-tones

An early version of Night Auto-Invert just swapped foreground/background and left every other color
(ghost segments, the red `ACCENT_COLOR` bezel/box lines) untouched. Two problems with that, both
raised directly by the developer: it was harder to read (a gray tone tuned to sit near a *light*
background collapses in contrast once the background is black instead — see the superseded
night-ghost-color fix below), and it undermined the burn-in-mitigation point of inverting at all —
a mid-gray pixel never fully swaps polarity the way a pure foreground/background pixel does, so any
element left gray at night only partially participates in the nightly pixel-polarity flip.

The fix was to stop trying to recolor these elements for night and instead **not draw them at all**
at night, so every visible pixel is either full background or full foreground with nothing in
between:

- The ghost "88:88" / "8888" / "88" segment backgrounds (behind the time, complication value, and
  seconds) are skipped entirely at night — only the real lit-ink value draws.
- The complication-mode icon row draws only the *active* complication's icon at night; the other two
  (previously drawn in ghost color to show all 3 modes) are omitted.
- The weekday row's non-today outline and the battery bar's unlit-segment outline are both skipped at
  night — only today's block and the actually-lit charge segments draw.
- `ACCENT_COLOR` (the fixed dark red used for the bezel lines and complication box outline) switches
  to white at night. Red has no natural "inverted" counterpart in a 2-level day/night swap — left
  as-is it would just sit as a dim, muddy red against the black background — so white stands in as
  the night-mode version of the same "permanent, unchanging chrome" role.

This made `screen_care_ghost_color()`'s earlier night-aware variant (a second, darker color table for
night mode, added to fix the legibility problem before the "just don't draw it" fix existed)
dead code, since nothing draws a ghost element at night anymore — it was removed, and the function
went back to being day-only with no `bool night` parameter. `screen_care_foreground_color(bool)` /
`screen_care_background_color(bool)` remain night-aware, since those are the two colors that
everything else derives from.

### WCAG note (evaluated at Level AAA on request)

The palette only offers 4 gray levels per channel (0x00/0x55/0xAA/0xFF), which makes it
mathematically impossible for one ghost tone to satisfy both ghost-vs-background and ghost-vs-ink
contrast at once:

| Ghost tone | vs. background | vs. ink (black) |
|---|---|---|
| 0xAA (current) | 2.3:1 | 9.15:1 |
| 0x55 (alternative) | 7.45:1 (exceeds AAA) | 2.82:1 |

Ink-vs-background is 21:1 (far exceeds AAA) for all the actual data — time, date, battery, and the
active complication value are always drawn in full-contrast ink and never depend on the ghost for
legibility. Ghost is decorative texture (the "unlit LCD segment" simulation), which WCAG 1.4.11
exempts from contrast requirements. Kept at 0xAA — prioritizing ghost-vs-ink legibility, since
darkening it to satisfy background contrast would reintroduce the "hard to read" problem it was
tuned to fix. Confirmed with the developer rather than assumed; documented in `screen_care.c`.

## Screen care: burn-in mitigation

Long-held static elements (a bezel, a colon, an unchanging icon) can cause real, if mild, LCD
burn-in. Two features address this, both piggybacking on the existing `MINUTE_UNIT` tick rather
than adding new wake-ups:

- **Night Auto-Invert** (above) periodically flips which pixels are lit — and, since the "pure
  binary, no mid-tones" fix above, actually flips *every* visible pixel rather than leaving ghost/
  outline elements sitting at a static gray that only partially participates in the swap.
- **Anti Burn-in Shift**: once per day (at the date rollover), the whole composition's origin nudges
  by a small deterministic offset, cycling through an 8-point ring (`logic_shift_for_day`) so a full
  lap takes 8 days — imperceptible day-to-day, but no single pixel row/column stays lit or unlit for
  weeks. Layout margins are ≥6px specifically so a ±1px shift never crops content.
- The bezel is two thin horizontal accent lines (top/bottom), not a full perimeter box — fewer
  permanently-lit pixels than a full frame, while still reading as a bezel accent.

## Color as a signal, not decoration

An early pass colored the active complication icon per-mode (blue/red/orange) and today's weekday
block in teal. Explicit feedback: color should live on **static, structural** chrome that never
changes meaning — mirroring Quartz's own red border/branding, which never encodes live data — not on
elements whose color would change based on changing data (which complication is selected, which day
is today). Reverted: icons and the weekday "today" fill are back to plain ink/ghost. The one fixed
accent color (`ACCENT_COLOR`, dark red) is used only on the top/bottom bezel lines and the
complication box outline — both always present, never changing meaning. At night it becomes white
instead of red (see "Night mode: pure binary, no mid-tones" above) - still the same fixed-chrome
role, just recolored to actually read against the inverted background.

## Typography: segmented digits, but not segmented letters

Numeric elements (date, time, complication value, seconds) use the DSEG7 "ClassicMini" font — a
real 7-segment font — for the authentic LCD look. Weekday letters use a normal bold system font
(`FONT_KEY_GOTHIC_28_BOLD`), not a segmented one. Two reasons:

1. The set of 7 letters never changes — like Quartz's own static bezel text, it doesn't need to
   simulate a segmented display the way the *changing* numeric readouts do.
2. DSEG14 (the segmented alphanumeric option) has a real M/W ambiguity — verified by rendering the
   actual glyph outlines with `fontTools`, they're genuinely different paths, just both built from
   the same four diagonal corner strokes, a well-known limitation of 14-segment alphanumeric
   displays generally. A plain bold sans-serif has no such ambiguity.

The battery indicator has no "BAT" text label for a related reason: DSEG7 renders it as "bAt", and
DSEG14's uppercase B — also checked by rendering the actual outline, not assumed — turned out to be
an unrecognizable stylized shape, not a real capital B. Rather than fight either font, the label was
dropped; a segmented bar is self-explanatory as a battery gauge without a caption (the same
convention as a phone's status bar), and dropping it frees the full row width for a bolder bar.

## Weather fetch efficiency (`src/pkjs/index.js`)

Temperature is the one complication that costs phone battery/network, so `fetchWeather()` is gated
three ways: only fetched on JS `ready` (phone reconnect) or a 30-minute repeating interval *if*
Temperature is actually the selected complication (checked via Clay's own `localStorage['clay-settings']`,
which it already persists on every save - no separate tracking needed), and the resulting AppMessage
send is skipped entirely if the rounded temperature hasn't changed since the last one sent. The watch
side additionally requests a fresh reading via `REQUEST_WEATHER` the moment the user switches *to*
Temperature, so it's never stuck showing whatever the periodic refresh last happened to fetch.

## Hard-won Pebble API gotchas

Worth knowing before touching `draw.c`'s layout constants:

- **`GTextOverflowModeFill` does not pixel-clip.** Per its own doc comment it behaves like
  word-wrap-then-ellipsis. A box a few pixels too small doesn't clip a sliver of the last
  character — it silently truncates the whole string to "…". This caused two separate broken-looking
  visual passes early on. Always measure with `graphics_text_layout_get_content_size()` before
  finalizing a `GRect`, rather than estimating from font size.
- **DSEG7/DSEG14 glyphs are ~0.816em wide** — much wider relative to height than a typical system
  font (each 7-segment digit is roughly as wide as it is tall). Budget layout width accordingly;
  "88:88" at a readable height needs most of the 200px screen width on its own.
- **A custom font resource's pixel height comes from the first digit run in its `name`.** The SDK's
  font generator does `re.search("([0-9]+)", name)` — a name like `FONT_DSEG7_46` matches the `7` in
  "DSEG7" before ever reaching `46`, silently building a 7px-tall font instead of 46px. Name
  resources so the only digit run present is the intended size (e.g. `FONT_TIME_52`).
- **Clay's `select` items always send a string**, regardless of what type the option `value` is
  declared as in `config.js`. Clay's select is backed by a real HTML `<select>`, and DOM `.value` is
  always a string per the HTML spec — declaring `"value": 0` vs `"value": "0"` makes no difference.
  Toggles and sliders use different DOM elements (`checkbox`/`range`) and *do* send proper integer
  tuples. `settings.c`'s `read_uint`/`read_bool` check `tuple->type` and parse `TUPLE_CSTRING` values
  with `atoi()` rather than reading `tuple->value->int32` unconditionally — the original code did the
  latter, which silently misread the string's raw bytes and made every `select`-backed setting
  (Central Face Color, Backlight Color, Complication, Temperature Unit, Seconds Display) appear to
  do nothing when changed. Confirmed via `APP_LOG` on real hardware before and after the fix.
- **A settings blob persisted by buggy parsing stays corrupted** until a fresh AppMessage overwrites
  it — bumping `SETTINGS_VERSION` isn't needed for a code fix to take effect, but the user does need
  to re-open the Clay config page and hit Save once after installing a fix, to send fresh values.
- **`fonts_get_system_font()` results must never be passed to `fonts_unload_custom_font()`** —
  unlike `fonts_load_custom_font()`, system fonts aren't app-owned resources.
- **Custom fonts and `GPath`s belong in `draw_create_layers`/`draw_destroy_layers`, not the redraw
  path.** An earlier pass loaded `s_time_font`/`s_small_font` and built the heart icon's `GPath`
  inside `face_update_proc`, repeating the load/create and unload/destroy on every single redraw -
  expensive, and pointless since none of it depends on anything that changes per-tick.
- **A per-element "skip redrawing if unchanged" optimization was attempted and reverted.** Real-device
  testing showed that skipping `face_update_proc`'s background fill on an unchanged tick reverted the
  screen to the window's own background color, erasing the previous frame's content - Pebble's
  compositor does not preserve a layer's drawn pixels across separate render passes the way that
  approach assumed. `face_update_proc` redraws the whole face unconditionally every tick by design;
  don't reintroduce partial redraws without a real fix for that persistence problem first (likely
  genuinely separate child `Layer`s per element rather than one function with internal caching).
- **C's `/` truncates toward zero, not toward negative infinity** - `logic_convert_temperature()`'s
  Celsius-to-Fahrenheit conversion needs an explicit floor adjustment for negative values, or
  non-multiples of 5 round 1°F warm (e.g. -1°C landing on 31°F instead of 30°F).
- **`app_message_open()` must run before anything that can send on the outbox.** `complication_set_active()`
  (called from `init()`) can trigger a weather request via `app_message_outbox_begin()` if
  Temperature is already the persisted complication at boot - it has to run after AppMessage is
  opened, not before.

## Layout

All positioning lives in named `GRect`/constant macros at the top of `draw.c`, each with a comment
explaining *why* that number, not just what it is — read those before changing layout. Rough
vertical structure for emery (200×228):

```
y=10            top accent line
y=14-46         date (left) + complication box with ghost/value (right)
y=50-74         3-icon complication mode row (steps / heart / temperature)
y=78-132        time (the focal element)
y=134-156       seconds (optional, off by default)
y=140-152       battery bar (deliberately low-ink so it stays secondary to the time; narrows from
                10 to 7 segments and re-centers when seconds are shown, so it doesn't run under them)
y=161-210       weekday row (block indicator + letters)
y=218           bottom accent line
```

## Follow-ups (not in this pass)

- Pebble Round 2 (`gabbro`, 260×260 round) support — needs its own circular layout pass.
- Publishing to the Pebble Appstore (`pebble publish`).
