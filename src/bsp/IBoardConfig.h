#pragma once

/**
 * @file IBoardConfig.h
 * @brief Abstract board configuration interface.
 *
 * Every board support file must implement all methods.
 * Callers query capabilities only — never board identity.
 * The only place that checks board identity is BoardFactory.cpp.
 */
class IBoardConfig {
public:
    virtual ~IBoardConfig() = default;

    // ── LoRa ─────────────────────────────────────────────────────────────────
    virtual bool hasLoRa()              const = 0;
    virtual int  pinLoRaSCK()           const = 0;
    virtual int  pinLoRaMISO()          const = 0;
    virtual int  pinLoRaMOSI()          const = 0;
    virtual int  pinLoRaCS()            const = 0;  // NSS
    virtual int  pinLoRaRST()           const = 0;
    virtual int  pinLoRaIRQ()           const = 0;  // DIO1
    virtual int  pinLoRaBusy()          const = 0;  // -1 if not present (SX1276/SX1278)
    virtual long loRaFrequency()        const = 0;
    virtual int  loRaDefaultTxPower()   const = 0;
    virtual int  loRaMaxTxPower()       const = 0;
    virtual bool loRaIsSX1262()         const = 0;  // false = SX1276/SX1278

    // ── Display ──────────────────────────────────────────────────────────────
    virtual bool hasDisplay()           const = 0;
    virtual int  pinSDA()               const = 0;  // -1 if no display
    virtual int  pinSCL()               const = 0;
    virtual int  pinDisplayRST()        const = 0;  // -1 if no dedicated reset
    virtual int  pinVext()              const = 0;  // -1 if no Vext power control
    virtual bool vextActiveLow()        const = 0;  // true = LOW enables power

    // ── Battery ──────────────────────────────────────────────────────────────
    virtual bool  hasBatteryMonitor()           const = 0;
    virtual int   pinBatteryADC()               const = 0;
    virtual int   pinBatteryADCEnable()         const = 0;  // -1 if always on
    virtual bool  batteryADCEnableActiveLow()   const = 0;
    virtual float batteryVoltageMultiplier()    const = 0;

    // ── WiFi Status LED ───────────────────────────────────────────────────────
    virtual bool hasWiFiLED()           const = 0;
    virtual int  pinWiFiLED()           const = 0;
    virtual bool wiFiLEDActiveLow()     const = 0;

    // ── User Button ───────────────────────────────────────────────────────────
    virtual bool hasUserButton()        const = 0;
    virtual int  pinUserButton()        const = 0;

    // ── Ethernet ─────────────────────────────────────────────────────────────
    virtual bool hasEthernet()          const = 0;

    // ── TFT Display (ST7735 / ST7789) ────────────────────────────────────────
    virtual int  pinTFT_MOSI()         const = 0;  // -1 if no TFT
    virtual int  pinTFT_SCK()          const = 0;
    virtual int  pinTFT_DC()           const = 0;
    virtual int  pinTFT_CS()           const = 0;
    virtual int  pinTFT_RST()          const = 0;
    virtual int  pinTFT_BL()           const = 0;  // backlight PWM

    // ── E-Paper Display (GxEPD2) ─────────────────────────────────────────────
    virtual int  pinEPaper_MOSI()      const = 0;  // -1 if no E-Paper
    virtual int  pinEPaper_SCK()       const = 0;
    virtual int  pinEPaper_CS()        const = 0;
    virtual int  pinEPaper_DC()        const = 0;
    virtual int  pinEPaper_RES()       const = 0;
    virtual int  pinEPaper_BUSY()      const = 0;
    virtual int  pinEPaper_BL()        const = 0;  // -1 if no backlight
};
