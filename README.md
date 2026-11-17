<div align="center">

  <h1>WiFi32 — ideaspark edition</h1>

  <p>
    A WiFi security/testing tool for the ESP32 board with an <b>integrated</b> 1.9" ST7789 TFT,
    controlled with a single BOOT button, with a live web dashboard over your home WiFi.
  </p>

<a href="https://github.com/syphr0/wifi32" title="Upstream repo"><img src="https://img.shields.io/static/v1?label=Fork%20of&message=syphr0%2Fwifi32&color=lightblue&logo=github&style=for-the-badge" alt="fork of syphr0/wifi32"></a>
<a href="https://github.com/cifertech/ESP32-DIV" title="Original repo"><img src="https://img.shields.io/static/v1?label=Originally&message=ESP32-DIV&color=lightgrey&logo=github&style=for-the-badge" alt="originally ESP32-DIV"></a>

</div>

<br>

# About

This is a fork of [syphr0/wifi32](https://github.com/syphr0/wifi32) (itself a fork of
[cifertech/ESP32-DIV](https://github.com/cifertech/ESP32-DIV)), re-targeted for a different,
much more common piece of hardware: an **ideaspark-style ESP32-D0WD-V3 board with a built-in
1.9" ST7789 TFT (170×320)** — the kind sold cheaply with the screen already soldered on, no
breadboard or wiring required. The upstream project targets a bare ESP32 DevKitC wired by hand
to a separate 1.8" ST7735 display and 4 momentary buttons; this board has neither of those, so
most of the hardware-facing code has been replaced.

### What's different from upstream

- **Display driver**: swapped `Adafruit_ST7735` (software SPI) for
  [LovyanGFX](https://github.com/lovyan03/LovyanGFX) talking to the onboard ST7789 at this
  board's actual pins (`CS=15, RST=4, DC=2, SCK=18, MOSI=23`, backlight on `GPIO32`) — see
  [`display_config.h`](display_config.h).
- **Input**: the 4 physical buttons (Up/Down/Select/Back on GPIO21/22/25/26) don't exist on this
  board. [`input.h`](input.h)/[`input.cpp`](input.cpp) decode the single onboard BOOT button
  (GPIO0) into three gestures that stand in for them:
  - **tap** → Next (cycle menu item / channel / list entry)
  - **double-tap** → Select (enter a menu item / toggle a feature on-off)
  - **hold** → Back (exit the current feature to the main menu)
- **Web dashboard** ([`webdash.h`](webdash.h)/[`webdash.cpp`](webdash.cpp)): the device joins
  your home WiFi and serves a live status page (Server-Sent Events, auto-reconnecting) with
  Next/Select/Back buttons that fire the same gestures as the physical button. Since this board
  has one WiFi radio, the dashboard drops offline for as long as a feature is using the radio
  exclusively (channel-hopping, promiscuous capture, packet injection) — it reconnects on its
  own once you're back at the main menu. While idle, it periodically does a disconnect → scan →
  reconnect cycle to report nearby networks.
- **New feature: Packet Stormer** ([`packetstormer.h`](packetstormer.h)/[`packetstormer.cpp`](packetstormer.cpp)) —
  a connection-resilience stress tester with four modes (cycled via a `[change mode]` list
  entry): a raw-802.11 **SSID Flood** (fake Authentication frames at a chosen AP's BSSID/channel,
  no WiFi join needed) and **TCP / UDP / HTTP Flood** against a LAN IP, discovered with a
  best-effort TCP port-80 sweep of your subnet.
- Fixed two upstream bugs in passing: the Deauth Detector screen and an active Beacon Spam
  session each had no way to exit at all in the original code.
- Ported the whole thing to the current (non-EOL) `esp32:esp32` Arduino core 3.x and
  `arduinoFFT` 1.6.x — the upstream project needed the long-retired `1.0.6` core.

### What's unchanged

All four of the original features (WiFi Analyzer, Packet Monitor, Beacon Spam, Deauth Detector)
work the same as upstream, just driven by the new input/display layers.

<br>

> [!WARNING]
> **Beacon Spam, Deauth Detector, and Packet Stormer are disruptive by design.** They send raw
> management frames, flood connections, or jam channels. Nothing in the firmware stops you from
> aiming any of these at a network you don't own — that's on you. Only use them against your own
> hardware/network, or with the explicit permission of whoever owns what you're pointing it at.

<br>

# Hardware

- An ideaspark-style ESP32 board with an **integrated** 1.9" ST7789 TFT (170×320), CH340 or
  CP210x USB-UART. No external display, buttons, or wiring needed — everything here assumes
  the TFT and BOOT button that come soldered on the board.
- If your board's TFT pins differ, confirm them against the module's own silkscreen/FPC and
  edit the `TFT_PIN_*` constants in [`display_config.h`](display_config.h).

<br>

# Setup

### 1. Arduino toolchain

- [Arduino IDE](https://www.arduino.cc/en/software) or [`arduino-cli`](https://arduino.github.io/arduino-cli/)
- Board package: `esp32:esp32` (Espressif's `arduino-esp32` core), version **3.x** — add
  `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
  as a Boards Manager URL first if you don't already have it.
- CH340/CP210x USB-UART driver for your OS, if your board needs one.

### 2. Libraries

Install via the Library Manager (or `arduino-cli lib install "<name>"`):

- `LovyanGFX`
- `arduinoFFT` — version **1.6.x** (the packet monitor's spectrogram code uses the pre-2.0 API;
  v2 renamed the class and will not compile against this code)
- `Adafruit GFX Library`, `Adafruit ST7735 and ST7789 Library`, `Adafruit NeoPixel` — pulled in
  transitively by a couple of upstream headers; not actually used for drawing any more, but still
  referenced in a couple of `#include` lines.

### 3. WiFi credentials for the dashboard

```sh
cp secrets.h.example secrets.h
```

Edit `secrets.h` and fill in your home network's SSID/password. This file is gitignored — it's
never committed.

### 4. Flash it

Connect the board over USB, select the `ESP32 Dev Module` board and the right serial port, and
upload. With `arduino-cli`, from this directory:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32 .
```

On boot it connects to your WiFi and prints the dashboard's URL over serial at 115200 baud.

<br>

# Credits

|User||
|:-:|:-|
|[<img src="https://github.com/cifertech.png" width="60px;"/><br /><sub><a href="https://github.com/cifertech">cifertech</a></sub>](https://github.com/cifertech/ESP32-DIV)|Original work (ESP32-DIV)|
|[<img src="https://github.com/syphr0.png" width="60px;"/><br /><sub><a href="https://github.com/syphr0">syphr0</a></sub>](https://github.com/syphr0/wifi32)|wifi32 fork|
