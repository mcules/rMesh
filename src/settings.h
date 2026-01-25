#pragma once

#include <Arduino.h>


//Konfiguration
struct Settings {
  bool dhcpActive;
  bool apMode;
  char wifiSSID[64];
  char wifiPassword[64];
  char mycall[17];     
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

void loadSettings();
void saveSettings();
void showSettings();
void sendSettings();

extern Settings settings;

