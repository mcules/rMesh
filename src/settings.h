#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"

#ifdef NRF52_PLATFORM
#include "platform_nrf52.h"
#else
#include <Preferences.h>
#endif

// WiFi network entry for the multi-network list
#define WIFI_NETWORK_SSID_LEN 64
#define WIFI_NETWORK_PW_LEN   64

struct WifiNetwork {
    char ssid[WIFI_NETWORK_SSID_LEN];
    char password[WIFI_NETWORK_PW_LEN];
    bool favorite;
};

//Configuration
struct Settings {
  bool dhcpActive;
  bool apMode;
  char wifiSSID[64];    // Kept for display-device compatibility; synced with wifiNetworks
  char wifiPassword[64];
  char mycall[17];  // WebUI validates MAX_CALLSIGN_LENGTH; keep [17] for NVS struct compatibility
  char position[24];   // lat/lon "48.1234,11.5678" or Maidenhead locator "JN48mw"
  char ntpServer[64];
#ifdef HAS_WIFI
  IPAddress wifiIP;
  IPAddress wifiNetMask;
  IPAddress wifiGateway;
  IPAddress wifiDNS;
#endif
  float loraFrequency;
  int8_t loraOutputPower;
  float loraBandwidth;
  uint8_t loraSyncWord;
  uint8_t loraCodingRate;
  uint8_t loraSpreadingFactor;
  int16_t loraPreambleLength;
  bool loraRepeat;
#ifdef HAS_WIFI
  IPAddress wifiBrodcast = IPAddress(255, 255, 255, 255);
#endif
  uint8_t loraMaxMessageLength;
};

struct ExtSettings {
    uint8_t maxHopMessage = 15;
    uint8_t maxHopPosition = 1;
    uint8_t maxHopTelemetry = 3;
    int8_t minSnr = -30;           // Minimum SNR (dB) for peer availability; -30 = disabled
};

#ifdef HAS_WIFI
// Dynamic UDP peer list (unlimited, stored separately in NVS)
// NVS key "udpPeers": [count:1][ip:4][legacy:1][enabled:1] per entry
extern std::vector<IPAddress> udpPeers;
extern std::vector<bool> udpPeerLegacy;
extern std::vector<bool> udpPeerEnabled;
// Fixed-size callsign type avoids heap-fragmenting String allocations on the UDP RX path
struct UdpPeerCallsign {
    char call[MAX_CALLSIGN_LENGTH + 1];
    UdpPeerCallsign() { call[0] = '\0'; }
    UdpPeerCallsign(const char* s) { strlcpy(call, s ? s : "", sizeof(call)); }
    const char* c_str() const { return call; }
    bool operator!=(const char* s) const { return strcmp(call, s) != 0; }
    UdpPeerCallsign& operator=(const char* s) { strlcpy(call, s ? s : "", sizeof(call)); return *this; }
};
extern std::vector<UdpPeerCallsign> udpPeerCall;

// Dynamic WiFi network list (unlimited, stored separately in NVS)
// NVS key "wifiNetworks": [count:1][ssid:64][password:64][favorite:1] per entry
extern std::vector<WifiNetwork> wifiNetworks;

// AP (Access Point) settings stored as individual NVS keys
extern String apName;      // AP SSID, default "rMesh"
extern String apPassword;  // AP password, empty = open network
#endif

void loadSettings();
void saveSettings();
#ifdef HAS_WIFI
void saveUdpPeers();      // Save peers only + notify WebUI (no initHal)
void saveWifiNetworks();  // Save WiFi network list + AP settings + notify WebUI (no initHal)
#endif
void showSettings();
void sendSettings();

extern Settings settings;
extern ExtSettings extSettings;
extern Preferences prefs;
extern uint8_t updateChannel; // 0=release (default), 1=dev
extern bool loraEnabled;      // RF transmitter active (false = LoRa completely disabled)
extern bool loraReady;  // true = RF module initialized and operational
extern bool batteryEnabled;       // Show battery level
extern float batteryFullVoltage;  // Voltage at 100% (V)

// WiFi TX power (dBm, persisted, clamped to WIFI_MAX_TX_POWER_DBM per HAL)
extern int8_t wifiTxPower;

// ── Ethernet settings (persisted as individual NVS keys) ─────────────────────
// Only meaningful on boards with HAS_ETHERNET; stubs exist on all HAS_WIFI builds
// so that the API/WebSocket code compiles unconditionally.
#ifdef HAS_WIFI
extern bool wifiEnabled;           // Enable WiFi interface (only togglable on boards with ETH)
extern bool ethEnabled;            // Enable W5500 Ethernet interface
extern bool ethDhcp;               // Use DHCP on Ethernet (true) or static IP (false)
extern IPAddress ethIP;
extern IPAddress ethNetMask;
extern IPAddress ethGateway;
extern IPAddress ethDNS;

// Per-interface service flags (only relevant when both WiFi and ETH are present)
extern bool wifiNodeComm;          // Allow node communication (UDP) on WiFi
extern bool wifiWebUI;             // Allow WebUI access on WiFi
extern bool ethNodeComm;           // Allow node communication (UDP) on Ethernet
extern bool ethWebUI;              // Allow WebUI access on Ethernet

// Primary interface for outbound traffic (OTA, NTP, reporting)
// 0 = auto, 1 = WiFi preferred, 2 = LAN preferred
extern uint8_t primaryInterface;

void saveEthSettings();
#endif

// Display brightness (0-255, persisted, applied to LCD/OLED where supported)
extern uint8_t displayBrightness;

// CPU frequency in MHz (80, 160, 240; persisted, applied at boot)
extern uint16_t cpuFrequency;

// OLED status display settings (boards with SSD1306)
extern bool oledEnabled;                // Display on/off (persisted, survives reboot)
extern char oledDisplayGroup[17];       // Group to show last message from
extern uint16_t oledPageInterval;       // Auto-rotate interval in ms (persisted)
extern uint8_t  oledPageMask;           // Bitmask of pages to include in rotation (persisted)
extern int8_t   oledButtonPin;          // GPIO for manual page-next button (-1 = disabled, persisted)
void saveOledSettings();                // Persist oledEnabled + oledDisplayGroup to NVS

// Channel/group names (persisted on device, shared across all clients)
#define MAX_CHANNELS 10
#define MAX_GROUP_NAME_LEN 17
extern char groupNames[MAX_CHANNELS + 1][MAX_GROUP_NAME_LEN];  // index 1-10
void saveGroupNames();
void loadGroupNames();
