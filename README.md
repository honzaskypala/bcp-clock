# BCP Clock

**A tournament round timer for the Ulanzi TC001 pixel clock, fed live from [Best Coast Pairings](https://www.bestcoastpairings.com).**

[![PlatformIO](https://img.shields.io/badge/built%20with-PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/platform-ESP32-blue?logo=espressif)](https://www.espressif.com/en/products/socs/esp32)
[![Framework: Arduino](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino)](https://docs.arduino.cc/)
[![License: WTFPL](https://img.shields.io/badge/license-WTFPL-brightgreen)](http://www.wtfpl.net/)

> [!WARNING]
> This project is a work in progress. There is no released version yet and it is not intended for
> production use.

<!-- TODO PICTURE: Hero photo of a TC001 on a tournament table showing a running countdown
     (white mm:ss digits, round indicator on the bottom row). Suggested file: docs/img/hero.jpg -->

BCP Clock is custom firmware for the Ulanzi TC001 desktop pixel clock. Enter a Best Coast Pairings event once, and the clock shows the official round
timer of that tournament: time to round start, the running countdown, a paused timer and overtime,
with colours that change as the end of the round approaches. Put one on each table or next to the
judges' desk and nobody has to check their phone.

## Contents

- [Features](#features)
- [What the display shows](#what-the-display-shows)
- [Hardware](#hardware)
- [Installation](#installation)
- [First setup](#first-setup)
- [Configuration](#configuration)
- [How it works](#how-it-works)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [History](#history)
- [Disclaimer](#disclaimer)
- [License](#license)
- [Credits](#credits)

## Features

- **Live BCP round timer**: follows the timer the tournament organiser runs in Best Coast Pairings.
- **Time to round start**, **running countdown**, **paused timer** and **overtime**, each with its own colour.
- **Configurable yellow and red thresholds** for the last minutes of a round.
- **Round indicator** on the bottom row: which round of how many is being played.
- **Event name and round number** when no timer is running.
- **UTF-8 text** on the display, including Latin Extended-A and Cyrillic, so event names with
  diacritics are shown correctly.
- **WiFi manager with captive portal**: stores several networks and connects to whichever is available.
- **Accurate time** from NTP, with the built-in RTC as a fallback.
- **Web configuration page** served by the clock itself, usable from a phone.
- **Self-recovery**: the clock reboots itself after repeated connection failures.

## What the display shows

| Situation | Display |
|---|---|
| Event not started yet, or already finished | Scrolling event name |
| Event running, no timer for the current round | `Round N` |
| Timer set, round not started yet | **Green** countdown to the round start |
| Round running | **White** countdown |
| Remaining time below the yellow threshold | **Yellow** countdown |
| Remaining time below the red threshold, overtime | **Red** countdown, negative values in overtime |
| Timer paused by the organiser | **Blinking magenta** remaining time |
| Configuration page open | Scrolling `Config page http://<IP>` |

Rounds up to one hour are shown as `mm:ss` in a large font, longer rounds as `h:mm:ss` in a smaller
font. If the value no longer fits (more than an hour of overtime in `mm:ss` mode), the digits are
replaced by `X`, e.g. `-XX:XX`.

While a timer is shown, the bottom row of LEDs is a round indicator: one segment per round, with the
current round highlighted in blue.

<!-- TODO PICTURE: Gallery of display states side by side (green pre-round, white, yellow, red overtime,
     magenta paused). Close-up photos of the matrix work best. Suggested files: docs/img/state-*.jpg -->

<!-- TODO PICTURE: Short GIF of the scrolling event name. Suggested file: docs/img/event-name.gif -->

## Hardware

The only hardware needed is an **Ulanzi TC001** (ESP32-WROOM, 32×8 WS2812 LED matrix, three buttons,
RTC, battery). No soldering or opening of the case is required; the firmware is flashed over USB-C.
The clock is easy to buy, e.g. on [Amazon](https://www.amazon.com/s?k=Ulanzi+TC001) or
[AliExpress](https://www.aliexpress.com/w/wholesale-Ulanzi-TC001.html).
Detailed hardware documentation is available at
[rroels/ulanzi_tc001_hardware](https://github.com/rroels/ulanzi_tc001_hardware).

> [!NOTE]
> The built-in battery lasts roughly 2–4 hours. For all-day tournaments, connect the clock to a
> power bank or a USB power adapter.

### Buttons

| Button | Held while powering on | Pressed while running |
|---|---|---|
| Left | – | not used |
| Middle | Opens the WiFi setup portal | Opens the configuration page (for 5 minutes) |
| Right | – | Refreshes the data from BCP immediately |

<!-- TODO PICTURE: Photo of the top of the TC001 with the left / middle / right buttons labelled.
     Suggested file: docs/img/buttons.jpg -->

## Installation

There are no prebuilt binaries yet; for now the firmware is built from source with PlatformIO.
Prebuilt binaries and a simple way to flash them will be provided with the first release.

<!-- TODO RELEASE: Add a "Flash a prebuilt binary" section above "Build from source": download link
     to the release binary and step-by-step flashing for non-developers (e.g. ESP Web Tools,
     flashing from the browser via Web Serial). Keep "Build from source" for developers. -->

### Build from source

#### Prerequisites

- [PlatformIO](https://platformio.org/install), either as the VS Code extension or the CLI
- A USB-C **data** cable (some cables only charge)

> [!CAUTION]
> Flashing replaces the original Ulanzi firmware. If you may want to go back to it, back it up first
> with [esptool](https://docs.espressif.com/projects/esptool/):
>
> ```sh
> esptool.py --baud 921600 read_flash 0x0 0x400000 tc001-stock.bin
> ```
>
> To restore it later: `esptool.py --baud 921600 write_flash 0x0 tc001-stock.bin`

#### Build and flash

```sh
git clone https://github.com/honzaskypala/bcp-clock.git
cd bcp-clock
pio run                       # build
pio run -t upload             # flash over USB
pio device monitor -b 9600    # optional: serial console
```

> [!TIP]
> Plug the TC001 directly into your computer. Uploading through a USB hub often fails.

## First setup

1. **Power on** the clock. A splash screen appears and the clock tries to connect to a known WiFi network.
2. On first start there is no known network, so the clock opens an **open WiFi network named
   `BCP-clock`** and scrolls the address of the setup page.
3. **Connect your phone** to `BCP-clock`. The setup page usually opens by itself (captive portal);
   if not, open the address shown on the display.
4. **Choose your WiFi network** and enter its password. The clock connects and synchronises its time.
5. With no event configured yet, the clock scrolls **`Config page http://<IP>`**. Open that address
   from a device in the same network.
6. **Paste the event** (the URL of the event page in BCP, or just its ID) and press Save.
7. The clock shows the event name, the round, or the countdown, depending on the state of the event.

<!-- TODO PICTURE: Phone screenshot of the WiFi setup (captive portal) page.
     Suggested file: docs/img/wifi-portal.png -->

<!-- TODO PICTURE: Screenshot of the configuration page (event, yellow / red thresholds, stored networks).
     Suggested file: docs/img/config-page.png -->

## Configuration

The configuration page is available at `http://<IP of the clock>`, or at `http://bcp-clock.local`
(the hostname setting followed by `.local`):

- automatically, with no time limit, while the configured event is missing or invalid;
- after pressing the **middle button**, for 5 minutes.

| Setting | Description | Default |
|---|---|---|
| Event | BCP event URL (e.g. `https://www.bestcoastpairings.com/event/AbCdEf123456`) or just the event ID | empty |
| Yellow | Remaining time below which the countdown turns yellow, as `m:ss` or `h:mm:ss` | `10:00` |
| Red | Remaining time below which the countdown turns red, as `m:ss` or `h:mm:ss` | `0:00` |
| Hostname | Device name in the network: shown in the router's client list and used for the `.local` address of the configuration page. Letters, digits and hyphens, up to 32 characters. A change of the router name applies at the next WiFi connection. | `BCP-clock` |

The same page lists the **stored WiFi networks**, where you can delete individual networks or all of
them. To add a new network, hold the **middle button** while powering on the clock to open the WiFi
setup portal.

All settings are stored in the ESP32's non-volatile memory and survive reboots and firmware updates.

## How it works

- Every 60 seconds (or when the right button is pressed), the clock fetches the event overview and
  the timer of the current round from the Best Coast Pairings API.
- The countdown itself is computed locally from the round end time and the system clock, so it runs
  smoothly between fetches. That is why accurate time matters: it comes from NTP (`pool.ntp.org`)
  after each WiFi connection and is stored in the RTC, which is used if NTP is not reachable.
- The display runs in its own FreeRTOS task, so network requests do not freeze the countdown.

### Project structure

| Path | Contents |
|---|---|
| `src/main.cpp` | Main loop: data refresh, display state decisions |
| `lib/bcp/` | Best Coast Pairings API client |
| `lib/hw/` | Hardware abstraction and the TC001 implementation (matrix, buttons, display task) |
| `lib/config/` | Settings storage and the configuration web server |
| `lib/wifimgr/` | WiFi manager with captive portal, NTP and RTC time synchronisation |
| `lib/gfx/` | Graphics helpers on top of Adafruit GFX: scrolling text, progress indicator, UTF-8 fonts |
| `lib/core/` | Hardware independent logic: parsing of the BCP data, countdown text and colours, round indicator, display decisions |
| `test/` | Unit tests: `native/` run on the computer, `device/` run on the TC001 |

## Testing

```sh
pio test -e native                          # unit tests on the computer, no device needed
pio test -e nodemcu-32s                     # unit tests on the TC001 connected over USB
python tools/bcp_contract.py <eventId>      # checks the live Best Coast Pairings API
```

The tests on the computer cover the hardware independent logic in `lib/core` and the web page helpers: parsing of
the event ID, of the BCP responses and of the times, colour thresholds, the countdown text and colours, the round
indicator, what the display shows and when the clock reboots. They take a few seconds.

The tests on the TC001 cover what needs the real hardware: fonts, UTF-8 text rendering, scrolling text, the
progress indicator, time conversion and settings storage (in a separate namespace, your settings are not touched).
They replace the firmware on the clock, so flash it again afterwards (`pio run -t upload`).

The contract test calls the unofficial BCP API with a real event and checks that the responses still have the
fields and formats the clock expects. Run it by hand, e.g. when the clock stops showing data.

A git pre-commit hook runs the tests before every commit and stops the commit on a failure. The tests on the
computer run always; the tests on the TC001 only when the commit changes code they cover (graphics, settings,
time parsing), and then the production firmware is flashed back. If they are needed and the clock is not
connected, the commit is stopped; `SKIP_DEVICE_TESTS=1 git commit` skips them. Turn the hook on once per clone:
`git config core.hooksPath tools/git-hooks`.

## Troubleshooting

| Problem | What to do |
|---|---|
| The WiFi setup portal opens on every start | No stored network is in range, or the clock connected but could not synchronise time (`Error: NTP time sync failed`). Check that the network has internet access. |
| The setup page does not open by itself on iPhone | In Settings → Wi-Fi → ⓘ next to `BCP-clock`, turn on **Auto-Login**, then forget the network and connect again. Or open the address shown on the display in the browser. |
| `Invalid event ID, please configure.` | The event was not found in BCP. Open the configuration page and check the event URL or ID. |
| The clock reboots by itself | It could not fetch data 5 times in a row (about 5 minutes) and restarts to recover. Check the WiFi network and internet access. |
| Upload or serial monitor fails | Connect the clock directly, without a USB hub, and use a data cable. |
| Need more details | Enable debug output by uncommenting `debugOut_ = &Serial;` in `setup()` in `src/main.cpp`, rebuild and watch the serial console at 9600 baud. |

## Roadmap

Planned features and known tasks are tracked in [TODO.md](TODO.md). Highlights:

- offline mode, counting down from a manually set time
- chess clock mode
- configurable display brightness
- beeps when passing the thresholds

## History

The project started in MicroPython. During development it hit the limits of that environment
(mainly available memory), so it was rewritten from scratch in C++ with the Arduino framework.
The original MicroPython code is still in the repository history up to
[commit f7299192](https://github.com/honzaskypala/bcp-clock/tree/f7299192a644fcca563c471fa8951567b8548e41).

## Disclaimer

This project is not affiliated with, endorsed by, or supported by Best Coast Pairings. It uses an
unofficial, undocumented API that may change or stop working at any time. HTTPS connections to the
API are made without certificate validation.

## License

Released under the [WTFPL](http://www.wtfpl.net/) – Do What The F*ck You Want To Public License.

## Credits

- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [FastLED](https://github.com/FastLED/FastLED) and [FastLED NeoMatrix](https://github.com/marcmerlin/FastLED_NeoMatrix)
- [ArduinoJson](https://arduinojson.org/)
- [ArduinoFake](https://github.com/FabioBatSilva/ArduinoFake) for the unit tests on the computer
- [RTClib](https://github.com/adafruit/RTClib)
- [rroels/ulanzi_tc001_hardware](https://github.com/rroels/ulanzi_tc001_hardware) for the TC001 hardware documentation
