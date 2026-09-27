# Debugging playbook

Concrete techniques that actually found and fixed real bugs during this project, in the order you'd
reach for them.

## "A visual change doesn't look right"

Don't debug from reading the code — take a screenshot and look at actual pixels. A small screenshot
viewed inline is easy to misread (a faint outline circle can look like a solid filled block at
thumbnail size — this genuinely happened during development and led to a wrong bug report before a
zoomed crop corrected it). Always upscale before drawing conclusions:

```python
from PIL import Image
im = Image.open("screenshot_emery.png")
im.resize((im.width * 4, im.height * 4), Image.NEAREST).save("zoomed.png")
# or crop a specific region first for an even closer look:
im.crop((x0, y0, x1, y1)).resize((..., ...), Image.NEAREST).save("zoomed_crop.png")
```

(`Image.NEAREST` matters — any smoothing resample will blur exactly the pixel-level detail you're
trying to inspect.) Requires Pillow in whatever Python environment you're running this from; see
`docs/dev-environment.md` for setting up a `uv venv` with it inside the toolbox.

## "Text is clipped/truncated and I don't know why"

This is almost always `GTextOverflowModeFill`'s truncate-to-"…" behavior on an undersized box, not a
partial-pixel clip (see `docs/design.md`). Don't guess a bigger size — measure the actual rendered
content:

```c
GSize measured = graphics_text_layout_get_content_size(
    "the exact string", the_font, GRect(0, 0, 200, 80), GTextOverflowModeFill, GTextAlignmentCenter);
APP_LOG(APP_LOG_LEVEL_DEBUG, "DEBUG measured %dx%d", measured.w, measured.h);
```

Use a generously large measurement box (200x80 or so) so the measurement itself can't be the thing
truncating — this exact mistake (measuring in a too-short box) produced a false "two letters have
identical width" reading earlier in this project, corrected by re-measuring with more headroom.

## "A font looks wrong / a glyph looks unrecognizable"

Don't assume — render the actual glyph outline and look at it. This caught DSEG14's uppercase 'B'
being a genuinely unrecognizable stylized shape (not a rendering bug, just a bad glyph in that
specific font) and confirmed 'M'/'W' are different outlines that just happen to look similar (a real,
known limitation of 14-segment displays generally, not a bug):

```python
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen
from PIL import Image, ImageDraw

f = TTFont("path/to/font.ttf")
cmap = f.getBestCmap()
gs = f.getGlyphSet()
pen = RecordingPen()
gs[cmap[ord("B")]].draw(pen)
# walk pen.value's (moveTo/lineTo/qCurveTo/closePath) commands to build polygons,
# then draw them with PIL to actually see the shape.
```

For quick metrics only (advance width, bounding box) without rendering:

```python
from fontTools.pens.boundsPen import BoundsPen
pen = BoundsPen(gs)
gs[cmap[ord("W")]].draw(pen)
print(pen.bounds)
```

## "A setting doesn't do anything when changed"

This is what found the `TUPLE_CSTRING` bug (see `docs/design.md`) — don't guess at the wire format,
log it:

```c
Tuple *tuple = dict_find(iter, key);
APP_LOG(APP_LOG_LEVEL_DEBUG, "DEBUG key=%lu type=%d len=%d int32=%ld",
        (unsigned long)key, tuple->type, tuple->length, (long)tuple->value->int32);
```

`tuple->type` values: `TUPLE_BYTE_ARRAY=0`, `TUPLE_CSTRING=1`, `TUPLE_UINT=2`, `TUPLE_INT=3`. If a key
you expect to be numeric shows up as `type=1`, you're reading `tuple->value->int32` on a string —
that reads uninitialized/adjacent bytes, not the actual value, and tends to produce a *consistent*
wrong number across different real selections (since the surrounding dictionary bytes are similar
each time), which looks exactly like "changing the setting does nothing." Fix: branch on
`tuple->type` and use `atoi(tuple->value->cstring)` for the string case.

Capture logs from a real phone/watch to see this while actually operating the settings page (the
emulator can't exercise the real Clay/phone serialization path):

```bash
pebble logs --phone <ip> > phone-logs.txt &
# ...change the setting on the phone, hit Save...
cat phone-logs.txt
```

If the developer connection has dropped (`Connection refused` on a command that worked a minute
ago), it needs re-enabling on the phone (Pebble app → Settings → Developer) — not fixable from the
CLI side.

**After fixing the parsing code**, the persisted settings blob on the watch is still corrupted with
whatever the buggy code last saved — the fix only takes effect on the *next* AppMessage. Re-open the
Clay config page and hit Save once more (even without changing the value) to confirm the fix, not
just reinstalling the app.

## Getting an independent second opinion

For visual/design review specifically, a fresh subagent with no memory of the implementation attempts
tends to catch things the implementing session has gone blind to — particularly useful for verifying
a claimed fix actually landed rather than trusting the implementer's own read of a screenshot. Give it:
the current screenshot, any reference images, the relevant source file (so it can cite specific named
constants rather than vague pixel descriptions), and an explicit list of what's already been decided
and shouldn't be re-litigated (to keep it from re-flagging settled trade-offs). Ask for a prioritized,
concrete punch list, most severe first, and to stay silent on anything it can't verify is actually
wrong. This caught, across two rounds on this project, several fixes that had been attempted but
hadn't actually landed (a "fixed" battery-weight issue that was still measurably heavier than the
time; a "fixed" 3-dot icon where two of the three dots were still touching) — both only caught because
the second round re-measured rather than assuming the first round's fix worked.
