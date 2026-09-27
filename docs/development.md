# Development workflow

For one-time toolchain setup, see `docs/dev-environment.md`. This doc is the day-to-day loop and
conventions for common changes.

## The edit loop

```bash
# after editing src/c/*.c or src/c/*.h:
clang-format -i src/c/*.c src/c/*.h
npm run lint          # cppcheck + eslint too
npm test              # logic.c unit tests (fast, no SDK needed)
pebble build          # the real ARM cross-compile - catches what unit tests can't
pebble install --emulator emery
pebble screenshot --no-open --emulator emery screenshot_emery.png
# then Read the PNG - don't call a visual change done without looking at it
```

Any change touching `settings.c`/`main.c`'s AppMessage handling should also be verified on real
hardware if at all possible (see "Real hardware" below) — the emulator doesn't exercise the actual
Clay/phone-side serialization path, which is where the most serious bug in this project's history
(the `TUPLE_CSTRING` settings bug, see `docs/design.md`) hid despite the emulator screenshots looking
fine.

## Adding a new setting

1. Add the option to `src/pkjs/config.js` (pick `toggle`/`select`/`slider` based on the value shape).
2. Add the same key name to `package.json`'s `messageKeys` array.
3. Add a field to `Settings` in `settings.h`, and a case in `settings_apply_inbox()` (`settings.c`)
   using `read_bool`/`read_uint` as appropriate.
4. If the new setting affects something computed once and cached (like the tick cadence or the
   HealthService subscription), update `main.c`'s `inbox_received_handler` to react to the change,
   not just `settings.c` to store it.
5. Read `settings.<field>` from wherever in `draw.c` needs it.

Remember: any `select`-type Clay item arrives as a string over AppMessage no matter what type its
option values are declared as (see `docs/design.md`) — `read_uint`/`read_bool` already handle this,
but if you add a *new* raw `dict_find`/`tuple->value->int32` call anywhere instead of going through
those helpers, it will silently misparse `select` values exactly like the original bug.

## Adding a new complication

1. Add the enum value to `ComplicationType` in `settings.h`.
2. Add the option to `config.js`'s `FirstRowComplication` select and bump `MAX_DISPLAYED_STEPS`-style
   formatting logic in `draw.c`'s `draw_complication()` switch if the new value needs custom
   formatting.
3. Add a getter in `complication.c` (mirror `complication_get_steps`/`_heart_rate_bpm`/
   `_temperature`'s `available`-out-param pattern so `draw.c` can show `--` gracefully).
4. If the new complication needs its own subscription/lifecycle (like heart rate's HealthService
   subscribe), wire it into `complication_set_active()`.
5. Add an icon-drawing function in `draw.c` and wire it into `draw_complication_icons()`'s 3-icon row
   — note this means the row becomes 4 icons; check spacing still fits within `COMPLICATION_BOX`'s
   width before assuming it does (see the layout-measurement note below).

## Changing layout

Never guess a `GRect` from a font's point size — DSEG7/DSEG14 glyphs are far wider relative to their
height than a typical system font, and `GTextOverflowModeFill` truncates to "…" instead of clipping
when a box is even a few pixels too small (both covered in `docs/design.md`). Measure first:

```c
// Temporary - add near the top of face_update_proc, remove before committing:
GSize measured = graphics_text_layout_get_content_size(
    "your test string", the_font, GRect(0, 0, 200, 80), GTextOverflowModeFill, GTextAlignmentCenter);
APP_LOG(APP_LOG_LEVEL_DEBUG, "DEBUG measured %dx%d", measured.w, measured.h);
```

Build, install, check `pebble logs`, then size the real `GRect` with real numbers (plus a few px of
padding) instead of a guess. Remove the temporary logging once the layout constant is set — see
`docs/debugging.md` for the full log-capture workflow.

## Real hardware

```bash
pebble install --phone <ip>
pebble logs --phone <ip>
pebble screenshot --no-open --phone <ip> out.png
```

The developer connection can drop between commands (`Connection refused` on a command that worked
moments ago) — this needs re-enabling on the phone side (Pebble app → Settings → Developer), it's
not something fixable from the CLI side.

A settings blob persisted by a version of the app with a parsing bug **stays corrupted** after the
bug is fixed in code and reinstalled — the watch only re-parses on a fresh AppMessage. After fixing a
settings-parsing bug, re-open the Clay config page and hit Save once (even without changing anything)
to send corrected values and overwrite the stale persisted blob.

## Getting a second opinion on visual changes

For anything beyond a mechanical fix, an independent review (a fresh subagent with no context from
the implementation session, given the current screenshot + any reference images + `draw.c`) tends to
catch things the implementing session has gone blind to — see `docs/debugging.md` for how this was
used during CrystalTick's own visual design passes, including a case where two review rounds in a row
were needed because a claimed fix hadn't actually landed.
