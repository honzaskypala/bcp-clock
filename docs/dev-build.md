# Dev build, console, tests

## Dev build (`-e dev`, `BCP_DEV` define)
- All development code is inside `#ifdef BCP_DEV`; the production build must stay byte-identical without it.
- Debug output (`DEV_LOG("Module", msg)` from `lib/devlog/devlog.h`, printed to `Serial`) exists only in the dev
  build; in production the macro emits no code or strings.
- Serial command console (`src/devconsole.cpp`, called from `loop()`), 9600 baud, one command per line,
  each response ends with `[Console] done`. `help` lists commands: `status`, `screen` (ASCII dump of the
  matrix: `.` off, `:` dim, letters = colour), `refresh`, `mid`, `timeshift <sec>`, `beep yellow|red` (play a threshold signal),
  `fake <mode> [sec] [length] [round/rounds]` (fake event data: `name`, `round`, `start`, `run`, `pause`,
  `over`, `off`; BCP is not contacted while active), `bcpfail on|off` (simulated BCP connection errors),
  `portal` (reboot into the captive portal), `reboot`.
- Helpers in `tools/`: run `python tools/devmon.py` (PlatformIO's python, it needs pyserial) in the background (keeps the port
  open so the device is not reset, logs to `.pio/dev.log`, reads commands from `.pio/dev.fifo`), then
  `tools/devcmd.sh <command>` sends a command and prints the response. Stop devmon before flashing.
- The config web server (started by `mid`) is reachable from the host over WiFi at the device IP
  (see `status`), so config changes can be made via HTTP.

## Tests
- Tests (PlatformIO Unity): `test/native/` runs on the host (env `native`, Arduino `String` from
  ArduinoFake, only `lib/core` and `lib/webui` are compiled), `test/device/` runs on the TC001. Device
  tests replace the firmware: stop devmon before, flash production (or dev) again right after every run
  (the test firmware leaves the display frozen). Every device test `setup()` must start by silencing the
  buzzer (see "Every TC001 firmware: silence the buzzer first" in CLAUDE.md). Running both
  device suites in one `pio test` call sometimes fails to upload the second one; run it again with
  `-f device/<suite>`. Unity on the ESP32 has no 64-bit asserts. Logic that needs tests belongs to
  `lib/core` (no ESP-only includes). Display, WiFi, portal, config server and buttons are verified on the
  device with the dev console.
- Git pre-commit hook `tools/git-hooks/pre-commit` (enable per clone: `git config core.hooksPath tools/git-hooks`):
  native tests always; TC001 suites only for related staged files (`lib/gfx/` → `test_gfx`; `lib/config/`,
  `lib/core/bcpparse.*`, `lib/core/textutil.*` → `test_platform`; `platformio.ini` → both), with upload
  retries, then production is flashed back. Blocks the commit if the TC001 is needed but missing or devmon
  holds the port; `SKIP_DEVICE_TESTS=1 git commit` skips the device part. A commit with device tests takes
  several minutes: run it in the background (Bash timeout is 10 min). Logs in `.pio/precommit/`.

## Hardware troubleshooting
- Flashing, the serial console and device tests need a TC001 connected over USB. If upload/monitor fails,
  check the USB connection first (USB hubs are a common cause).
- In 2025 the device occasionally froze after flashing. Recovery: hold Left + Right to power off, power on
  again, flash again.
- Opening or closing the serial port resets the ESP32 (auto-reset circuit), even with DTR/RTS low.
- Board profile is `nodemcu-32s` (the TC001 is ESP32-WROOM based). `upload_speed = 115200`: the CH340
  USB-serial bridge is unreliable at higher rates.
- Flash usage is ~85 % of the 1.25 MB app partition (dev build ~90 %). Watch the size when adding
  features/fonts/HTML.
