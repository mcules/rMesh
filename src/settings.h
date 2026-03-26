#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// WiFi network entry for the multi-network list
#define WIFI_NETWORK_SSID_LEN 64
#define WIFI_NETWORK_PW_LEN   64

struct WifiNetwork {
    char ssid[WIFI_NETWORK_SSID_LEN];
    char password[WIFI_NETWORK_PW_LEN];
    bool favorite;
};

//Konfiguration
struct Settings {
  bool dhcpActive;
  bool apMode;
  char wifiSSID[64];    // Kept for display-device compatibility; synced with wifiNetworks
  char wifiPassword[64];
  char mycall[17];
  char position[24];   // lat/lon "48.1234,11.5678" oder Maidenhead-Locator "JN48mw"
  char ntpServer[64];
  IPAddress wifiIP;
  IPAddress wifiNetMask;
  IPAddress wifiGateway;
  IPAddress wifiDNS;
  float loraFrequency;
  int8_t loraOutputPower;
  float loraBandwidth;
  uint8_t loraSyncWord;
  uint8_t loraCodingRate;
  uint8_t loraSpreadingFactor;
  int16_t loraPreambleLength;
  bool loraRepeat;
  IPAddress wifiBrodcast = IPAddress(255, 255, 255, 255);
  uint8_t loraMaxMessageLength;
};

struct ExtSettings {
    uint8_t maxHopMessage = 15;
    uint8_t maxHopPosition = 1;
    uint8_t maxHopTelemetry = 3;
    int8_t minSnr = -30;           // Minimum SNR (dB) for peer availability; -30 = disabled
};

// Dynamic UDP peer list (unlimited, stored separately in NVS)
// NVS key "udpPeers": [count:1][ip:4][legacy:1][enabled:1] per entry
extern std::vector<IPAddress> udpPeers;
extern std::vector<bool> udpPeerLegacy;
extern std::vector<bool> udpPeerEnabled;
extern std::vector<String> udpPeerCall;  // Callsign of UDP peer (RAM, learned on RX)

// Dynamic WiFi network list (unlimited, stored separately in NVS)
// NVS key "wifiNetworks": [count:1][ssid:64][password:64][favorite:1] per entry
extern std::vector<WifiNetwork> wifiNetworks;

// AP (Access Point) settings stored as individual NVS keys
extern String apName;      // AP SSID, default "rMesh"
extern String apPassword;  // AP password, empty = open network

void loadSettings();
void saveSettings();
void saveUdpPeers();      // Save peers only + notify WebUI (no initHal)
void saveWifiNetworks();  // Save WiFi network list + AP settings + notify WebUI (no initHal)
void showSettings();
void sendSettings();

extern Settings settings;
extern ExtSettings extSettings;
extern Preferences prefs;
extern uint8_t updateChannel; // 0=release (default), 1=dev
extern bool loraEnabled;      // HF-Sender aktiv (false = LoRa komplett deaktiviert)
extern bool loraReady;  // true = HF-Modul initialisiert und betriebsbereit
extern bool batteryEnabled;       // Akkustand anzeigen
extern float batteryFullVoltage;  // Spannung bei 100 % (V)

// OLED status display settings (boards with SSD1306)
extern bool oledEnabled;                // Display on/off (persisted, survives reboot)
extern char oledDisplayGroup[17];       // Group to show last message from
void saveOledSettings();                // Persist oledEnabled + oledDisplayGroup to NVS

