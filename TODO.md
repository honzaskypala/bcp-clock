# TODO for BCP-clock project

## main related
- [ ] offline mode, just run countdown from configurable time
- [ ] chess clock functionality
- [ ] [Low battery warning](https://github.com/rroels/ulanzi_tc001_hardware?tab=readme-ov-file#battery-voltage-example-2-mapped-value)

## TC001 hardware related
- [ ] bliking every 0.5 sec instead of 1 sec
- [ ] blink red (over time) always
- [ ] blink green (time to start) last 2 minutes
- [ ] display brightness configurable
- [ ] [beep](https://github.com/rroels/ulanzi_tc001_hardware?tab=readme-ov-file#example-code-buzzer) at passing thresholds
- [ ] support for rounds >= 10 hours (double digit hours)
- [ ] show finished tables + config top / bottom / none

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
- [ ] Refactor all code, method names to verbNouns()
- [ ] Refactor code, unnest
- [ ] Refactor change String to char * where it mekes sense
- [ ] Release: prebuilt binary + simple flashing for users (e.g. [ESP Web Tools](https://esphome.github.io/esp-web-tools/) on GitHub Pages), document it in README Installation

## Done
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