#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>


//Konfiguration
struct Settings {
  bool dhcpActive;
  bool apMode;
  char wifiSSID[64];
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
};

// Dynamische UDP-Peer-Liste (unbegrenzt, separat in NVS gespeichert)
// Format NVS-Key "udpPeers": [count:1][ip:4][legacy:1][enabled:1] pro Eintrag
extern std::vector<IPAddress> udpPeers;
extern std::vector<bool> udpPeerLegacy;
extern std::vector<bool> udpPeerEnabled;
extern std::vector<String> udpPeerCall;  // Rufzeichen des UDP-Peers (RAM, wird beim Empfang gelernt)

void loadSettings();
void saveSettings();
void saveUdpPeers();   // Nur Peers speichern + WebUI benachrichtigen (kein initHal)
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

