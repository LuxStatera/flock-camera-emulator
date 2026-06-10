/*
 * Flock Camera Emulator - Test Tool
 * Button 1 (D13): Sends WiFi wildcard probe requests with Flock OUI
 * Button 2 (D14): Sends BLE advertisements with Flock OUI + manufacturer ID 0x09C8
 * For testing flock detector devices only.
 *
 * Hardware: ESP32 WROOM-32, two buttons
 * Build with: Arduino IDE with ESP32 board package
 */

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

#define WIFI_BUTTON_PIN  13
#define BLE_BUTTON_PIN   32
#define DEBOUNCE_MS      300

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
};

static const int NUM_OUIS = sizeof(FLOCK_OUIS) / sizeof(FLOCK_OUIS[0]);
static const uint8_t PROBE_CHANNELS[] = {1, 6, 11};

// Flock BLE manufacturer ID (XUNTONG) - credit: Will Greenberg (@wgreenberg)
#define FLOCK_MFG_ID 0x09C8

static int wifi_oui_index = 0;
static int ble_oui_index = 0;
static unsigned long last_wifi_press = 0;
static unsigned long last_ble_press = 0;
static bool ble_initialized = false;

// ===================== WiFi Section =====================

// 802.11 probe request frame with zero-length SSID (wildcard probe)
static uint8_t probe_packet[] = {
  // Frame Control: Probe Request (Type 0, Subtype 4)
  0x40, 0x00,
  // Duration
  0x00, 0x00,
  // Addr1: Destination (broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // Addr2: Source (transmitter) — overwritten with Flock OUI
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // Addr3: BSSID (broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // Sequence Control
  0x00, 0x00,
  // SSID element: tag=0, length=0 (wildcard — zero-length SSID)
  0x00, 0x00,
  // Supported Rates element
  0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24,
};

void set_source_mac(const uint8_t oui[3]) {
  probe_packet[10] = oui[0];
  probe_packet[11] = oui[1];
  probe_packet[12] = oui[2];
  probe_packet[13] = esp_random() & 0xFF;
  probe_packet[14] = esp_random() & 0xFF;
  probe_packet[15] = esp_random() & 0xFF;
}

void send_flock_wifi() {
  const uint8_t *oui = FLOCK_OUIS[wifi_oui_index];
  set_source_mac(oui);

  Serial.printf("[WiFi] Probe as %02X:%02X:%02X:%02X:%02X:%02X\n",
    probe_packet[10], probe_packet[11], probe_packet[12],
    probe_packet[13], probe_packet[14], probe_packet[15]);

  for (int i = 0; i < 3; i++) {
    esp_wifi_set_channel(PROBE_CHANNELS[i], WIFI_SECOND_CHAN_NONE);
    delay(10);
    for (int j = 0; j < 5; j++) {
      esp_wifi_80211_tx(WIFI_IF_STA, probe_packet, sizeof(probe_packet), false);
      delay(5);
    }
  }

  wifi_oui_index = (wifi_oui_index + 1) % NUM_OUIS;
}

// ===================== BLE Section =====================

// Build raw advertising PDU so we have full control over the address bytes.
// The ESP32 can send raw HCI commands, but the simplest reliable method
// is to construct the full adv data with the exact OUI we want.

static volatile bool adv_data_set = false;
static volatile bool adv_started = false;

static esp_ble_adv_params_t adv_params = {
  .adv_int_min = 0x20,   // 20ms
  .adv_int_max = 0x40,   // 40ms
  .adv_type = ADV_TYPE_NONCONN_IND,
  .own_addr_type = BLE_ADDR_TYPE_RANDOM,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      adv_data_set = true;
      break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        adv_started = true;
        Serial.println("[BLE] Advertising started OK");
      } else {
        Serial.printf("[BLE] Adv start FAILED: %d\n", param->adv_start_cmpl.status);
      }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      adv_started = false;
      break;
    default:
      break;
  }
}

void init_ble() {
  if (ble_initialized) return;

  esp_err_t ret;

  ret = esp_bluedroid_init();
  Serial.printf("[BLE] bluedroid init: %s\n", esp_err_to_name(ret));

  ret = esp_bluedroid_enable();
  Serial.printf("[BLE] bluedroid enable: %s\n", esp_err_to_name(ret));

  esp_ble_gap_register_callback(gap_event_handler);

  ble_initialized = true;
  Serial.println("[BLE] Initialized");
}

