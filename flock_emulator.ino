/*
 * Flock Camera Emulator - Test Tool
 * Sends wildcard probe requests using known Flock Safety OUI prefixes
 * when a button is pressed. For testing flock detector devices only.
 *
 * Hardware: ESP32-S3 (or any ESP32), button on GPIO 2, LED on GPIO 4
 * Build with: Arduino IDE with ESP32 board package, or PlatformIO
 */

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#define BUTTON_PIN  13
#define DEBOUNCE_MS 300

// Known Flock Safety OUI prefixes (from flock-you research)
static const uint8_t FLOCK_OUIS[][3] = {
  {0x70, 0xC9, 0x4E},
  {0x3C, 0x91, 0x80},
  {0xD8, 0xF3, 0xBC},
  {0x80, 0x30, 0x49},
  {0xB8, 0x35, 0x32},
  {0x14, 0x5A, 0xFC},
  {0x74, 0x4C, 0xA1},
  {0x08, 0x3A, 0x88},
  {0x9C, 0x2F, 0x9D},
  {0xC0, 0x35, 0x32},
  {0x94, 0x08, 0x53},
  {0xE4, 0xAA, 0xEA},
  {0xF4, 0x6A, 0xDD},
  {0xF8, 0xA2, 0xD6},
  {0x24, 0xB2, 0xB9},
  {0x00, 0xF4, 0x8D},
  {0xD0, 0x39, 0x57},
  {0xE8, 0xD0, 0xFC},
  {0xE0, 0x4F, 0x43},
  {0xB8, 0x1E, 0xA4},
  {0x70, 0x08, 0x94},
  {0x58, 0x8E, 0x81},
  {0xEC, 0x1B, 0xBD},
  {0x3C, 0x71, 0xBF},
  {0x58, 0x00, 0xE3},
  {0x90, 0x35, 0xEA},
  {0x5C, 0x93, 0xA2},
  {0x64, 0x6E, 0x69},
  {0x48, 0x27, 0xEA},
  {0xA4, 0xCF, 0x12},
  {0x82, 0x6B, 0xF2},
  {0xB4, 0x1E, 0x52},  // registered Flock Safety OUI
};

static const int NUM_OUIS = sizeof(FLOCK_OUIS) / sizeof(FLOCK_OUIS[0]);
static const uint8_t PROBE_CHANNELS[] = {1, 6, 11};

static int oui_index = 0;
static unsigned long last_press = 0;

// 802.11 probe request frame with zero-length SSID (wildcard probe)
// This is the exact signature the flock-you detector looks for:
// Type 0 (Management), Subtype 4 (Probe Request), empty SSID element
static uint8_t probe_packet[] = {
  // Frame Control: Probe Request (Type 0, Subtype 4)
  0x40, 0x00,
  // Duration
  0x00, 0x00,
  // Addr1: Destination (broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // Addr2: Source (transmitter) — will be overwritten with Flock OUI
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // Addr3: BSSID (broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // Sequence Control
  0x00, 0x00,
  // Tagged Parameters:
  // SSID element: tag=0, length=0 (wildcard — zero-length SSID)
  0x00, 0x00,
  // Supported Rates element
  0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24,
};

void set_source_mac(const uint8_t oui[3]) {
  // Addr2 (source/transmitter) starts at byte 10
  probe_packet[10] = oui[0];
  probe_packet[11] = oui[1];
  probe_packet[12] = oui[2];
  // Random lower 3 bytes for device portion
  probe_packet[13] = esp_random() & 0xFF;
  probe_packet[14] = esp_random() & 0xFF;
  probe_packet[15] = esp_random() & 0xFF;
}

void send_flock_probe() {
  const uint8_t *oui = FLOCK_OUIS[oui_index];

  set_source_mac(oui);

  Serial.printf("Sending wildcard probe as %02X:%02X:%02X:%02X:%02X:%02X\n",
    probe_packet[10], probe_packet[11], probe_packet[12],
    probe_packet[13], probe_packet[14], probe_packet[15]);

  // Send on channels 1, 6, and 11 to match real camera behavior
  for (int i = 0; i < 3; i++) {
    esp_wifi_set_channel(PROBE_CHANNELS[i], WIFI_SECOND_CHAN_NONE);
    delay(10);

    // Send multiple frames per channel to increase detection probability
    for (int j = 0; j < 5; j++) {
      esp_wifi_80211_tx(WIFI_IF_STA, probe_packet, sizeof(probe_packet), false);
      delay(5);
    }
  }

  // Cycle to next OUI for variety
  oui_index = (oui_index + 1) % NUM_OUIS;

}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Flock Camera Emulator (Test Tool) ===");
  Serial.printf("Loaded %d Flock OUI prefixes\n", NUM_OUIS);
  Serial.println("Press button to send a wildcard probe request");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Init WiFi in station mode for raw frame injection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.println("Ready. Waiting for button press...");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - last_press > DEBOUNCE_MS) {
      last_press = now;
      send_flock_probe();
    }
  }
  delay(10);
}
