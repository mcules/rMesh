#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for Heltec HT-Tracker V1.2 (ESP32-S3 + SX1262 + ST7735 TFT).
 *
 * Uses a 80x160 ST7735S TFT (SPI) instead of the OLED on other Heltec boards.
 * Vext (GPIO3) controls power for TFT and GNSS (HIGH = on).
 * Battery ADC enable is active-HIGH (GPIO37 HIGH = measure active).
 */
class Board_Heltec_HT_Tracker_V1_2 : public IBoardConfig {
public:
    // LoRa (SX1262, HSPI)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 9;       }
    int  pinLoRaMISO()          const override { return 11;      }
    int  pinLoRaMOSI()          const override { return 10;      }
    int  pinLoRaCS()            const override { return 8;       }
    int  pinLoRaRST()           const override { return 12;      }
    int  pinLoRaIRQ()           const override { return 14;      }
    int  pinLoRaBusy()          const override { return 13;      }
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }
    bool loRaIsSX1262()         const override { return true;    }

    // Display: ST7735S TFT (SPI) — I2C pins not applicable here.
    // SDA/SCL return -1; TFT-specific pins are in the display driver directly.
    bool hasDisplay()           const override { return true;    }
    int  pinSDA()               const override { return -1;      }
    int  pinSCL()               const override { return -1;      }
    int  pinDisplayRST()        const override { return 39;      }  // TFT_RST
    int  pinVext()              const override { return 3;       }  // Vext: HIGH = on
    bool vextActiveLow()        const override { return false;   }

    // Battery (GPIO37 = enable active-HIGH, GPIO1 = ADC)
    bool  hasBatteryMonitor()           const override { return true;     }
    int   pinBatteryADC()               const override { return 1;        }
    int   pinBatteryADCEnable()         const override { return 37;       }
    bool  batteryADCEnableActiveLow()   const override { return false;    }
    float batteryVoltageMultiplier()    const override { return 0.001611f; }

    // WiFi status LED (GPIO18, active HIGH)
    bool hasWiFiLED()           const override { return true;    }
    int  pinWiFiLED()           const override { return 18;      }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // User button (PRG = GPIO0, active LOW)
    bool hasUserButton()        const override { return true;    }
    int  pinUserButton()        const override { return 0;       }

    bool hasEthernet()          const override { return false;   }

    // ── TFT Display (ST7735S 80x160) ────────────────────────────────────────
    int  pinTFT_MOSI()    const override { return 42; }
    int  pinTFT_SCK()     const override { return 41; }
    int  pinTFT_DC()      const override { return 40; }
    int  pinTFT_CS()      const override { return 38; }
    int  pinTFT_RST()     const override { return 39; }
    int  pinTFT_BL()      const override { return 21; }

    // ── E-Paper Display ──────────────────────────────────────────────────────
    int  pinEPaper_MOSI() const override { return -1; }
    int  pinEPaper_SCK()  const override { return -1; }
    int  pinEPaper_CS()   const override { return -1; }
    int  pinEPaper_DC()   const override { return -1; }
    int  pinEPaper_RES()  const override { return -1; }
    int  pinEPaper_BUSY() const override { return -1; }
    int  pinEPaper_BL()   const override { return -1; }
};
