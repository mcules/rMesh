#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for LILYGO T-Echo (nRF52840 + SX1262 + E-Paper).
 *
 * Uses nRF52840 — different GPIO numbering to all ESP32 boards.
 * E-Paper display (GDEH0154D67, 200x200): SPI-based, not I2C.
 * pinSDA/pinSCL return -1; display driver handles its own SPI pins.
 * Battery ADC on GPIO4 (AIN2), no enable control pin.
 */
class Board_LILYGO_T_Echo : public IBoardConfig {
public:
    // LoRa (SX1262, SPI)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 19;      }
    int  pinLoRaMISO()          const override { return 23;      }
    int  pinLoRaMOSI()          const override { return 22;      }
    int  pinLoRaCS()            const override { return 24;      }
    int  pinLoRaRST()           const override { return 25;      }
    int  pinLoRaIRQ()           const override { return 20;      }
    int  pinLoRaBusy()          const override { return 17;      }
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }
    bool loRaIsSX1262()         const override { return true;    }

    // E-Paper display — SPI, not I2C. Display driver manages its own pins.
    bool hasDisplay()           const override { return true;    }
    int  pinSDA()               const override { return -1;      }
    int  pinSCL()               const override { return -1;      }
    int  pinDisplayRST()        const override { return 2;       }  // EINK_RES
    int  pinVext()              const override { return -1;      }
    bool vextActiveLow()        const override { return false;   }

    // Battery ADC (GPIO4 / AIN2, always enabled — no enable pin)
    bool  hasBatteryMonitor()           const override { return true;    }
    int   pinBatteryADC()               const override { return 4;       }
    int   pinBatteryADCEnable()         const override { return -1;      }
    bool  batteryADCEnableActiveLow()   const override { return false;   }
    float batteryVoltageMultiplier()    const override { return 0.001611f; }

    // Status LEDs (green GPIO1, blue GPIO14 — use green as WiFi indicator)
    bool hasWiFiLED()           const override { return true;    }
    int  pinWiFiLED()           const override { return 1;       }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // User button (GPIO42, active LOW)
    bool hasUserButton()        const override { return true;    }
    int  pinUserButton()        const override { return 42;      }

    bool hasEthernet()          const override { return false;   }

    // ── TFT Display ──────────────────────────────────────────────────────────
    int  pinTFT_MOSI()    const override { return -1; }
    int  pinTFT_SCK()     const override { return -1; }
    int  pinTFT_DC()      const override { return -1; }
    int  pinTFT_CS()      const override { return -1; }
    int  pinTFT_RST()     const override { return -1; }
    int  pinTFT_BL()      const override { return -1; }

    // ── E-Paper Display (GDEH0154D67 200x200) ───────────────────────────────
    int  pinEPaper_MOSI() const override { return 29; }
    int  pinEPaper_SCK()  const override { return 31; }
    int  pinEPaper_CS()   const override { return 30; }
    int  pinEPaper_DC()   const override { return 28; }
    int  pinEPaper_RES()  const override { return 2; }
    int  pinEPaper_BUSY() const override { return 3; }
    int  pinEPaper_BL()   const override { return 43; }
};