void send_flock_ble() {
  if (!ble_initialized) {
    init_ble();
    delay(500);
  }

  const uint8_t *oui = FLOCK_OUIS[ble_oui_index];

  // Stop any current advertising
  esp_ble_gap_stop_advertising();
  delay(100);

  // Set BLE address with exact Flock OUI — no bit mangling
  // BLE random static addresses require top 2 bits of byte[0] = 11,
  // which corrupts OUIs like 70:C9:4E → F0:C9:4E.
  // Instead we use raw advertising data which includes the address
  // in the manufacturer-specific data, and also set the address
  // with the proper OUI for scanners that check the source address.
  esp_bd_addr_t ble_addr = {
    oui[0], oui[1], oui[2],
    (uint8_t)(esp_random() & 0xFF),
    (uint8_t)(esp_random() & 0xFF),
    (uint8_t)(esp_random() & 0xFF),
  };

  // Force top 2 bits for BLE random static compliance
  // but preserve the OUI in manufacturer data for detectors
  esp_bd_addr_t rand_addr;
  memcpy(rand_addr, ble_addr, 6);
  rand_addr[0] |= 0xC0;

  esp_err_t ret = esp_ble_gap_set_rand_addr(rand_addr);
  Serial.printf("[BLE] set_rand_addr: %s\n", esp_err_to_name(ret));
  delay(50);

  // Build raw advertisement data with:
  // - Flags
  // - Manufacturer Specific Data containing Flock mfg ID 0x09C8
  //   AND the real unmodified OUI so the detector matches it
  uint8_t raw_adv_data[] = {
    // AD 1: Flags
    0x02, 0x01, 0x06,  // length=2, type=flags, LE General Disc + BR/EDR Not Supported
    // AD 2: Manufacturer Specific Data
    0x09, 0xFF,        // length=9, type=0xFF (manufacturer specific)
    (FLOCK_MFG_ID & 0xFF), ((FLOCK_MFG_ID >> 8) & 0xFF),  // company ID: 0x09C8
    oui[0], oui[1], oui[2],  // Flock OUI (unmodified)
    ble_addr[3], ble_addr[4], ble_addr[5],  // device bytes
    0x01,  // padding byte
  };

  adv_data_set = false;
  ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
  Serial.printf("[BLE] config_adv_data_raw: %s\n", esp_err_to_name(ret));

  // Wait for data to be set (callback driven)
  int timeout = 0;
  while (!adv_data_set && timeout < 50) {
    delay(10);
    timeout++;
  }

  if (!adv_data_set) {
    Serial.println("[BLE] WARN: adv data set timeout");
  }

  // Start advertising
  adv_started = false;
  ret = esp_ble_gap_start_advertising(&adv_params);
  Serial.printf("[BLE] start_advertising: %s\n", esp_err_to_name(ret));

  // Wait for confirm
  timeout = 0;
  while (!adv_started && timeout < 50) {
    delay(10);
    timeout++;
  }

  Serial.printf("[BLE] Broadcasting OUI %02X:%02X:%02X  addr=%02X:%02X:%02X:%02X:%02X:%02X  mfg=0x%04X\n",
    oui[0], oui[1], oui[2],
    rand_addr[0], rand_addr[1], rand_addr[2],
    ble_addr[3], ble_addr[4], ble_addr[5],
    FLOCK_MFG_ID);

  // Advertise for 3 seconds
  delay(3000);
  esp_ble_gap_stop_advertising();
  Serial.println("[BLE] Stopped");

  ble_oui_index = (ble_oui_index + 1) % NUM_OUIS;
}

// ===================== Setup & Loop =====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Flock Camera Emulator (WiFi + BLE) ===");
  Serial.printf("Loaded %d Flock OUI prefixes\n", NUM_OUIS);
  Serial.println("D13 = WiFi probe request");
  Serial.println("D32 = BLE advertisement");

  pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BLE_BUTTON_PIN, INPUT_PULLUP);

  // Start BT using Arduino's native function (handles controller init)
  btStart();
  Serial.printf("[BLE] btStart: %s\n", btStarted() ? "OK" : "FAILED");

  // Init WiFi for raw frame injection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.println("Ready. Waiting for button press...");
}

void loop() {
  unsigned long now = millis();

  // WiFi button (D13)
  if (digitalRead(WIFI_BUTTON_PIN) == LOW) {
    if (now - last_wifi_press > DEBOUNCE_MS) {
      last_wifi_press = now;
      send_flock_wifi();
    }
  }

  // BLE button (D14)
  if (digitalRead(BLE_BUTTON_PIN) == LOW) {
    if (now - last_ble_press > DEBOUNCE_MS) {
      last_ble_press = now;
      send_flock_ble();
    }
  }

  delay(10);
}
