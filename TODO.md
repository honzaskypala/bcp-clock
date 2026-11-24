# TODO for BCP-clock project

## main related

## TC001 hardware related
- [ ] bliking every 0.5 sec instead of 1 sec
- [ ] blink red (over time) always
- [ ] blink green (time to start) last 2 minutes
- [ ] show finished tables + config top / bottom / none

## TC002 support (see docs/tc002.md)
Requirements: stock firmware and its apps stay, BCP Clock is an extra app; everything runs on the clock (no NAS/PC). Waiting for the device.
- [ ] Feasibility check on the device, firmware untouched (adb, `/tmp`): static ARMv7 hello world; stock API reachable on `127.0.0.1:80`; push a `db` frame to `/api/custom?name=bcp` – shown in the carousel? survives reboot? max push rate?; DNS, HTTPS to BCP with own CA bundle, NTP; free memory
- [ ] Make `lib/core` platform neutral (`std::string` / `char*` instead of Arduino `String`, or a small `String` shim)
- [ ] Interfaces for HTTP fetch, settings storage and config web server (ESP32 implementations unchanged)
- [ ] Linux build (CMake, static ARMv7: zig or `arm-linux-musleabihf`) next to PlatformIO
- [ ] `Hw` implementation for 52x16: render to a framebuffer, push it to the stock custom-app API (no SPI); layout for the larger display
- [ ] Config page on its own port (e.g. 8080), settings file in `/data`; WiFi stays with the stock firmware
- [ ] Autostart: back up mtd3, repack `res` with a bootstrap library that starts our daemon and then the stock `libzkgui.so`; test recovery; reinstall after official OTA updates

