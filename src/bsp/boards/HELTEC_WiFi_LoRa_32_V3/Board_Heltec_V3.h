#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262).
 *
 * Verified against HTIT-WB32LA(F)_V3_Schematic_Diagram.pdf
 * PIN_VBAT_CTRL is active-HIGH in the existing HAL (digitalWrite HIGH to enable).
 * Note: V3.2 inverts ADC enable polarity — create Board_Heltec_V3_2 subclass if needed.
 */
class Board_Heltec_V3 : public IBoardConfig {
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

    // Display (SSD1306 128x64 OLED, I2C, powered via Vext GPIO36)
    bool hasDisplay()           const override { return true;    }
    int  pinSDA()               const override { return 17;      }
    int  pinSCL()               const override { return 18;      }
    int  pinDisplayRST()        const override { return 21;      }
    int  pinVext()              const override { return 36;      }
    bool vextActiveLow()        const override { return true;    }

    // Battery (GPIO37 = enable active-HIGH per V3 HAL, GPIO1 = ADC, 1:2 divider)
    bool  hasBatteryMonitor()           const override { return true;     }
    int   pinBatteryADC()               const override { return 1;        }
    int   pinBatteryADCEnable()         const override { return 37;       }
    bool  batteryADCEnableActiveLow()   const override { return false;    }
    float batteryVoltageMultiplier()    const override { return 0.001611f; }
    // 0.001611 = (3.3 / 4095) * 2

    // WiFi status LED (GPIO35, active HIGH)
    bool hasWiFiLED()           const override { return true;    }
    int  pinWiFiLED()           const override { return 35;      }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // User button (PRG = GPIO0, active LOW)
    bool hasUserButton()        const override { return true;    }
    int  pinUserButton()        const override { return 0;       }

    bool hasEthernet()          const override { return false;   }

    // ── TFT Display ──────────────────────────────────────────────────────────
    int  pinTFT_MOSI()    const override { return -1; }
    int  pinTFT_SCK()     const override { return -1; }
    int  pinTFT_DC()      const override { return -1; }
    int  pinTFT_CS()      const override { return -1; }
    int  pinTFT_RST()     const override { return -1; }
    int  pinTFT_BL()      const override { return -1; }

    // ── E-Paper Display ──────────────────────────────────────────────────────
    int  pinEPaper_MOSI() const override { return -1; }
    int  pinEPaper_SCK()  const override { return -1; }
    int  pinEPaper_CS()   const override { return -1; }
    int  pinEPaper_DC()   const override { return -1; }
    int  pinEPaper_RES()  const override { return -1; }
    int  pinEPaper_BUSY() const override { return -1; }
    int  pinEPaper_BL()   const override { return -1; }
};
