# Flock Camera Emulator

A test tool that emulates Flock Safety camera WiFi and BLE traffic using an ESP32. Sends wildcard probe requests and BLE advertisements with known Flock Safety OUI prefixes and manufacturer ID — the exact signatures that Flock detector devices look for. Built for testing and validating Flock Hunter detectors ([CYD WiFi](https://github.com/LuxStatera/flock-hunter-cyd-wifi), [CYD BLE](https://github.com/LuxStatera/flock-hunter-cyd-ble), and [D1 Mini](https://github.com/LuxStatera/flock-hunter-d1-mini-wifi)) and similar detection tools.

## How It Works

Real Flock Safety cameras emit two detectable signals:

**WiFi (Button 1 — D13):** Cameras periodically send 802.11 wildcard probe requests (empty SSID) as part of their normal operation. This emulator replicates that by crafting raw probe request frames using `esp_wifi_80211_tx()`, with the source MAC set to a known Flock OUI prefix. Sends 5 frames each on channels 1, 6, and 11.

**BLE (Button 2 — D32):** Cameras also broadcast BLE advertisements with manufacturer ID `0x09C8` (XUNTONG). This emulator sends BLE advertisements with that manufacturer ID and Flock OUI data in the payload. Advertises for 3 seconds per press.

Each button press cycles to the next OUI from the list of 32 known prefixes, with random device bytes — like driving past multiple cameras.

## Hardware

- ESP32 WROOM-32 dev board
- 2x momentary push buttons (simple two-pin, no polarity)

### Wiring Diagram

```
ESP32 WROOM-32       Component
──────────────       ─────────
D13 (GPIO 13)   →    WiFi Button (+)
D32 (GPIO 32)   →    BLE Button (+)
GND             →    WiFi Button (-) + BLE Button (-)
```

No external resistors needed — uses internal pull-ups. Buttons have no polarity, either terminal works for either wire.

> **Changing pins:** Edit `#define WIFI_BUTTON_PIN` and `#define BLE_BUTTON_PIN` at the top of the sketch.

## Building & Flashing

**Important:** This sketch requires the `no_ota` partition scheme due to BLE stack size. Without it, compilation will fail with "Sketch too big."

### Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support: **File > Preferences > Additional Board Manager URLs:**
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools > Board > Boards Manager** > search "esp32" > install **ESP32 by Espressif Systems**
4. Open `flock_emulator.ino`
5. **Tools > Board:** Select `ESP32 Dev Module`
6. **Tools > Partition Scheme:** Select `No OTA (2MB APP/2MB SPIFFS)`
7. **Tools > Port:** Select your board's serial port
8. Click **Upload**

### Arduino CLI

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota flock_emulator
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=no_ota --port /dev/cu.usbserial-0001 flock_emulator
```

### PlatformIO

```ini
; platformio.ini
[env:esp32]
platform = espressif32
board = esp32dev
board_build.partitions = no_ota.csv
framework = arduino
```

```bash
pio run -t upload
```

## Usage

1. Power on the ESP32
2. Open Serial Monitor at 115200 baud (optional — shows what's being sent)
3. Press **D13 button** to send a WiFi probe request
4. Press **D32 button** to send a BLE advertisement
5. Your Flock detector should trigger a detection alert

## Serial Output

```
=== Flock Camera Emulator (WiFi + BLE) ===
Loaded 32 Flock OUI prefixes
D13 = WiFi probe request
D32 = BLE advertisement
[BLE] btStart: OK
Ready. Waiting for button press...
[WiFi] Probe as 70:C9:4E:A3:1F:82
[WiFi] Probe as 3C:91:80:5D:E7:19
[BLE] bluedroid init: ESP_OK
[BLE] bluedroid enable: ESP_OK
[BLE] Initialized
[BLE] set_rand_addr: ESP_OK
[BLE] config_adv_data_raw: ESP_OK
[BLE] start_advertising: ESP_OK
[BLE] Advertising started OK
[BLE] Broadcasting OUI 70:C9:4E  addr=F0:C9:4E:A9:6C:9C  mfg=0x09C8
[BLE] Stopped
```

## Detection Methods Triggered

| Button | Detector Alert | Description |
|--------|---------------|-------------|
| D13 (WiFi) | `WILD_PROBE` | Management frame type 0/subtype 4 with matching OUI and empty SSID |
| D13 (WiFi) | `OUI_TX` | Transmitter address matches known Flock OUI |
| D32 (BLE) | Manufacturer ID match | BLE advertisement contains manufacturer ID `0x09C8` |

## Companion Projects

- **[Flock Hunter CYD WiFi](https://github.com/LuxStatera/flock-hunter-cyd-wifi)** — WiFi detector with 32 OUI prefixes + PCAP capture
- **[Flock Hunter CYD BLE](https://github.com/LuxStatera/flock-hunter-cyd-ble)** — Bluetooth detector scanning for manufacturer ID 0x09C8
- **[Flock Hunter D1 Mini WiFi](https://github.com/LuxStatera/flock-hunter-d1-mini-wifi)** — Compact WiFi detector with OLED display + piezo buzzer
- **[Flock Camera Emulator](https://github.com/LuxStatera/flock-camera-emulator)** — ESP32 test tool for validating detectors (this project)

## Credits

OUI prefixes and BLE manufacturer ID from the original **[Flock You](https://github.com/colonelpanichacks/flock-you)** project by **[colonelpanichacks](https://github.com/colonelpanichacks)**. BLE manufacturer ID `0x09C8` research credit to **Will Greenberg** ([@wgreenberg](https://github.com/wgreenberg)).

This firmware and README were written with **[Claude](https://claude.ai)** by Anthropic.

## Legal Disclaimer

This tool is for **testing Flock detector devices only**. It does not interact with, interfere with, or connect to any real Flock Safety infrastructure. It transmits standard 802.11 probe request frames and BLE advertisements — the same types of transmissions every WiFi and Bluetooth device sends. Use responsibly and check your local laws regarding wireless transmission.
