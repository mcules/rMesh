/*
 * LilyGo T-Echo (nRF52840) variant for rMesh
 * Pin mapping: 1:1 GPIO mapping (Arduino pin N = nRF52 P0.N / P1.(N-32))
 */

#ifndef _VARIANT_LILYGO_T_ECHO_
#define _VARIANT_LILYGO_T_ECHO_

#define VARIANT_MCK       (64000000ul)

#define USE_LFXO      // Board uses 32khz crystal for LF

#include "WVariant.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Total number of pins (P0.0-P0.31 = 0-31, P1.0-P1.15 = 32-47)
#define PINS_COUNT           (48)
#define NUM_DIGITAL_PINS     (48)
#define NUM_ANALOG_INPUTS    (6)
#define NUM_ANALOG_OUTPUTS   (0)

// LEDs
#define PIN_LED1             (1)    // P0.01 Green LED
#define PIN_LED2             (14)   // P0.14 Blue LED

#define LED_BUILTIN          PIN_LED1
#define LED_CONN             PIN_LED2
#define LED_RED              PIN_LED1
#define LED_BLUE             PIN_LED2
#define LED_STATE_ON         1

// Buttons
#define PIN_BUTTON1          (42)   // P1.10 User button

// Analog pins (battery)
#define PIN_A0               (4)    // P0.04 Battery ADC (AIN2)

static const uint8_t A0  = PIN_A0;

// Default SPI is used for E-Paper display
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_MISO         (38)   // P1.06 (E-Paper MISO, unused but required)
#define PIN_SPI_MOSI         (29)   // P0.29 (E-Paper MOSI)
#define PIN_SPI_SCK          (31)   // P0.31 (E-Paper SCK)

static const uint8_t SS   = 30;    // P0.30 E-Paper CS
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK  = PIN_SPI_SCK;

// I2C (for BME280)
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (26)   // P0.26
#define PIN_WIRE_SCL         (27)   // P0.27

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

// UART (optional, for GPS)
#define PIN_SERIAL1_RX       (8)    // P0.08
#define PIN_SERIAL1_TX       (41)   // P1.09

// USB
#define USB_VID              0x239A
#define USB_PID              0x8029
#define USB_MANUFACTURER     "LilyGo"
#define USB_PRODUCT          "T-Echo"

#ifdef __cplusplus
}
#endif

#endif // _VARIANT_LILYGO_T_ECHO_
