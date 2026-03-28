/*
 * LilyGo T-Echo (nRF52840) variant for rMesh
 * 1:1 pin mapping: Arduino pin N = nRF52 GPIO N
 * P0.0-P0.31 = pin 0-31, P1.0-P1.15 = pin 32-47
 */

#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] =
{
    // P0.00 - P0.31 (direct 1:1 mapping)
     0,  // D0  = P0.00
     1,  // D1  = P0.01  (Green LED)
     2,  // D2  = P0.02  (E-Paper RST)
     3,  // D3  = P0.03  (E-Paper BUSY)
     4,  // D4  = P0.04  (Battery ADC)
     5,  // D5  = P0.05
     6,  // D6  = P0.06
     7,  // D7  = P0.07
     8,  // D8  = P0.08  (GPS RX)
     9,  // D9  = P0.09
    10,  // D10 = P0.10
    11,  // D11 = P0.11
    12,  // D12 = P0.12
    13,  // D13 = P0.13
    14,  // D14 = P0.14  (Blue LED)
    15,  // D15 = P0.15
    16,  // D16 = P0.16
    17,  // D17 = P0.17  (SX1262 BUSY)
    18,  // D18 = P0.18
    19,  // D19 = P0.19  (SX1262 SCK)
    20,  // D20 = P0.20  (SX1262 DIO1)
    21,  // D21 = P0.21
    22,  // D22 = P0.22  (SX1262 MOSI)
    23,  // D23 = P0.23  (SX1262 MISO)
    24,  // D24 = P0.24  (SX1262 NSS)
    25,  // D25 = P0.25  (SX1262 RST)
    26,  // D26 = P0.26  (I2C SDA)
    27,  // D27 = P0.27  (I2C SCL)
    28,  // D28 = P0.28  (E-Paper DC)
    29,  // D29 = P0.29  (E-Paper MOSI)
    30,  // D30 = P0.30  (E-Paper CS)
    31,  // D31 = P0.31  (E-Paper SCK)

    // P1.00 - P1.15
    32,  // D32 = P1.00
    33,  // D33 = P1.01
    34,  // D34 = P1.02  (GPS RESET)
    35,  // D35 = P1.03  (GPS EN)
    36,  // D36 = P1.04
    37,  // D37 = P1.05
    38,  // D38 = P1.06
    39,  // D39 = P1.07
    40,  // D40 = P1.08
    41,  // D41 = P1.09  (GPS TX)
    42,  // D42 = P1.10  (User Button)
    43,  // D43 = P1.11
    44,  // D44 = P1.12
    45,  // D45 = P1.13
    46,  // D46 = P1.14
    47,  // D47 = P1.15
};

void initVariant()
{
    // Green LED
    pinMode(PIN_LED1, OUTPUT);
    ledOff(PIN_LED1);

    // Blue LED
    pinMode(PIN_LED2, OUTPUT);
    ledOff(PIN_LED2);
}
