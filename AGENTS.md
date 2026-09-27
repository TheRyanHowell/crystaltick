# CrystalTick

A from-scratch Pebble Time 2 (`emery` platform) watchface, independently inspired by "Quartz by
Dalpek." See `docs/design.md` for full design rationale and `docs/dev-environment.md` for toolchain
setup (this project is built inside a `toolbox`/container, not on the host directly).

## Organization

- `src/c/` — watch-side C. `main.c` (lifecycle/wiring), `settings.c` (persisted settings + Clay
  AppMessage parsing), `complication.c` (steps/heart-rate/temperature + HealthService lifecycle),
  `screen_care.c` (theme colors, day/night polarity), `logic.c` (pure decision logic, no Pebble SDK
  dependency — this is the only file unit-tested on the host), `draw.c` (all rendering; layout
  constants live here with rationale comments).
- `src/pkjs/` — phone-side JS. `config.js` (Clay settings schema), `index.js` (Clay wiring + weather
  fetch).
- `resources/fonts/` — DSEG7 "ClassicMini" font (SIL OFL 1.1).
- `tests/` — host-compiled C unit tests for `logic.c` only (everything else depends on the Pebble
  SDK and is verified via QEMU/real-device screenshots instead, not unit tests).
- `scripts/` — lint/static-analysis/coverage helper scripts `npm run *` shells out to.
- `docs/` — design rationale and dev environment setup.

## Commands — VERIFY BEFORE CLAIMING DONE

```bash
npm test              # host-compiled unit tests for logic.c (gcc, no Pebble SDK needed)
npm run lint           # clang-format check + cppcheck (--enable=all) + eslint
pebble build           # actual ARM cross-compile - catches real build errors tests can't
```

Do not consider a change to `draw.c`/`main.c`/etc. done until `pebble build` succeeds — the C unit
tests only cover `logic.c`. For any visual change, install to the emulator and screenshot it; do not
rely on reading the code alone.

```bash
pebble install --emulator emery
pebble screenshot --no-open --emulator emery screenshot_emery.png   # then read the PNG
```

For settings/AppMessage changes specifically, emulator testing isn't sufficient — see the Clay
gotcha below. Verify on real hardware if at all possible: `pebble install --phone <ip>`,
`pebble logs --phone <ip>`.

## Code style

- `clang-format` (config in `.clang-format`, matches PebbleOS's own style) for C.
- `eslint` (`.eslintrc.json`) for the `src/pkjs/` JS.
- Comments explain *why*, not *what* — especially in `draw.c`'s layout constants, which have
  hard-won rationale (measured values, prior failed attempts) worth preserving. Don't strip these
  when refactoring.

## Critical gotchas (read before touching layout or settings code)

Full detail in `docs/design.md`; summary:

1. **`GTextOverflowModeFill` does not pixel-clip** — an undersized `GRect` silently truncates text to
   "…" rather than clipping a few pixels. Always measure real content size with
   `graphics_text_layout_get_content_size()` before finalizing a box, never estimate from font point
   size. This caused multiple broken-looking visual regressions early in this project.
2. **A custom font resource's pixel height is parsed from the first digit run in its `name`** (e.g.
   `FONT_DSEG7_46` builds at height 7, not 46, because "7" in "DSEG7" matches first). Name resources
   so the only digit run is the intended size.
3. **Clay's `select` items always send a string value over AppMessage**, regardless of whether
   `config.js` declares the option `value` as a JS number or string — Clay's select is backed by a
   real HTML `<select>`, whose DOM `.value` is always a string. `settings.c` must check `tuple->type`
   and parse `TUPLE_CSTRING` with `atoi()`; reading `tuple->value->int32` unconditionally silently
   misreads the string's bytes and makes the setting appear to do nothing when changed. This bit every
   `select`-backed setting in this project at once; toggles/sliders were unaffected (different DOM
   elements, already send proper `TUPLE_INT`).
4. **A settings blob persisted by buggy parsing stays corrupted after the parsing code is fixed** —
   the watch only re-parses on a fresh AppMessage. After fixing a settings bug, the phone-side Clay
   config page needs to be re-opened and saved once to send corrected values.
5. **`fonts_get_system_font()` results must never be passed to `fonts_unload_custom_font()`.**

## Git rules

- Commit only when explicitly asked.
- Keep commits focused; don't bundle unrelated layout tweaks with logic fixes.
