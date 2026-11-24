# Runtime architecture

## Two execution contexts
- **Arduino `loop()`** (main task, core 1 = `CONFIG_ARDUINO_RUNNING_CORE`): config change handling, button events,
  `tick()` of the current mode (BCP: data refresh, `updateDisplay()`), `delay(10)`.
- **`Tc001::handleEvents`** FreeRTOS task (`xTaskCreatePinnedToCore`, core 1, priority 1, 8 KB stack),
  runs every 10 ms: button polling, `Config.handleClient()`, scrolling text / progress indicator
  steps, and redrawing the countdown when `displayState == DISPLAY_EVENT_COUNTDOWN` (once per second,
  when `time(nullptr)` changes; the dev console `status` shows the redraw count).
- Both contexts draw to the same `FastLED_NeoMatrix` and share `scrollingText`/`progressIndicator`.
  All display access in `tc001.cpp` goes through a recursive mutex (`DisplayLock`); keep new drawing
  code inside it and keep slow work (HTTP, config server) outside.
- Both tasks run on core 1 with the same priority, so FreeRTOS time-slices them; a blocking HTTPS
  request in `loop()` does not stop the countdown redraw (measured 1 redraw/s during fetches). Core 0
  is left to the WiFi stack on purpose: the display task next to it risks FastLED flicker.

## Modes (`src/mode.h`, `src/main.cpp`)
- A `Mode` owns the display content and the buttons while it is current: `enter()` / `leave()`, `tick()` from
  `loop()`, `onButton()` for Left, Right and a short Mid press (returns `MODE_NEXT` to switch), `onConfigUpdated()`.
  It draws when `Hw::redrawRequested` is set (at boot, after the mode name or the config page address).
- `main.cpp` lists the modes of the build (`#if BCP_MODE_*`) in the switching order, handles the long Mid press
  (config page) and the switching: `leave()`, save the `ModeId` (`lib/core/modeid.h`) to NVS, `showModeName()`,
  `enter()`. At boot it enters the stored mode (the first one if the build does not contain it). A build with a
  single mode ignores the short Mid press (only ends a message).
- BCP mode (`src/bcpmode.*`): the former main loop. Leaving it switches the connection alert off and gives a
  config server running with no timeout (invalid event ID) the normal timeout; entering it refreshes at once.
- Offline mode (`src/offlinemode.*`): one `ManualTimer` (`lib/core/manualtimer`: ready / running with an end
  epoch / paused with the remaining seconds), button logic in `handleOfflineButton()`; saved with
  `Config.saveState()` at every change and loaded at the first `enter()`, so it runs on while the clock is off or
  in another mode. A countdown without a round indicator is drawn one row lower (vertically centred).
- Network: `Mode::needsNetwork()` (BCP mode). At boot a mode that needs it calls `ensureConnection(required)`
  (portal with timeout); other modes try the stored networks only (`WifiMgr.connect(..., allowPortal=false)`,
  portal only with Mid held) and run without WiFi, the time then comes from the RTC (`Tc001::setTimeFromRtc()`,
  it keeps the time while the clock is off). Switching to a mode that needs the network while offline connects
  first. A short Mid press during the connection (`Tc001::connecting`) leaves the portal (`WifiMgr.abortPortal()`)
  when the build has a mode without network, which is entered instead; other presses are dropped. Long Mid
  without WiFi shows `No WiFi` (like a mode name).
- A countdown is passed to the `Hw` as a `CountdownDisplay` copy (`lib/core/countdown.h`: one `CountdownTimer` per
  player, thresholds, round indicator), never as a pointer to mode data, so `handleEvents` never reads data that
  `loop()` is changing. `displayCountdown()` starts a countdown (the first redraw never beeps),
  `updateCountdown()` passes new data of the shown one (BCP refresh, pause) and keeps the threshold memory.

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
- `CONFIG_MESSAGE` — config server started by a long Mid press: the address scrolls once, data refreshes
  do not redraw meanwhile; when it finishes, `handleEvents` sets `Hw::redrawRequested` and the mode
  redraws (BCP: `updateDisplay()` without fetching data).
- `MODE_NAME` — after a mode switch: the name in f3x5, centred for 5 s if it fits, else scrolled once; then
  `Hw::redrawRequested` like `CONFIG_MESSAGE` (`isMessageShown()` covers both).
- Any button press ends `CONFIG_MESSAGE` / `MODE_NAME` at once (`Tc001::nextButtonEvent()` → `finishMessage()`),
  then the button does its action.

