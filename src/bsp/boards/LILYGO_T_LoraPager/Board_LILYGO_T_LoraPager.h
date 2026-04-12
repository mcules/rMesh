#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for LILYGO T-LoraPager (ESP32-S3 + SX1262).
 *
 * No dedicated WiFi LED. Display details managed by the existing display driver.
 */
class Board_LILYGO_T_LoraPager : public IBoardConfig {
public:
    // LoRa (SX1262, shared SPI bus)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 35;      }
    int  pinLoRaMISO()          const override { return 33;      }
    int  pinLoRaMOSI()          const override { return 34;      }
    int  pinLoRaCS()            const override { return 36;      }
    int  pinLoRaRST()           const override { return 47;      }
    int  pinLoRaIRQ()           const override { return 14;      }
    int  pinLoRaBusy()          const override { return 48;      }
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }
    bool loRaIsSX1262()         const override { return true;    }

    // Display present but pins managed entirely by display driver
    bool hasDisplay()           const override { return true;    }
    int  pinSDA()               const override { return -1;      }
    int  pinSCL()               const override { return -1;      }
    int  pinDisplayRST()        const override { return -1;      }
    int  pinVext()              const override { return -1;      }
    bool vextActiveLow()        const override { return false;   }

    // No battery ADC in original HAL
    bool  hasBatteryMonitor()           const override { return false;   }
    int   pinBatteryADC()               const override { return -1;      }
    int   pinBatteryADCEnable()         const override { return -1;      }
    bool  batteryADCEnableActiveLow()   const override { return false;   }
    float batteryVoltageMultiplier()    const override { return 0.0f;    }

    // No dedicated WiFi LED
    bool hasWiFiLED()           const override { return false;   }
    int  pinWiFiLED()           const override { return -1;      }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // User button (BOOT = GPIO0, active LOW)
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
