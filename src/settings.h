#pragma once

#include <Arduino.h>
#include <Preferences.h>


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
    IPAddress udpPeer[5] = {
        IPAddress(0, 0, 0, 0),
        IPAddress(0, 0, 0, 0),
        IPAddress(0, 0, 0, 0),
        IPAddress(0, 0, 0, 0),
        IPAddress(0, 0, 0, 0)
    };  
    uint8_t maxHopMessage = 15;
    uint8_t maxHopPosition = 1;
    uint8_t maxHopTelemetry = 3;
};

void loadSettings();
void saveSettings();
void showSettings();
void sendSettings();

extern Settings settings;
extern ExtSettings extSettings;
extern Preferences prefs;
extern bool loraReady;  // true = HF-Modul initialisiert und betriebsbereit

