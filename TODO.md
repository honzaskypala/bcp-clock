# TODO for BCP-clock project

## main related
- [ ] offline mode, just run countdown from configurable time
- [ ] chess clock functionality
- [ ] Low battery warning

## TC001 hardware related
- [ ] make pressing right button refresh data and update display immediately
- [ ] bliking every 0.5 sec instead of 1 sec
- [ ] blink red (over time) always
- [ ] blink green (time to start) last 2 minutes
- [ ] display brightness configurable
- [ ] beep at passing thresholds
- [ ] support for rounds >= 10 hours (double digit hours)
- [ ] show finished tables + config top / bottom / none

## GFX related
- [ ] check progress indicator, it looks like it moves between -1 and 30 instead of 0 and 31
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

## Done
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
- [x] [GFX] [Refactor] Run GFX updates in separate thread on Core 1, so it is not blocked by code on Core 0
- [x] [General] [Refactor] Rewrite from scratch in [Arduino C++](https://docs.arduino.cc/arduino-cloud/guides/arduino-c/) / [PlatformIO](https://platformio.org/) to mitigate limitations of [microPython](https://micropython.org/)
- [x] [Main] [Feature] Mid button to start Config http server
- [x] [WiFiMgr] [Refactor] Config webserver as separate module loaded on demand to save resources
- [x] [Main] [Feature] 3x5 font added, use to display time if displaying h:mm:ss
- [x] [WiFiMgr] [Feature] Captive portal for Wifi Manager
- [x] [WiFiMgr] [Feature] WiFi manager instead of fixed connection
- [x] [GFX] [Bugfix] Utilize asyncio to not block display countdown when fetching new data