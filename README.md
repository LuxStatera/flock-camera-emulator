# Flock Camera Emulator

A test tool that emulates Flock Safety camera WiFi traffic using an ESP32. Sends wildcard probe requests with known Flock Safety OUI prefixes — the exact signature that Flock detector devices look for. Built for testing and validating [Flock Hunter](https://github.com/LuxStatera/flock-hunter-cyd) and similar detection tools.

## How It Works

Real Flock Safety cameras periodically send 802.11 wildcard probe requests (empty SSID) as part of their normal operation. This emulator replicates that behavior by crafting raw probe request frames using the ESP32's `esp_wifi_80211_tx()` function, with the source MAC set to a known Flock OUI prefix.

Each button press:
1. Picks the next Flock OUI from the list of 31 known prefixes
2. Generates a random device MAC with that OUI prefix
3. Sends 5 wildcard probe frames on each of channels 1, 6, and 11
4. Cycles to the next OUI for the next press

This triggers the `WILD_PROBE` detection method (highest confidence) on any Flock detector in range.

## Hardware

Any ESP32 board with a button:

| Part | Pin | Notes |
|------|-----|-------|
| ESP32 (any variant) | — | ESP32, ESP32-S3, ESP32-C3, etc. |
| Momentary button | GPIO 13 → GND | Uses internal pull-up |

That's it — no display, no buzzer, just a board and a button.

> **Changing the button pin:** Edit `#define BUTTON_PIN 13` at the top of the sketch.

## Building & Flashing

### Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support: **File → Preferences → Additional Board Manager URLs:**
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** → search "esp32" → install **ESP32 by Espressif Systems**
4. Open `flock_emulator.ino`
5. **Tools → Board:** Select your ESP32 board (e.g. `ESP32 Dev Module`, `ESP32-S3 Dev Module`)
6. **Tools → Port:** Select your board's serial port
7. Click **Upload**

### PlatformIO

```bash
# Create platformio.ini with your board, then:
pio run -t upload
```

## Usage

1. Power on the ESP32
2. Open Serial Monitor at 115200 baud (optional — shows what's being sent)
3. Press the button to send a Flock camera probe
4. Your Flock detector should trigger a detection alert

Each press uses a different OUI prefix and random device bytes, so the detector sees unique MAC addresses — just like driving past multiple cameras.

## Serial Output

```
=== Flock Camera Emulator (Test Tool) ===
Loaded 31 Flock OUI prefixes
Press button to send a wildcard probe request
Ready. Waiting for button press...
Sending wildcard probe as 70:C9:4E:A3:1F:82
Sending wildcard probe as 3C:91:80:5D:E7:19
Sending wildcard probe as D8:F3:BC:44:92:CB
```

## Companion Projects

- **[Flock Hunter CYD](https://github.com/LuxStatera/flock-hunter-cyd)** — ESP32 CYD detector with TFT display, SD card PCAP capture
- **[Flock Hunter D1 Mini](https://github.com/LuxStatera/flock-hunter-d1-mini)** — ESP8266 detector with OLED display and piezo buzzer

## Credits

OUI prefixes from the original **[Flock You](https://github.com/colonelpanichacks/flock-you)** project by **[colonelpanichacks](https://github.com/colonelpanichacks)**.

This firmware and README were written with **[Claude](https://claude.ai)** by Anthropic.

## Legal Disclaimer

This tool is for **testing Flock detector devices only**. It does not interact with, interfere with, or connect to any real Flock Safety infrastructure. It transmits standard 802.11 probe request frames — the same type of frame every WiFi device sends when scanning for networks. Use responsibly and check your local laws regarding WiFi frame transmission.
