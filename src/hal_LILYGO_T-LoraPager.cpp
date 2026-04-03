/*
 * hal_LILYGO_T-LoraPager.cpp
 *
 * Hardware abstraction layer for the LILYGO T-LoraPager (ESP32-S3 + SX1262).
 *
 * SPI bus coordination:
 *   - instance.begin() initializes the entire SPI bus and IO expander.
 *   - All RadioLib accesses are protected with instance.lockSPI() / unlockSPI()
 *     so that LoRa and display can share the bus.
 *   - LovyanGFX uses the same lock (use_lock=true in bus configuration).
 */

#ifdef LILYGO_T_LORA_PAGER

#include "hal.h"
#include "display_LILYGO_T-LoraPager.h"
#include "RadioLib.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "webFunctions.h"
#include "dutycycle.h"
#include "logging.h"

// LilyGoLib instance (defined in LilyGo_LoRa_Pager.cpp)
#include <LilyGoLib.h>
#include <SD.h>

// radio is defined by LilyGoLib (via ARDUINO_LILYGO_LORA_SX1262 in LilyGo_LoRa_Pager.cpp)

bool txFlag = false;
bool rxFlag = false;

// ─── SD card ──────────────────────────────────────────────────────────────

static bool sdMounted = false;

bool pagerSdAvailable() { return sdMounted; }

// Called from the main Arduino loop — synchronous write is safe here.
// A FreeRTOS task was tried previously but it held instance.lockSPI() across
// the full FAT32 open/write/close, which blocked display and input for hundreds
// of milliseconds and made the device appear to hang.
void pagerAddMessageToSD(const char* json, size_t len) {
    if (!sdMounted) return;
    instance.lockSPI();
    File f = SD.open("/messages.json", FILE_APPEND);
    if (f) {
        f.write((const uint8_t*)json, len);
        f.print('\n');
        f.close();
    }
    instance.unlockSPI();
}

// ─── Helper functions ────────────────────────────────────────────────────

static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "FAILED code %d", state);
    }
}

void setWiFiLED(bool value) {
    // T-LoraPager has no dedicated WiFi LED
    (void)value;
}

bool getKeyApMode() {
    // BOOT button (GPIO0) as AP mode switch
    return !digitalRead(PIN_AP_MODE_SWITCH);
}

// ─── Hardware init ───────────────────────────────────────────────────────

void initHal() {
    txFlag  = false;
    rxFlag  = false;

    pinMode(PIN_AP_MODE_SWITCH, INPUT);

    // Display + Keyboard init (calls instance.begin() internally,
    // which sets up SPI bus, XL9555, TCA8418 keyboard, and also tries installSD()).
    initDisplay();

    // SD state is determined by instance.begin() inside initDisplay()
    sdMounted = (SD.cardType() != CARD_NONE);
    logPrintf(LOG_INFO, "HAL", "%s", sdMounted ? "SD card mounted" : "SD no card detected");

    // Only initialize LoRa if frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    // LoRa init on already configured SPI bus, with SPI lock
    instance.lockSPI();

    radio.reset();
    delay(100);
    int beginState = radio.begin();
    if (beginState != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "radio.begin() failed (code %d)", beginState);
        loraReady = false;
        instance.unlockSPI();
        return;
    }
    printState(radio.setDio2AsRfSwitch(true));
    printState(radio.setSyncWord(settings.loraSyncWord));
    printState(radio.setFrequency(settings.loraFrequency));
    printState(radio.setOutputPower(settings.loraOutputPower));
    printState(radio.setBandwidth(settings.loraBandwidth));
    printState(radio.setCodingRate(settings.loraCodingRate));
    printState(radio.setSpreadingFactor(settings.loraSpreadingFactor));
    printState(radio.setPreambleLength(settings.loraPreambleLength));
    printState(radio.setCRC(true));
    printState(radio.setCurrentLimit(140));
    printState(radio.setRxBoostedGainMode(true));
    printState(radio.startReceive());

    instance.unlockSPI();

    loraReady = true;
    logPrintf(LOG_INFO, "LoRa", "SX1262 ready.");
}

// ─── Reception ───────────────────────────────────────────────────────────

bool checkReceive(Frame &f) {
    if (!loraReady) return false;
    instance.lockSPI();
    uint16_t irqFlags = radio.getIrqFlags();
    instance.unlockSPI();

    // Channel busy (header detected)
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (!rxFlag) { rxFlag = true;  statusTimer = 0; }
    } else {
        if (rxFlag)  { rxFlag = false; statusTimer = 0; }
    }

    // Transmit complete
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        instance.lockSPI();
        radio.startReceive();
        instance.unlockSPI();
        txFlag = false;
        statusTimer = 0;
    }

    // Data received
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        instance.lockSPI();
        uint8_t rxBuffer[256];
        size_t rxBufferLength = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxBufferLength);
        //Read RSSI/SNR BEFORE startReceive(), as startReceive() can reset the registers
        float rxRssi     = radio.getRSSI();
        float rxSnr      = radio.getSNR();
        float rxFrqError = radio.getFrequencyError();
        radio.startReceive();
        instance.unlockSPI();

        if (state == RADIOLIB_ERR_NONE) {
            f.importBinary(rxBuffer, rxBufferLength);
            f.tx        = false;
            f.timestamp = time(NULL);
            f.rssi      = rxRssi;
            f.snr       = rxSnr;
            f.frqError  = rxFrqError;
            f.port      = 0;
            return true;
        }
    }
    return false;
}

// ─── Transmit ────────────────────────────────────────────────────────────

void transmitFrame(Frame &f) {
    if (!loraReady) return;

    uint8_t txBuffer[255];
    size_t txBufferLength;

    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx        = true;
    f.port      = 0;
    f.timestamp = time(NULL);

    if (strlen(f.nodeCall) == 0) { return; }

    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty cycle check for public band (10% in 60s)
    if (isPublicBand(settings.loraFrequency)) {
        instance.lockSPI();
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txBufferLength) / 1000);
        instance.unlockSPI();
        if (!dutyCycleAllowed(toa)) {
            logPrintf(LOG_WARN, "LoRa", "Duty cycle limit reached, TX skipped.");
            return;
        }
    }

    txFlag = true;
    instance.lockSPI();
    radio.startTransmit(txBuffer, txBufferLength);
    instance.unlockSPI();

    f.monitorJSON();
    displayMonitorFrame(f);
}

#endif // LILYGO_T_LORA_PAGER
