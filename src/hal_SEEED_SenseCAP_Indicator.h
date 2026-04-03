#pragma once

#include "frame.h"

// ── Seeed Studio SenseCAP Indicator ──────────────────────────────────────────
// ESP32-S3R8 (8 MB PSRAM, 8 MB Flash) + RP2040 co-processor
// Display: ST7701S 480×480 RGB parallel interface
// Touch:   FT5x06 on I2C (SDA=39, SCL=40)
// I2C expander: PCA9535 @ 0x20 (controls LCD_CS, LCD_RST, TP_RST, …)
//
// No onboard LoRa — runs as WiFi gateway / relay (loraReady = false)

#define LORA_DEFAULT_TX_POWER 22
#define WIFI_MAX_TX_POWER_DBM 20

// ── SPI pins for SX1262 (directly on ESP32-S3, shared with display init SPI) ──
#define LORA_SCK    41
#define LORA_MISO   47
#define LORA_MOSI   48

// ── PCA9535 I2C expander (0x20): controls display + LoRa control pins ─────────
#define SENSECAP_I2C_SDA    39
#define SENSECAP_I2C_SCL    40
#define PCA9535_ADDR        0x20
// Port 0 – Outputs
#define PCA9535_LORA_CS_BIT  (1 << 0)  // bit 0: SX1262 CS (active low)
#define PCA9535_LORA_RST_BIT (1 << 1)  // bit 1: SX1262 RST (active low)
#define PCA9535_LCD_CS_BIT   (1 << 4)  // bit 4: ST7701S CS (active low)
#define PCA9535_LCD_RST_BIT  (1 << 5)  // bit 5: ST7701S RST
// Port 0 – Inputs
#define PCA9535_LORA_BUSY_BIT (1 << 2) // bit 2: SX1262 BUSY
#define PCA9535_LORA_DIO1_BIT (1 << 3) // bit 3: SX1262 DIO1 (IRQ)
#define PCA9535_TP_RST_BIT    (1 << 7) // bit 7: Touch RST

// Virtual pin numbers for RadioLib HAL (-> PCA9535 bits)
#define LORA_NSS    200   // → PCA9535 bit 0 (CS)
#define LORA_RST    201   // → PCA9535 bit 1 (RST)
#define LORA_BUSY   202   // -> PCA9535 bit 2 (BUSY, input)
#define LORA_DIO1   203   // -> PCA9535 bit 3 (DIO1, input)

// Display RGB interface
#define LCD_BL_PIN          45         // backlight (active high, via GPIO)
#define PIN_AP_MODE_SWITCH  0          // BOOT button (not used, RP2040 pulls low)

void setWiFiLED(bool value);
void initHal();
bool checkReceive(Frame &f);
void transmitFrame(Frame &f);
bool getKeyApMode();

extern bool txFlag;
extern bool rxFlag;
