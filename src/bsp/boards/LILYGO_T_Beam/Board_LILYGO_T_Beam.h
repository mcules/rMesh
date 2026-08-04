#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for LILYGO T-Beam V1.1+ (ESP32 + SX1278).
 *
 * Uses SX1278 (no BUSY pin). No dedicated WiFi LED.
 * User button is GPIO38 (not GPIO0 like most other boards).
 */
class Board_LILYGO_T_Beam : public IBoardConfig {
public:
    // LoRa (SX1278, VSPI)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 5;       }
    int  pinLoRaMISO()          const override { return 19;      }
    int  pinLoRaMOSI()          const override { return 27;      }
    int  pinLoRaCS()            const override { return 18;      }
    int  pinLoRaRST()           const override { return 23;      }
    int  pinLoRaIRQ()           const override { return 26;      }  // DIO0
    int  pinLoRaBusy()          const override { return -1;      }  // SX1278 has no BUSY
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 20;      }
    int  loRaMaxTxPower()       const override { return 20;      }
    bool loRaIsSX1262()         const override { return false;   }

    // Display (OLED via I2C if populated — T-Beam has optional OLED header)
    bool hasDisplay()           const override { return true;    }
    int  pinSDA()               const override { return 21;      }
    int  pinSCL()               const override { return 22;      }
    int  pinDisplayRST()        const override { return -1;      }
    int  pinVext()              const override { return -1;      }
    bool vextActiveLow()        const override { return false;   }

    // No battery ADC routed in the original HAL
    bool  hasBatteryMonitor()           const override { return false;   }
    int   pinBatteryADC()               const override { return -1;      }
    int   pinBatteryADCEnable()         const override { return -1;      }
    bool  batteryADCEnableActiveLow()   const override { return false;   }
    float batteryVoltageMultiplier()    const override { return 0.0f;    }

    // No dedicated WiFi LED
    bool hasWiFiLED()           const override { return false;   }
    int  pinWiFiLED()           const override { return -1;      }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // User button (GPIO38, active LOW)
    bool hasUserButton()        const override { return true;    }
    int  pinUserButton()        const override { return 38;      }

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
