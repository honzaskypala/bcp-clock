# Runtime architecture

## Two execution contexts
- **Arduino `loop()`** (main task, core 1 = `CONFIG_ARDUINO_RUNNING_CORE`): config change handling, data refresh, `updateDisplay()`, `delay(10)`.
- **`Tc001::handleEvents`** FreeRTOS task (`xTaskCreatePinnedToCore`, core 1, priority 1, 8 KB stack),
  runs every 10 ms: mid-button handling, `Config.handleClient()`, scrolling text / progress indicator
  steps, and redrawing the countdown when `displayState == DISPLAY_EVENT_COUNTDOWN` (once per second,
  when `time(nullptr)` changes; the dev console `status` shows the redraw count).
- Both contexts draw to the same `FastLED_NeoMatrix` and share `scrollingText`/`progressIndicator`.
  All display access in `tc001.cpp` goes through a recursive mutex (`DisplayLock`); keep new drawing
  code inside it and keep slow work (HTTP, config server) outside.
- Both tasks run on core 1 with the same priority, so FreeRTOS time-slices them; a blocking HTTPS
  request in `loop()` does not stop the countdown redraw (measured 1 redraw/s during fetches). Core 0
  is left to the WiFi stack on purpose: the display task next to it risks FastLED flicker.

## Display state machine (`DisplayState` in `lib/core/displaystate.h`)
`displayState` says what is shown. Only the `Hw` implementation sets it (each `display*()` /
`showSplashScreen()` / `showConfigServerMsg()` method sets its state); `main.cpp` only reads it to skip
redrawing what is already shown.
- `BOOT` — splash screen or connection messages (also while the event ID is not verified yet).
- `EVENT_NAME` — event not running (not started or ended): scrolling event name.
- `EVENT_ROUND` — event running, no timer for the current round: "Round N".
- `EVENT_COUNTDOWN` — timer active; `handleEvents` redraws every second. Colours: green = time to round
  start, white = running, yellow/red = below configured thresholds, blinking magenta = paused.
  Bottom row shows round progress (`displayRoundWithCountdown`). When the remaining time crosses
  the yellow / red threshold between two redraws, `drawCountdown` starts a beep (see Buzzer below).
- `CONFIG_REQUIRED` — invalid event ID: red message once, then the config page address loops; config
  server runs with no timeout.
- `CONFIG_MESSAGE` — config server started by the Mid button: the address scrolls once, data refreshes
  do not redraw meanwhile; when it finishes, `handleEvents` sets `Hw::redrawRequested` and `loop()`
  calls `updateDisplay()` without fetching data.

## Buttons (TC001 pins)
- Mid (GPIO 27): held at boot → force WiFi captive portal; pressed at runtime → start config server (5 min timeout).
- Right (GPIO 14): immediate data refresh (`Hw::enforceUpdate`).
- Left button is not used. LED matrix data on GPIO 32.

## Buzzer (GPIO 15, passive)
- Idles as `INPUT_PULLDOWN`; a floating pin whistles. A beep attaches LEDC channel 0 (`ledcWriteTone`, 2 kHz) and
  pulls the pin down again when the pattern ends.
- `Tc001::stepBeep()` runs in `handleEvents` every 10 ms, so the beep never blocks the countdown. Patterns and
  threshold crossing are in `lib/core/countdown` (`detectThresholdCrossing`, `isBeepOn`): yellow = one 2 s beep,
  red = three 2 s beeps with 1 s gaps. `lastRemaining` is reset in `displayCountdown()`, so the first redraw
  (boot or return from the config message in the middle of a round) never beeps. Disabled by config `beep`.

## Startup flow
`Tc001` ctor (matrix, task) → splash → `WifiMgr.connect()` (blocking; portal if no known/open network
connects or NTP fails) → `BCPEvent.setID(Config.eventId())`. An invalid/empty event ID starts the config
server with no timeout and scrolls "Config page http://<IP>".

## Persistent storage (NVS / Preferences)
- Namespace `config`: `event` (string, ID or URL as entered), `yellow`, `red` (long seconds; legacy string `m:ss` is still read), `beep` (bool, acoustic signals, default on), `hostname`.
- Namespace `wificreds`: `__ssids` (newline-separated list), `pwd_<fnv1a32(ssid) hex>` per network.
- Config web form (`/`, `POST /config`, `/wifi/delete`, `/wifi/deleteAll`) also lists/deletes stored WiFi networks.

## Time
Countdown = `roundEndEpoch - time(nullptr)`, so the system clock must be valid UTC. `WifiMgr` syncs NTP
(`pool.ntp.org`) after each connection and writes it to the DS1307-compatible RTC; if NTP fails, a
plausible RTC time is used instead, otherwise the connection attempt counts as failed.
