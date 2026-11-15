# Non-obvious contracts and code style

## Non-obvious contracts

- **`Tc001` is a `Print`** passed to `WifiMgr.connect()` as the user-message sink. `Tc001::write()` buffers
  lines and dispatches on the line prefix (`classifyMessage()` in `lib/core/policy`): `"Error:"` → red scrolling text, `"Connecting "` → "Connect" +
  progress indicator, `"Connected "` → ignored, anything else → white scrolling text. Changing message
  texts in `wifimgr.cpp` changes what the display does.
- **Fonts** (`lib/gfx`):
  - `UTF8_32BitFont` extends `GFXfont` with 32-bit `dFirst/dLast`. `F3x5[]` is a *segmented* font: an
    array of segments (ASCII, Latin-1 + Latin Extended-A, Cyrillic, punctuation, arrows, regional indicators)
    terminated by an all-null entry. Code walking segments relies on that terminator.
  - Time fonts `F4x6` (mm:ss) and `F3x5_Fixed` (h:mm:ss) only cover `-` to `:`; the `/` glyph is an `X`
    bitmap, used to display overflow (`-//://`).
  - Adafruit's `getTextBounds` clips to the display, so `ScrollingText` has its own `measureText`/`measureChar`.
- Singletons: `CBCPEvent`/`CConfig` use private ctor + friend `get...Instance()` + global reference
  (`BCPEvent`, `Config`); `Tc001::getInstance()`; `WifiMgr` is a plain global object.

## Code style

- File header: one-line description, `// (c) 2025 Honza Skýpala`, `// WTFPL license applies`.
- Classes `CName` for singletons, members with trailing underscore (`name_`), methods camelCase.
  (wifimgr/config have some older `_prefix` members.)
- Function and method names start with a verb (`updateDisplay()`, `sealSnapshot()`, `startProgress()`).
  Getters follow the Qt style: value getters are nouns (`name()`, `eventId()`), bool getters use
  `is`/`has` (`isStarted()`, `hasTimeSyncFailed()`). `xToY()` conversions are fine (`secondsToTime()`).
  Overrides of Arduino/Adafruit methods keep their names (`write`, `setFont`, `draw`).
- Per-module debug macros (`MAIN_DEBUG`, `CONFIG_DEBUG`, `WIFIMGR_DEBUG`) are aliases of `DEV_LOG("Module", msg)`,
  which prints `[Module] msg` to `Serial` in the dev build only.
- Arduino `String` is used widely; the TODO lists moving to `const String&` / `char*` where it makes sense.
- Section comments in the form `// ---- Section ----`.