## GFX related
- [ ] add negative chars to the font, eg. 🅽
- [ ] [emojis](https://unicode.org/emoji/charts/full-emoji-list.html) support
- [ ] emoji graphens support, eg. country flags
- [ ] clip on text buffer
- [ ] rainbow effect on screen, option to limit it on a clipped area

## Config related
- [ ] option to have config web password protected
- [ ] refresh interval configurable
- [ ] option to use small font always (even when displaying mm:ss only)
- [ ] option to show rounds at top / bottom / none

## WiFi Manager related
- [ ] add callback to to validate established connection. Move NTP timesync out of WiFi Manager and have it run through the callback.
- [ ] test for captive portal and consider a failure if we cannot get real public internet response

## General

## Done
- [x] [Config] [Bug] Reboot on a long Mid press: when the previous config server had stopped on its timeout, the event task saw the stale timeout while `loop()` was still starting the new server and stopped it (`MDNS.end()` during `MDNS.begin()` → `assert failed: vQueueDelete`); start, stop and `handleClient()` of the config server now run under a recursive mutex in `CConfig` (also covers BCP mode stopping the server while a request is served); reproduced and verified on the TC001 with `midlong` after the timeout
- [x] [Config] [Feature] Display brightness 10–100 % (step 10, default 100 %) on the config page (section Display, range slider), NVS `config/bright`, applied at startup and after a save (`Hw::setBrightness`, FastLED brightness); build feature `BCP_FEATURE_BRIGHTNESS` (+1 KB flash in `tc001`); `lib/core/brightness` with native tests
- [x] [Main] [Feature] Chess clock (feature `BCP_MODE_CHESS`): round length on the config page (default 1:00:00), half per player; Right = left player on turn (blue bar at the left), Left = right player (bar at the right), short Mid = pause / reset / next mode; thresholds halved, beeps per player; TC001 shows the player on turn, `CountdownDisplay` carries both (TC002 can show both); a time touching the bar (hh:mm:ss) moves one pixel; state in NVS (`state/chess`), runs on while the clock is off; logic in `lib/core/chessclock` (two `ManualTimer`s)
- [x] [Main] [Feature] Offline mode (feature `BCP_MODE_OFFLINE`): countdown of a length set on the config page (default 1:00:00), Right = start / resume, short Mid = pause / next mode, Left = reset when stopped; same colours, thresholds and beeps as BCP, no round indicator, no connection monitoring; computed from the end time and saved in NVS (`state/offline`), so it runs on while the clock is off; logic in `lib/core/manualtimer`; boot without WiFi in a mode that does not need it (stored networks only, then time from the RTC), short Mid leaves the portal of BCP mode to offline mode, long Mid without WiFi shows `No WiFi`; durations accept h:mm:ss (`1:00:00`, also the thresholds; signed values fixed); countdown without round indicator one row lower; a finished scrolling text no longer ends the next message; new build config `tc001-offline` (no WiFi, ~370 KB)
- [x] [Main] [Feature] Modes: `Mode` interface (`src/mode.h`), the former main loop is BCP mode (`src/bcpmode.*`, feature `BCP_MODE_BCP`); short Mid = next mode, long Mid (1 s, without waiting for the release) = config page; mode name in f3x5 for 5 s or scrolled once, any button ends it (also the config page address); last mode stored in NVS (`config/mode`); buttons polled in handleEvents (`lib/core/buttons`, 30 ms debounce, queue to loop), Left button read; countdown passed to the Hw as a `CountdownDisplay` copy; mm:ss shows total minutes (a one hour countdown starts at 60:00); dev console `left`, `right`, `mid`, `midlong`
- [x] [General] [Build] Build by features: build configurations in `configs/*.ini` (PlatformIO `extra_configs`, `-D BCP_FEATURE_*=0|1`), checks and dependencies in `lib/features/buildfeatures.h`; WiFi manager, mDNS, battery monitoring and connection monitoring are features (code, config page sections, dev console commands); envs renamed `nodemcu-32s` → `tc001`, `dev` → `tc001-dev`, new `tc001-minimal` (-33 KB flash); `tools/build_configs.sh` builds every configuration (pre-commit hook, release workflow); docs/features.md
- [x] [Hw] [Battery] Battery thresholds calibrated by one discharge (full brightness, white countdown, 112 min on battery, ADC median 2514 -> 2256): low < 2321 (~30 min left), critical < 2288 (~10 min left), hysteresis 10; percentages removed; dev console `battery [raw|off]` and `battlog [start [sec]|stop]` (calibration log in NVS)
- [x] [Hw] Rounds >= 10 hours: countdown in hh:mm:ss shifted 2 px left (x=1) so it fits the display; a round starting with two-digit hours keeps them until the end (09:59:59, ...), also paused and time to start >= 10 h; over time stays -h:mm:ss (-hh:mm:ss does not fit)
- [x] [Main] [Hw] Connection indicator: blinking status pixel amber (WiFi lost) or magenta (WiFi up, BCP connection or HTTP error), from the 4th failed refresh in a row a WiFi icon / unplugged plug icon in the alert cycle (content, battery, connection icon, 5 s each); WiFi outages now count toward the reboot after 5 failures like BCP connection errors; icon cycle moved to `lib/core/statusalert`; dev console `wififail on|off`, connection alert in `status`, `A` (amber) in `screen`; flash stays at 85.4 %
- [x] [Hw] [Battery] Low battery warning: < 10 % blinking red status pixel top left, < 5 % red empty battery icon alternating with the content every 5 s (not during the config message after Mid); battery on GPIO 34, median of 15 readings, hysteresis 2 %; status overlay in `Tc001::showMatrix()` ready for more indicators stacked in column 0; dev console `battery [pct|off]`; flash +6.7 kB (84.9 % -> 85.4 %, of which 5.6 kB is the Arduino ADC driver), RAM +1 kB
- [x] [Config] [Refactor] mDNS moved from main.cpp to CConfig: announced only while the config server runs, restarted there on a hostname change
- [x] [General] [Release] v1.0.2: install page loads the manifest bypassing the GitHub Pages cache (max-age=600 kept showing and offering the previous release)
- [x] [General] [Release] v1.0.1: installer page and README say to tick "Erase device" on the first installation (ESP Web Tools leaves it unticked by default)
- [x] [General] [Release] First release v1.0.0: browser installer (ESP Web Tools, `install/index.html`) on GitHub Pages, `tools/build_release.sh` builds the firmware with `BCP_VERSION` and the merged image, `.github/workflows/release.yml` publishes a GitHub Release and the installer on a pushed `v*` tag; firmware version in the configuration page footer (+128 B, 84.9 %); README Installation rewritten for non-developers, work-in-progress warning removed
- [x] [General] [Docs] README tagline: works with any game system, as long as the tournament is run on BCP
- [x] [General] [Docs] README pictures in `docs/img`: cover photo, animated GIF of all display states (dark/light variant via `<picture>`), WiFi portal and configuration page screenshots, labelled button photo; "Acoustic signals" row in the Configuration table, "Click Connect" in First setup
- [x] [General] [Docs] CLAUDE.md slimmed to ~100 lines to save context; details moved to `docs/` (`architecture.md`, `dev-build.md`, `bcp-api.md`, `contracts.md`, `tc002.md`)
- [x] [TC001] [Feature] Acoustic signals at thresholds: one 2 s beep when the countdown reaches the yellow threshold, three 2 s beeps (1 s gaps) at red. Non-blocking (`Tc001::stepBeep` in `handleEvents`, LEDC square wave on GPIO 15, pin pulled down again afterwards), no beep on the first redraw or while paused. Crossing detection and patterns in `lib/core/countdown` (native tests). Config checkbox "Acoustic signals" (NVS `beep`, default on), dev console `beep yellow|red`. Production firmware 7 628 B larger (84.3 % → 84.9 %)
- [x] [General] [Refactor] Debug output merged with the dev build: `DEV_LOG` macro (`lib/devlog/devlog.h`) prints to `Serial` only with `BCP_DEV`, in production it emits nothing; `Print *debugOut` parameters/members removed from `Tc001`, `WifiMgr.connect`, `Config` and `main.cpp`, unused `Tc001::debugPrintln` removed. Production firmware 10 348 B smaller (85.1 % → 84.3 %)
- [x] [General] [Refactor] Long functions split (target ≤ 30 lines): WiFiMgr `startConfigPortal` (`startAccessPoint`, `registerPortalRoutes`), `connect` (`runPortal`), `handleRoot` (`appendScannedNetworksHtml`, page CSS/script as constants); Config `handleConfigRoot` (`statusMessage`, `appendConfigFormHtml`, CSS/script constants); `updateDisplay` (`collectDisplayInputs`, `applyDisplayAction`); `formatCountdown` (`formatTime`); `decideDisplay` (`decideValidEvent`); Tc001 `showMessage` (`showErrorMessage`, `showInfoMessage`); `ScrollingText::append` reuses `createCanvas`. No behaviour change, firmware 196 B larger
- [x] [WiFi Manager] [Bugfix] Memory leak: `scanAndSort()` now calls `freeScanData()` before scanning, so a Rescan in the captive portal no longer leaks the previous scan arrays (a failed Rescan leaves the list empty)
- [x] [General] [Refactor] Less nesting (guard clauses, extracted functions): WiFiMgr `connect` (known/open network passes) and `scanAndSort` (driver/fallback results, sort), shared `readStoredList`; Tc001 `handleEvents` (`handleMidButton`, `stepAnimations`) and `write` (`showMessage`); `loop()` (`handleConfigUpdate`, `refreshEventData`); round indicator layout; Config guard clauses. UTF-8 decoding and font segment lookup shared by UTF8canvas16 and ScrollingText (`lib/gfx/fontsegment.h`, inline). No behaviour change, firmware 20 B smaller
- [x] [General] [Refactor] Function and method names start with a verb (e.g. `updateDisplay`, `sealSnapshot`, `showSplashScreen`, `handleEvents`, `WebUI::buildPageStart`/`escapeHtml`); bool getters use `is`/`has` (`isValid`, `isStarted`, `hasTimeSyncFailed`), value getters stay nouns. No behaviour change, firmware size unchanged
- [x] [General] [Tests] Device test firmware silences the buzzer as the first thing in `setup()` (the TC001 whistled during and after device tests)
- [x] [BCP] JSON responses parsed directly from the HTTP stream instead of `getString()` (no ~6 KB body String on every refresh); requests use HTTP/1.0 so the body is never chunked
- [x] [General] [Refactor] String replaced where it pays off: countdown text built with snprintf into a char buffer (no heap allocations in the per-second redraw), BCPEvent/Config getters return `const String&`, "Round N" and splash text without String. String cannot be removed completely (ESP32 core WebServer/HTTPClient/Preferences APIs use it)
- [x] [BCP] Event overview fetched from BCP API v2 (`/v2/events/<id>`) with a JSON filter keeping only name and status; the timer stays on v1 (no v2 exists)
- [x] [General] [Tests] Git pre-commit hook (tools/git-hooks): native tests on every commit, TC001 tests only when related files are staged, then production firmware is flashed back
- [x] [GFX] [Bugfix] Scrolling text width skips invalid UTF-8 bytes like the canvas does (a lone byte 0xA0–0xFF was counted as a Latin-1 character)
- [x] [Hw] [Bugfix] Round indicator centred also in the last round (16–30 rounds): only the gaps actually drawn around the current round are reserved
- [x] [GFX] [Bugfix] Text appended to a scrolling text (e.g. a message after an error) keeps the UTF-8 segmented font, so non-ASCII characters are drawn
- [x] [General] [Tests] Unit tests: `pio test -e native` on the Mac (event ID, BCP JSON and time parsing, thresholds, hostname, WiFi list, countdown text/colour, round indicator, display/refresh decisions, message prefixes, RTC snapshot, web UI), `pio test -e nodemcu-32s` on the TC001 (fonts, UTF-8 canvas, scrolling text, progress indicator, newlib time, config in NVS), `tools/bcp_contract.py` for the live BCP API; testable logic moved to lib/core without behaviour change
- [x] [General] [Docs] Documented that loop() and the display task both run on Core 1 (time-sliced); Core 0 is left to the WiFi stack
- [x] [WiFiMgr] [Minor] Time zone set to plain UTC (no daylight offset), so logged times match the console and BCP times
- [x] [General] [Refactor] Code review cleanup: unused urlEncode and _serverTimedOut removed, lastDataRefresh typed as millis(), named RTC plausibility constant, GFXanimation virtual destructor and isActive default
- [x] [Hw] [Bugfix] Progress indicator moves between columns 0 and 31 (was -1 and 30)
- [x] [Config] [Feature] Hostname setting applied: DHCP hostname and mDNS (config page at `http://<hostname>.local`), sanitized to letters, digits and hyphens
- [x] [General] [Feature] Dev build: `portal` console command (reboot into captive portal), portal HTTP request logging; README notes iPhone Auto-Login for the captive portal
- [x] [Config] [Refactor] Captive portal and config page share one page template and CSS (lib/webui); main buttons labelled Save / Connect; styled "Connected" page
- [x] [Hw] [Bugfix] Countdown redrawn once per second on the second change instead of ~47 times per second (stopAnimations() reset the redraw timer)
- [x] [Hw] [Refactor] Display state only says what is shown and is set only by the Hw implementation (CONFIG_REQUIRED / CONFIG_MESSAGE instead of DONT_UPDATE / ENFORCE_REFRESH, redrawRequested flag, Hw::displayCountdown()); Hw methods use the event parameter; unused serial.h removed
- [x] [BCP] [Bugfix] Timer data (incl. pause state) cleared on round change, for a round without timer (`{}`) and on 404; connection errors keep the last timer data
- [x] [Hw] [Bugfix] No crash (division by zero) when the event has a running timer but zero rounds; the round indicator is skipped
- [x] [Hw] [Bugfix] Invalid event ID message shown in red before the looping config page address; no extra static text print in the message display
- [x] [GFX] [Bugfix] Looping scrolling text with appended part (newStart) no longer flashes the first part before restarting; bitmap drawn with correct row stride
- [x] [Config] [Bugfix] Config page: no double URL decoding of form values, HTML-escaped values in the form, deleting stored WiFi networks works again (route mismatch) and uses the DELETE method in both config page and captive portal
- [x] [Hw] [Bugfix] Display mutex: loop() and eventHandler no longer draw to the matrix or delete animations concurrently; UTF-8 decoder state per object instead of static
- [x] [Main] [Bugfix] Recover from WiFi/BCP outages: reboot only on connection errors with WiFi up, keep event data across reboot (RTC memory), config page only for invalid event ID, captive portal times out after 5 min and retries stored networks
- [x] [General] [Feature] Dev build (`pio run -e dev`) with debug output and serial console (status, screen dump, buttons, timeshift, fake event data)
- [x] [Hw] [Feature] Pressing right button refreshes data and updates display immediately
- [x] [Config] [Feature] Event field gets ⨂ at the right end to erase the content
- [x] [WiFiMgr] [Minor] Move button to delete all stored networks to the same line as section header, far right; this is for the user to not mistake the button with the main submit button of the form
- [x] [Config] [Minor] Align the Config web page appearance with WiFi Manager web config, displaying nice on tablets and computers (so far optimized for cell phones only)
- [x] [WiFiMgr] [Minor] change emoji at Refresh button to ↻
- [x] [Config] [Bugfix] Handling of invalid event in config form (both entering invalid event as well invalid event stored and entering valid event)
- [x] [GFX] [Bugfix] Short delay in stopAnimations to get all display routines finish
- [x] [Hw] [Refactor] Create Hw abstract interface and Tc001 class to handle hardware-dependent stuff
- [x] [Main] [Bugfix] Update event name or round number if previously displayed the same but different value
- [x] [GFX] [Refactor] Drop own implementation of framebuffer, instead use [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library), including fonts
- [x] [WiFiMgr] [Refactor] Use Print object for both user and debug messages
- [x] [Main] [Feature] Support for paused timer
- [x] [Main] [Feature] Support for time to round start
- [x] [GFX] [Feature] Support for [UTF8](https://www.charset.org/utf-8) in texts on display
- [x] [Main] [Bugfix] Remove resetting displayState after every data fetch
- [x] [GFX] [Feature] Support for proportional fonts
- [x] [Main] [Bugfix] Check for event being valid and bring Config if not
- [x] [Main] [Feature] Reboot device after 5 consecutive data fetch failures
- [x] [WiFiMgr] [Feature] Add RTC support for time synchronization
- [x] [WiFiMgr] [Bugfix] If connecting to multiple networks and failing on NTP sync, display the error only once after the last network processed
- [x] [WiFiMgr] [Feature] Enforce WiFi Manager if mid button pressed on starting up the device
- [x] [WiFiMgr] [Feature] Button to delete all stored networks in WiFi config
- [x] [Config] [Feature] Read/write config to persistent storage instead of filesystem
- [x] [GFX] [Refactor] One hardware timer for animations
- [x] [GFX] [Refactor] Run GFX updates in a separate FreeRTOS task, so they are not blocked by data fetching in loop() (both run on Core 1, time-sliced)
- [x] [General] [Refactor] Rewrite from scratch in [Arduino C++](https://docs.arduino.cc/arduino-cloud/guides/arduino-c/) / [PlatformIO](https://platformio.org/) to mitigate limitations of [microPython](https://micropython.org/)
- [x] [Main] [Feature] Mid button to start Config http server
- [x] [WiFiMgr] [Refactor] Config webserver as separate module loaded on demand to save resources
- [x] [Main] [Feature] 3x5 font added, use to display time if displaying h:mm:ss
- [x] [WiFiMgr] [Feature] Captive portal for Wifi Manager
- [x] [WiFiMgr] [Feature] WiFi manager instead of fixed connection
- [x] [GFX] [Bugfix] Utilize asyncio to not block display countdown when fetching new data