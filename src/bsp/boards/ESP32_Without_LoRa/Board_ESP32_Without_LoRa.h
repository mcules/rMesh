#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for generic ESP32 without LoRa (WiFi relay / gateway only).
 *
 * All LoRa-related methods return -1 / false / 0.
 * Used for pure WiFi mesh relay nodes.
 */
class Board_ESP32_Without_LoRa : public IBoardConfig {
public:
    bool hasLoRa()              const override { return false;   }
    int  pinLoRaSCK()           const override { return -1;      }
    int  pinLoRaMISO()          const override { return -1;      }
    int  pinLoRaMOSI()          const override { return -1;      }
    int  pinLoRaCS()            const override { return -1;      }
    int  pinLoRaRST()           const override { return -1;      }
    int  pinLoRaIRQ()           const override { return -1;      }
    int  pinLoRaBusy()          const override { return -1;      }
    long loRaFrequency()        const override { return 0;       }
    int  loRaDefaultTxPower()   const override { return 0;       }
    int  loRaMaxTxPower()       const override { return 0;       }
    bool loRaIsSX1262()         const override { return false;   }

    bool hasDisplay()           const override { return false;   }
    int  pinSDA()               const override { return -1;      }
    int  pinSCL()               const override { return -1;      }
    int  pinDisplayRST()        const override { return -1;      }
    int  pinVext()              const override { return -1;      }
    bool vextActiveLow()        const override { return false;   }

    bool  hasBatteryMonitor()           const override { return false;   }
    int   pinBatteryADC()               const override { return -1;      }
    int   pinBatteryADCEnable()         const override { return -1;      }
    bool  batteryADCEnableActiveLow()   const override { return false;   }
    float batteryVoltageMultiplier()    const override { return 0.0f;    }

    // WiFi status LED (GPIO2, active HIGH — standard ESP32 dev board LED)
    bool hasWiFiLED()           const override { return true;    }
    int  pinWiFiLED()           const override { return 2;       }
    bool wiFiLEDActiveLow()     const override { return false;   }

    // BOOT button (GPIO0, active LOW)
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
