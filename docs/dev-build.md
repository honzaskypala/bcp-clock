# Dev build, console, tests

## Dev build (`-e tc001-dev`, `BCP_DEV` define)
- Every build configuration in `configs/` has a dev env next to its release env (`tc001` / `tc001-dev`), with the
  same features plus `-D BCP_DEV`.
- All development code is inside `#ifdef BCP_DEV`; the production build must stay byte-identical without it.
- Debug output (`DEV_LOG("Module", msg)` from `lib/devlog/devlog.h`, printed to `Serial`) exists only in the dev
  build; in production the macro emits no code or strings.
- Serial command console (`src/devconsole.cpp`, called from `loop()`), 9600 baud, one command per line,
  each response ends with `[Console] done`. `help` lists commands: `status` (incl. the mode), `screen` (ASCII dump of the
  matrix: `.` off, `:` dim, letters = colour), `left`, `right`, `mid` (short press), `midlong` (long press, config page),
  `refresh` (BCP data now), `timeshift <sec>`, `beep yellow|red` (play a threshold signal),
  `fake <mode> [sec] [length] [round/rounds]` (fake event data: `name`, `round`, `start`, `run`, `pause`,
  `over`, `off`; BCP is not contacted while active), `bcpfail on|off` (simulated BCP connection errors), `wififail on|off` (simulated WiFi outage: the refresh
  fails and main sees WiFi as disconnected, no reconnect),
  `battery [raw|off]` (raw / median / level; `raw` simulates the ADC median, e.g. `2300` = low, `2280` =
  critical, `off` returns to the real reading), `battlog [start [sec]|stop]` (battery calibration log: the median every `sec` seconds, default 60,
  up to 1024 samples in NVS namespace `battlog`; survives power loss and restarts, `start` erases the previous
  log, no argument prints CSV `seconds,raw`; seconds count samples, so the gap of a restart is not included), `portal` (reboot into the captive portal), `reboot`. `screen` includes the status overlay as
  it was last shown (`A` = amber). `status` shows the connection alert and the reboot count.
- Helpers in `tools/`: run `python tools/devmon.py` (PlatformIO's python, it needs pyserial) in the background (keeps the port
  open so the device is not reset, logs to `.pio/dev.log`, reads commands from `.pio/dev.fifo`), then
  `tools/devcmd.sh <command>` sends a command and prints the response. Stop devmon before flashing.
- The config web server (started by `midlong`) is reachable from the host over WiFi at the device IP
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
- `tools/build_configs.sh` builds every env of every `configs/*.ini` (logs in `.pio/configs/`), so a feature
  combination that does not compile is found; device tests run with the release config (`pio test -e tc001`).
- Git pre-commit hook `tools/git-hooks/pre-commit` (enable per clone: `git config core.hooksPath tools/git-hooks`):
  native tests always; `tools/build_configs.sh` when `src/`, `lib/`, `configs/` or `platformio.ini` is staged;
  TC001 suites only for related staged files (`lib/gfx/` → `test_gfx`; `lib/config/`,
  `lib/core/bcpparse.*`, `lib/core/textutil.*` → `test_platform`; `platformio.ini`, `configs/tc001.ini` → both), with upload
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

## Release
- Push a tag `vX.Y.Z` (e.g. `git tag v1.0.1 && git push origin v1.0.1`). `.github/workflows/release.yml` runs the host
  tests, builds every build configuration (`tools/build_configs.sh`), builds the release configuration `tc001` with
  `tools/build_release.sh X.Y.Z`, creates a GitHub Release with the merged image
  `bcp-clock-X.Y.Z.bin` and deploys the browser installer to GitHub Pages (https://honzaskypala.github.io/bcp-clock/).
  Pages must have Source: GitHub Actions.
- The version comes from the tag as the `BCP_VERSION` define (`PLATFORMIO_BUILD_FLAGS`) and is shown in the footer of the
  configuration page; local builds show `dev`.
- The installer (`install/index.html`, ESP Web Tools) flashes the bootloader, partition table, `boot_app0` and the
  application as separate parts, so NVS (0x9000) is not touched and an update without "Erase device" keeps the settings.
  The merged image covers NVS with `0xFF` and erases the settings.
- GitHub Pages sends `max-age=600`; the install page loads `manifest.json` with a cache-busting query, so a new
  release is offered at once. After a release, check in a browser that the installer shows the new version.
- Local test: `sh tools/build_release.sh X.Y.Z`, then serve `.pio/release/site` on `localhost` (Web Serial needs a
  secure context) and open it in Chrome.
