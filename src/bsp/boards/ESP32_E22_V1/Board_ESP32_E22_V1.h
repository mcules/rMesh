#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for ESP32 E22 V1 / NANO-VHF / Rentnergang board.
 *
 * Uses SX1262 E22 module with external PA (TX_ENA GPIO13, RX_ENA GPIO14).
 * PA pins are managed in the HAL, not exposed here.
 * Higher max TX power (33 dBm) due to external PA.
 */
class Board_ESP32_E22_V1 : public IBoardConfig {
public:
    // LoRa (SX1262, VSPI)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 5;       }
    int  pinLoRaMISO()          const override { return 19;      }
    int  pinLoRaMOSI()          const override { return 27;      }
    int  pinLoRaCS()            const override { return 18;      }
    int  pinLoRaRST()           const override { return 23;      }
    int  pinLoRaIRQ()           const override { return 33;      }
    int  pinLoRaBusy()          const override { return 32;      }
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }  // External PA
    bool loRaIsSX1262()         const override { return true;    }

    // Display (OLED via I2C — same pins as T3/T-Beam family)
    bool hasDisplay()           const override { return true;    }
    int  pinSDA()               const override { return 21;      }
    int  pinSCL()               const override { return 22;      }
    int  pinDisplayRST()        const override { return -1;      }
    int  pinVext()              const override { return -1;      }
    bool vextActiveLow()        const override { return false;   }

    bool  hasBatteryMonitor()           const override { return false;   }
    int   pinBatteryADC()               const override { return -1;      }
    int   pinBatteryADCEnable()         const override { return -1;      }
    bool  batteryADCEnableActiveLow()   const override { return false;   }
    float batteryVoltageMultiplier()    const override { return 0.0f;    }

    // WiFi status LED (GPIO25, active HIGH)
    bool hasWiFiLED()           const override { return true;    }
    int  pinWiFiLED()           const override { return 25;      }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // No user button in original HAL (PIN_AP_MODE_SWITCH commented out)
    bool hasUserButton()        const override { return false;   }
    int  pinUserButton()        const override { return -1;      }

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
