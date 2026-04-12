#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for LILYGO T-ETH-Elite + SX1262 LoRa Shield (ESP32-S3).
 *
 * Ethernet gateway with SX1262 LoRa shield attached.
 * No display, no battery. Two separate SPI buses: one for LoRa, one for Ethernet.
 */
class Board_LILYGO_T_ETH_Elite_SX1262 : public IBoardConfig {
public:
    // LoRa (SX1262 shield)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 10;      }
    int  pinLoRaMISO()          const override { return 9;       }
    int  pinLoRaMOSI()          const override { return 11;      }
    int  pinLoRaCS()            const override { return 40;      }
    int  pinLoRaRST()           const override { return 46;      }
    int  pinLoRaIRQ()           const override { return 8;       }
    int  pinLoRaBusy()          const override { return 16;      }
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }
    bool loRaIsSX1262()         const override { return true;    }

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

    bool hasWiFiLED()           const override { return false;   }
    int  pinWiFiLED()           const override { return -1;      }
    bool wiFiLEDActiveLow()     const override { return false;   }

    bool hasUserButton()        const override { return true;    }
    int  pinUserButton()        const override { return 0;       }

    bool hasEthernet()          const override { return true;    }

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
