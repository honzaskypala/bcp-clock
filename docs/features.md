# Build features and configurations

A build of BCP-clock contains a chosen set of features. The choice is a **build configuration**: one file in
`configs/` that defines PlatformIO envs and sets every feature switch as a preprocessor define.

## Mechanism
- `platformio.ini` loads all `configs/*.ini` (`extra_configs`) and holds the shared hardware settings
  (`[tc001_base]`: platform, board, libraries). It has no build env of its own except `native` (host tests).
- A configuration file sets `-D BCP_FEATURE_<NAME>=0|1` for **every** feature and defines a release env and a
  dev env (`-D BCP_DEV`) with the same features, e.g. `tc001` / `tc001-dev`.
- `lib/features/buildfeatures.h` fails the build (`#error`) when a feature is not defined (a missing or misspelled
  switch) or a dependency is not met. Code includes it and tests features with `#if BCP_FEATURE_X`, never `#ifdef`.
- `#if` is used only where a feature is wired in (`src/`, `lib/hw`, `lib/config`). Unused code in `lib/core` is not
  guarded: the linker drops what is not referenced (`--gc-sections`), and the host tests test all of it.
- A disabled feature disappears completely: its function, its status indicators and icons, its part of the
  configuration page and its dev console commands.
- The same defines work outside PlatformIO (e.g. CMake `add_compile_definitions` for a future Linux build).
- `tools/build_configs.sh` builds every env of every configuration (pre-commit hook, release workflow).
- A GitHub release publishes one configuration per hardware platform (TC001: `tc001`).

## Configurations

| File | Envs | Contents |
|---|---|---|
| `configs/tc001.ini` | `tc001`, `tc001-dev` | TC001 release: all features |
| `configs/tc001-minimal.ini` | `tc001-minimal`, `tc001-minimal-dev` | TC001, BCP mode without the optional features (build check) |

## Features

| Define | Contents | Requires |
|---|---|---|
| `BCP_FEATURE_WIFIMGR` | WiFi manager: stored networks, captive portal (Mid held at power-on), NTP + RTC time sync, stored networks on the config page, hostname setting (DHCP) | – |
| `BCP_FEATURE_MDNS` | Config page at `http://<hostname>.local` | `WIFIMGR` |
| `BCP_FEATURE_BATTERY` | Battery voltage monitoring: low battery status pixel and icon, dev console `battery`, `battlog` | – |
| `BCP_FEATURE_CONNECTION_MONITOR` | WiFi / BCP status pixels and icons, reboot after 5 consecutive failed refreshes, dev console connection status | `BCP_MODE_BCP` |
| `BCP_MODE_BCP` | BCP mode (`src/bcpmode.*`, `lib/bcp`): event field on the config page, event name / round / countdown with the round indicator, dev console `refresh`, `fake`, `bcpfail`, `wififail` and the event status | `WIFIMGR` |

A build needs at least one mode (`BCP_MODE_*`), the build fails otherwise. Modes are switched by a short press of
the middle button in the order of `modes[]` in `src/main.cpp`; a new mode also needs its `ModeId`
(`lib/core/modeid.h`).

## Adding a feature
1. Add the define to every `configs/*.ini` and to the check in `buildfeatures.h` (with its dependencies).
2. Guard the wiring code with `#if BCP_FEATURE_X`, including config page fields and dev console commands.
3. Add it to the table above; run `sh tools/build_configs.sh`.

## Adding a configuration
Copy a `configs/*.ini` file, rename its sections (`[<name>_features]`, `[env:<name>]`, `[env:<name>-dev]`) and set
the switches. Section names must be unique across all configuration files.
