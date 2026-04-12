
#pragma once

#include "mesh/frame.h"

// Pin definitions for LilyGO T-ETH-Elite ESP32-S3 + SX1262 868 MHz Shield
// Pin mapping per LilyGO official example (utilities.h):
// https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/blob/master/examples/T-ETH-ELite-Shield/T-ETH-Elite-LoRa-Shield/utilities.h

// ── SX1262 LoRa Shield ──────────────────────────────────────────────────────
#define LORA_SCK    10
#define LORA_MISO   9
#define LORA_MOSI   11
#define LORA_NSS    40  // SPI chip select (RADIO_CS_PIN)
#define LORA_RST    46  // Reset          (RADIO_RST_PIN)
#define LORA_DIO1   8   // IRQ            (RADIO_IRQ_PIN)
#define LORA_BUSY   16  // BUSY           (RADIO_BUSY_PIN)

#define LORA_DEFAULT_TX_POWER 22
#define LORA_MAX_TX_POWER     22  // SX1262 hardware limit
#define WIFI_MAX_TX_POWER_DBM 20

// ── W5500 Ethernet (shares SPI bus with SD) ─────────────────────────────────
#define ETH_SPI_SCK   48
#define ETH_SPI_MISO  47
#define ETH_SPI_MOSI  21
#define ETH_SPI_CS    45
#define ETH_INT_PIN   14
#define ETH_RST_PIN   -1  // No hardware reset pin on T-ETH-Elite
#define ETH_PHY_ADDR  1

// ── SD Card (directly connected, active-low CS) ────────────────────────────
#define SD_CS  12

// ── Misc ─────────────────────────────────────────────────────────────────────
// No user-controllable LED on T-ETH-Elite; stub the WiFi LED pin.
#define PIN_WIFI_LED  -1
// BOOT button (active-low)
#define PIN_AP_MODE_SWITCH 0


void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