## Status overlay, battery and connection (`Tc001`, `lib/core/battery`, `lib/core/statusalert`)
- `Tc001::showMatrix()` replaces every `matrix.show()`: it saves the content, draws the overlay over it, shows
  the matrix and restores the content, so `matrixleds` never contains the overlay and the drawing code ignores
  it. `stepAnimations()` requests a show when `overlayState()` changes (blink, icon phase), also on static screens.
- Status indicators (`enum StatusIndicator`: low battery red, WiFi amber, BCP magenta) blink 1 s on / 1 s off in
  column 0, stacked from the top in enum order without gaps. A new indicator = enum entry + colour + where it is set.
- Alert icons (`enum AlertIcon`: battery, WiFi, BCP) alternate with the content, 5 s each, content first, then
  each active icon in enum order (`alertCyclePhase`); not while the config page address scrolls once after a
  long Mid press or the mode name is shown (`isMessageShown()`). Amber is `Color(255, 140, 0)`, (255, 77, 0) on the LEDs after the
  gamma correction of Framebuffer_GFX (a lower green looks red).
- Battery voltage on GPIO 34 (ADC1, works with WiFi), read by `stepBattery()` in `handleEvents` once a second.
  `BatteryMonitor` keeps the median of the last 15 readings (no level for the first 15 s) and compares it with
  raw thresholds measured by one discharge (2026-11-24, full brightness, white countdown: 2514 after
  unplugging, off at 2256 after 112 min, ~2700 when charging): LOW < 2321 (~30 min left), CRITICAL < 2288
  (~10 min left), a level is left 10 above its threshold (hysteresis). Fewer lit LEDs = less voltage sag, so
  the time left is approximate (+-5 min). Recalibrate with the dev console `battlog`.
- CRITICAL: the 15x7 battery icon joins the icon cycle.
- Connection: after each data refresh `main.cpp` updates `ConnectionAlert` (`nextConnectionAlert`) and passes it
  to `Hw::setConnectionAlert()`. A failed refresh with WiFi disconnected = WiFi problem (amber pixel; 25x8 icon: amber WiFi symbol crossed out
  in red + red "Wi-Fi"),
  with WiFi up (connection or HTTP error) = BCP problem (magenta, 24x5 unplugged plug icon); a successful refresh
  clears it, so the indicator reacts within one refresh interval (60 s). The icon joins the cycle from the 4th
  consecutive failed refresh of any kind.
- Reboot (`decideRefresh`): WiFi outages and BCP connection errors (status < 0) share `failCount`; the 5th in a
  row reboots (about 5 min, so the icon shows the last minute before it). HTTP errors reset the count and never
  reboot. The captive portal at boot is not affected (no refresh runs there).

## Buttons (TC001 pins)
- Left (GPIO 26), Mid (GPIO 27), Right (GPIO 14), active low with pull-ups. Polled by `Tc001::stepButtons()` in
  `handleEvents` every 10 ms once the WiFi connection is up (`PressDetector` in `lib/core/buttons`: 30 ms debounce);
  events go through a FreeRTOS queue to `loop()` (`Hw::nextButtonEvent()`). A button held at that moment counts
  only after its release (Mid held at power-on for the portal).
- Left / Right: an event at the press. Mid: short = released within 1 s, long = held for 1 s (reported at once,
  without waiting for the release).
- Mid held at boot → force WiFi captive portal; long Mid → config server (5 min timeout); short Mid → the mode
  (usually the next mode). BCP mode: Right → immediate data refresh. Dev console: `left`, `right`, `mid`, `midlong`.
- LED matrix data on GPIO 32.

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
- Namespace `config`: `event` (string, ID or URL as entered), `yellow`, `red` (long seconds; legacy string `m:ss` is still read), `beep` (bool, acoustic signals, default on), `hostname`, `mode` (uchar `ModeId` of the last mode, saved at each switch), `offline` (long seconds, offline mode length).
- Namespace `state`: plain data of the modes (`Config.loadState()` / `saveState()`, ignored when the size differs):
  `offline` (`ManualTimer`).
- Namespace `wificreds`: `__ssids` (newline-separated list), `pwd_<fnv1a32(ssid) hex>` per network.
- Config web form (`/`, `POST /config`, `/wifi/delete`, `/wifi/deleteAll`) also lists/deletes stored WiFi networks.

## Time
Countdown = `roundEndEpoch - time(nullptr)`, so the system clock must be valid UTC. `WifiMgr` syncs NTP
(`pool.ntp.org`) after each connection and writes it to the DS1307-compatible RTC; if NTP fails, a
plausible RTC time is used instead, otherwise the connection attempt counts as failed.
