#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for Seeed XIAO ESP32-S3 + Wio-SX1262 (B2B connector).
 *
 * Antenna switch enable pin (GPIO38) is managed by the HAL directly,
 * not exposed through IBoardConfig (it's a radio-internal concern).
 */
class Board_SEEED_XIAO_ESP32S3_Wio_SX1262 : public IBoardConfig {
public:
    // LoRa (SX1262, B2B connector)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 7;       }
    int  pinLoRaMISO()          const override { return 8;       }
    int  pinLoRaMOSI()          const override { return 9;       }
    int  pinLoRaCS()            const override { return 41;      }
    int  pinLoRaRST()           const override { return 42;      }
    int  pinLoRaIRQ()           const override { return 39;      }
    int  pinLoRaBusy()          const override { return 40;      }
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }
    bool loRaIsSX1262()         const override { return true;    }

    // No display
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

    // Built-in user LED (GPIO21, active HIGH)
    bool hasWiFiLED()           const override { return true;    }
    int  pinWiFiLED()           const override { return 21;      }
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
