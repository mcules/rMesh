#pragma once
#include "bsp/IBoardConfig.h"

/**
 * @brief Board config for Seeed SenseCAP Indicator (ESP32-S3 + SX1262 via PCA9535).
 *
 * SX1262 control pins (CS, RST, BUSY, DIO1) are not directly connected to
 * ESP32 GPIOs — they are routed through a PCA9535 I2C GPIO expander at 0x20.
 * The HAL uses virtual pin numbers 200-203 mapped to expander bits.
 * pinLoRaCS/RST/IRQ/Busy return these virtual numbers; the HAL interprets them.
 *
 * No battery ADC. No dedicated WiFi LED (RP2040 co-processor handles UI).
 */
class Board_SEEED_SenseCAP_Indicator : public IBoardConfig {
public:
    // LoRa (SX1262, via PCA9535 expander for control pins)
    bool hasLoRa()              const override { return true;    }
    int  pinLoRaSCK()           const override { return 41;      }
    int  pinLoRaMISO()          const override { return 47;      }
    int  pinLoRaMOSI()          const override { return 48;      }
    int  pinLoRaCS()            const override { return 200;     }  // PCA9535 bit 0
    int  pinLoRaRST()           const override { return 201;     }  // PCA9535 bit 1
    int  pinLoRaIRQ()           const override { return 203;     }  // PCA9535 bit 3
    int  pinLoRaBusy()          const override { return 202;     }  // PCA9535 bit 2
    long loRaFrequency()        const override { return 868E6;   }
    int  loRaDefaultTxPower()   const override { return 22;      }
    int  loRaMaxTxPower()       const override { return 22;      }
    bool loRaIsSX1262()         const override { return true;    }

    // Display present (ST7701S 480x480 RGB) — managed entirely by display driver
    bool hasDisplay()           const override { return true;    }
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

    // BOOT button (GPIO0, active LOW — not actively used on this board)
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
