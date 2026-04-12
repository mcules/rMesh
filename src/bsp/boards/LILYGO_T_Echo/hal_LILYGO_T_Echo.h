#pragma once

#include "mesh/frame.h"

// ── LilyGo T-Echo Pin Definitions (nRF52840 + SX1262 + GDEH0154D67 E-Paper) ──

// SX1262 LoRa Radio (SPI)
#define LORA_NSS    24   // P0.24
#define LORA_SCK    19   // P0.19
#define LORA_MOSI   22   // P0.22
#define LORA_MISO   23   // P0.23
#define LORA_DIO1   20   // P0.20  (IRQ)
#define LORA_RST    25   // P0.25
#define LORA_BUSY   17   // P0.17

#define LORA_DEFAULT_TX_POWER 22
#define LORA_MAX_TX_POWER     22  // SX1262 hardware limit
#define WIFI_MAX_TX_POWER_DBM 20

// 1.54" E-Paper Display GDEH0154D67 (200x200, SPI)
#define EINK_CS     30   // P0.30
#define EINK_BUSY   3    // P0.03
#define EINK_DC     28   // P0.28
#define EINK_RES    2    // P0.02
#define EINK_SCK    31   // P0.31
#define EINK_MOSI   29   // P0.29

// User Button
#define PIN_BUTTON  42   // P1.10 (32+10)

// LEDs
#define PIN_LED_GREEN  1    // P0.01
#define PIN_LED_BLUE   14   // P0.14

// Battery ADC
#define PIN_VBAT_ADC   4    // P0.04 (AIN2)
#define HAS_BATTERY_ADC
#define VBAT_DIVIDER_COMP  2.0f  // Voltage divider compensation factor

// GPS L76K (optional, not used in initial firmware)
#define GPS_TX_PIN   41   // P1.09 (32+9)
#define GPS_RX_PIN   8    // P0.08
#define GPS_EN_PIN   35   // P1.03 (32+3)
#define GPS_RESET_PIN 34  // P1.02 (32+2)

// Reuse button pin for AP mode switch (button toggles display pages)
#define PIN_AP_MODE_SWITCH  PIN_BUTTON


// ── HAL Function Declarations ────────────────────────────────────────────────

void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();
float getBatteryVoltage();

extern bool txFlag;
extern bool rxFlag;
