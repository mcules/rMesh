/*
 * hal_LILYGO_T-LoraPager.cpp
 *
 * Hardware-Abstraktionsschicht für den LILYGO T-LoraPager (ESP32-S3 + SX1262).
 *
 * SPI-Bus-Koordination:
 *   - instance.begin() initialisiert den gesamten SPI-Bus und IO-Expander.
 *   - Alle RadioLib-Zugriffe werden mit instance.lockSPI() / unlockSPI()
 *     gesichert, damit sich LoRa und Display den Bus teilen können.
 *   - LovyanGFX nutzt denselben Lock (use_lock=true in Bus-Konfiguration).
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

// LilyGoLib-Instanz (definiert in LilyGo_LoRa_Pager.cpp)
#include <LilyGoLib.h>
#include <SD.h>

// radio wird von LilyGoLib definiert (via ARDUINO_LILYGO_LORA_SX1262 in LilyGo_LoRa_Pager.cpp)

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

// ─── Hilfsfunktionen ─────────────────────────────────────────────────────

static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] FAILED code %d\n", state);
    }
}

void setWiFiLED(bool value) {
    // T-LoraPager hat keine dedizierte WiFi-LED
    (void)value;
}

bool getKeyApMode() {
    // BOOT-Taste (GPIO0) als AP-Mode-Switch
    return !digitalRead(PIN_AP_MODE_SWITCH);
}

// ─── Hardware-Init ────────────────────────────────────────────────────────

void initHal() {
    txFlag  = false;
    rxFlag  = false;

    pinMode(PIN_AP_MODE_SWITCH, INPUT);

    // Display + Keyboard init (calls instance.begin() internally,
    // which sets up SPI bus, XL9555, TCA8418 keyboard, and also tries installSD()).
    initDisplay();

    // SD state is determined by instance.begin() inside initDisplay()
    sdMounted = (SD.cardType() != CARD_NONE);
    Serial.printf("[SD] %s\n", sdMounted ? "Card mounted" : "No card detected");

    // LoRa-Init nur wenn Frequenz konfiguriert ist
    if (!loraConfigured(settings.loraFrequency)) {
        Serial.println("[LoRa] Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    // LoRa-Init auf bereits konfiguriertem SPI-Bus, mit SPI-Lock
    instance.lockSPI();

    radio.reset();
    delay(100);
    printState(radio.begin());
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
    Serial.println("[LoRa] SX1262 ready.");
}

// ─── Empfang ──────────────────────────────────────────────────────────────

bool checkReceive(Frame &f) {
    if (!loraReady) return false;
    instance.lockSPI();
    uint16_t irqFlags = radio.getIrqFlags();
    instance.unlockSPI();

    // Kanal belegt (Header erkannt)
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (!rxFlag) { rxFlag = true;  statusTimer = 0; }
    } else {
        if (rxFlag)  { rxFlag = false; statusTimer = 0; }
    }

    // Senden abgeschlossen
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        instance.lockSPI();
        radio.startReceive();
        instance.unlockSPI();
        txFlag = false;
        statusTimer = 0;
    }

    // Daten empfangen
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        instance.lockSPI();
        uint8_t rxBuffer[256];
        size_t rxBufferLength = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxBufferLength);
        //RSSI/SNR VOR startReceive() lesen, da startReceive() die Register zurücksetzen kann
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

// ─── Senden ───────────────────────────────────────────────────────────────

void transmitFrame(Frame &f) {
    if (!loraReady) return;

    uint8_t txBuffer[255];
    size_t txBufferLength;

    txFlag = true;
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx        = true;
    f.port      = 0;
    f.timestamp = time(NULL);

    if (strlen(f.nodeCall) == 0) { return; }

    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty-Cycle-Check für Public-Band (10 % in 60 s)
    if (isPublicBand(settings.loraFrequency)) {
        instance.lockSPI();
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txBufferLength) / 1000);
        instance.unlockSPI();
        if (!dutyCycleAllowed(toa)) {
            Serial.println("[LoRa] Duty-Cycle-Limit erreicht, TX übersprungen.");
            return;
        }
        dutyCycleTrackTx(toa);
    }

    instance.lockSPI();
    radio.startTransmit(txBuffer, txBufferLength);
    instance.unlockSPI();

    f.monitorJSON();
    displayMonitorFrame(f);
}

#endif // LILYGO_T_LORA_PAGER
