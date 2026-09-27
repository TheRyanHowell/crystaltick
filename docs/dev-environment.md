# Dev environment setup

On an immutable/atomic Linux host (Fedora Silverblue/Kinoite and similar), don't install the SDK
toolchain on the host directly — use a `toolbox` (or any container) instead. This is exactly what
was used to build and test CrystalTick throughout development.

```bash
toolbox create pebble-dev
toolbox enter pebble-dev
```

## System packages

```bash
sudo dnf install -y \
  nodejs npm SDL2 glib2 pixman zlib curl gcc make \
  qemu-system-arm \
  clang-tools-extra cppcheck lcov
```

- `nodejs`/`npm` — for `@rebble/clay` and eslint (the pkjs side).
- `SDL2`/`glib2`/`pixman`/`zlib` — QEMU emulator dependencies for `pebble install --emulator`.
- `qemu-system-arm` — the actual emulator; the Pebble SDK ships the watch firmware images but not
  QEMU itself.
- `clang-tools-extra` (for `clang-format`), `cppcheck`, `lcov` — the static analysis / coverage
  tooling `npm run lint` and `npm run coverage:html` shell out to.

## Pebble CLI + SDK

The current `pebble` CLI is installed via [`uv`](https://docs.astral.sh/uv/), not pip directly:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
~/.local/bin/uv tool install pebble-tool
export PATH="$HOME/.local/bin:$PATH"

pebble sdk install latest   # pulls the ARM toolchain + QEMU firmware images (SDK 4.33.1 as of
                             # this writing includes emery and gabbro platform support)
```

Verify the emery (Pebble Time 2) platform is present:

```bash
ls ~/.local/share/pebble-sdk/SDKs/*/sdk-core/pebble/emery/qemu/
```

## Font metrics tooling (optional, for layout work)

Picking font sizes/box dimensions for `draw.c` relies on measuring actual rendered text rather than
guessing (see the `GTextOverflowModeFill` gotcha in `docs/design.md`) — either via a temporary
`APP_LOG` + `graphics_text_layout_get_content_size()` in the app itself (the most reliable method,
since it measures exactly what the device will render), or via `fontTools`/`Pillow` on the host for
inspecting a font file's raw glyph metrics/outlines directly:

```bash
uv venv /tmp/fttools_venv
uv pip install --python /tmp/fttools_venv/bin/python fonttools Pillow
```

## Everyday commands

```bash
cd crystaltick
npm install
pebble build
pebble install --emulator emery
pebble screenshot --no-open --emulator emery screenshot_emery.png
```

For real-hardware testing, enable the developer connection in the Pebble phone app (Settings →
Developer) and use `--phone <ip>` in place of `--emulator emery` for `install`/`logs`/`screenshot`.
The developer WebSocket connection can drop between commands — if `pebble logs`/`screenshot` starts
returning `Connection refused` after a successful `install`, re-enable it on the phone.
