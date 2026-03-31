/*
 * hal_SEEED_SenseCAP_Indicator.cpp
 *
 * HAL für den Seeed Studio SenseCAP Indicator D1L (ESP32-S3R8 + SX1262).
 *
 * SX1262-Anbindung:
 *   SPI-Datenleitungen:  SCK=GPIO41, MOSI=GPIO48, MISO=GPIO47  (Hardware-SPI FSPI)
 *   Steuerpins via PCA9535 I2C-Expander (0x20, Port 0):
 *     Bit 0 = CS   (Ausgang, active low)
 *     Bit 1 = RST  (Ausgang, active low)
 *     Bit 2 = BUSY (Eingang, HIGH = chip beschäftigt)
 *     Bit 3 = DIO1 (Eingang, IRQ-Signal vom SX1262)
 *
 * Custom RadioLib-HAL leitet digitalRead/Write für virtuelle Pins 200-203
 * über den PCA9535 um.
 */

#ifdef SEEED_SENSECAP_INDICATOR

#include "hal.h"
#include "display_SEEED_SenseCAP_Indicator.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "dutycycle.h"

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>

// ─── Globale Flags ────────────────────────────────────────────────────────────
bool txFlag = false;
bool rxFlag = false;

// ─── SPI-Instanz für SX1262 ───────────────────────────────────────────────────
static SPIClass loraSPI(FSPI);

// ─── Custom RadioLib-HAL: routet virtuelle Pins 200-203 über PCA9535 ─────────
class SenseCAPLoRaHal : public ArduinoHal {
public:
    SenseCAPLoRaHal() : ArduinoHal(loraSPI) {}

    // Gibt den PCA9535-Bit-Index für einen virtuellen Pin zurück (-1 = kein vPin)
    static int vpinToBit(uint32_t pin) {
        switch (pin) {
            case LORA_NSS:  return 0;   // CS
            case LORA_RST:  return 1;   // RST
            case LORA_BUSY: return 2;   // BUSY (Eingang)
            case LORA_DIO1: return 3;   // DIO1 (Eingang)
            default:        return -1;
        }
    }

    void spiBegin() override {
        loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
    }

    void pinMode(uint32_t pin, uint32_t mode) override {
        if (vpinToBit(pin) >= 0) return;   // Richtung via PCA9535, nicht GPIO
        ArduinoHal::pinMode(pin, mode);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        int bit = vpinToBit(pin);
        if (bit >= 0) { pca9535_write_bit(bit, value != 0); return; }
        ArduinoHal::digitalWrite(pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        int bit = vpinToBit(pin);
        if (bit >= 0) return pca9535_read_bit(bit) ? 1 : 0;
        return ArduinoHal::digitalRead(pin);
    }

    // PCA9535-Pins können keine Hardware-Interrupts auslösen
    void attachInterrupt(uint32_t pin, void (*cb)(void), uint32_t mode) override {
        if (vpinToBit(pin) >= 0) return;
        ArduinoHal::attachInterrupt(pin, cb, mode);
    }
    void detachInterrupt(uint32_t pin) override {
        if (vpinToBit(pin) >= 0) return;
        ArduinoHal::detachInterrupt(pin);
    }
};

static SenseCAPLoRaHal sensecapHal;

// SX1262: cs=LORA_NSS(200), irq=LORA_DIO1(203), rst=LORA_RST(201), busy=LORA_BUSY(202)
SX1262 radio = new Module(&sensecapHal, LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ─── Hilfsfunktion Fehlerausgabe ──────────────────────────────────────────────
static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] FAILED code %d\n", state);
    }
}

// ─── HAL-Implementierung ──────────────────────────────────────────────────────

void setWiFiLED(bool value) {
    (void)value;  // kein dediziertes WiFi-LED
}

bool getKeyApMode() {
    return false;  // GPIO0 wird vom RP2040 auf LOW gezogen → immer false
}

void initHal() {
    // Display nur einmal initialisieren (Bus_RGB kann kein lcd.init() ein 2. Mal)
    static bool displayInited = false;
    if (!displayInited) {
        displayInited = true;
        initDisplay();
    }

    txFlag = false;
    rxFlag = false;

    // LoRa bei jeder Einstellungsänderung neu konfigurieren
    if (!loraConfigured(settings.loraFrequency)) {
        Serial.println("[LoRa] Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    // PCA9535: LoRa-Pins als Ausgänge/Eingänge konfigurieren (ergänzt LCD-Pins)
    // Bit0=LORA_CS(out), Bit1=LORA_RST(out), Bit2=LORA_BUSY(in), Bit3=LORA_DIO1(in)
    static bool pcaLoraInited = false;
    if (!pcaLoraInited) {
        pcaLoraInited = true;
        const uint8_t outputs = PCA9535_LCD_CS_BIT | PCA9535_LCD_RST_BIT |
                                PCA9535_LORA_CS_BIT | PCA9535_LORA_RST_BIT |
                                PCA9535_TP_RST_BIT;  // TP_RST bleibt HIGH (kein Re-Reset)
        Wire.beginTransmission(PCA9535_ADDR);
        Wire.write(0x06);
        Wire.write(~outputs);
        Wire.endTransmission();
        // LORA_CS und LORA_RST high (inaktiv)
        pca9535_write_bit(0, true);
        pca9535_write_bit(1, true);
    }

    // SPI-Pins erst initialisieren wenn LoRa wirklich benötigt wird
    // (loraSPI.begin rekonfiguriert GPIO41/48 die LovyanGFX für Display-Init nutzt)
    static bool spiInited = false;
    if (!spiInited) {
        spiInited = true;
        loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
    }

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

    loraReady = true;
    Serial.println("[LoRa] SenseCAP Indicator D1L – SX1262 bereit.");
}

// ─── Empfang (Polling über IRQ-Flags) ────────────────────────────────────────
bool checkReceive(Frame &f) {
    if (!loraReady) return false;

    uint16_t irqFlags = radio.getIrqFlags();

    // RX-Busy-Flag für Status-Anzeige
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (!rxFlag) { rxFlag = true;  statusTimer = 0; }
    } else {
        if (rxFlag)  { rxFlag = false; statusTimer = 0; }
    }

    // TX fertig → zurück in RX
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        radio.startReceive();
        txFlag = false;
        statusTimer = 0;
    }

    // Paket empfangen
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        uint8_t rxBuffer[256];
        size_t  rxLen = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxLen);
        //RSSI/SNR VOR startReceive() lesen, da startReceive() die Register zurücksetzen kann
        float rxRssi     = radio.getRSSI();
        float rxSnr      = radio.getSNR();
        float rxFrqError = radio.getFrequencyError();
        radio.startReceive();
        if (state == RADIOLIB_ERR_NONE) {
            f.importBinary(rxBuffer, rxLen);
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

// ─── Senden ───────────────────────────────────────────────────────────────────
void transmitFrame(Frame &f) {
    if (!loraReady) { Serial.println("[TX] loraReady=false, abgebrochen"); return; }

    txFlag = true;
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx        = true;
    f.timestamp = time(NULL);
    f.port      = 0;

    if (strlen(f.nodeCall) == 0) {
        Serial.println("[TX] nodeCall leer (mycall nicht gesetzt), abgebrochen");
        return;
    }

    uint8_t txBuf[255];
    size_t  txLen = f.exportBinary(txBuf, sizeof(txBuf));

    Serial.printf("[TX] Sende %s->%s type=%d len=%d freq=%.3f\n",
                  f.nodeCall, f.dstCall, f.frameType, (int)txLen, settings.loraFrequency);

    // Duty-Cycle-Check für Public-Band (10 % / 60 s)
    if (isPublicBand(settings.loraFrequency)) {
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txLen) / 1000);
        if (!dutyCycleAllowed(toa)) {
            Serial.println("[TX] Duty-Cycle-Limit erreicht, TX übersprungen.");
            txFlag = false;
            return;
        }
        dutyCycleTrackTx(toa);
    }

    int16_t state = radio.startTransmit(txBuf, txLen);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[TX] startTransmit FEHLER: %d\n", state);
        txFlag = false;
    } else {
        Serial.println("[TX] startTransmit OK");
    }
    f.monitorJSON();
}

#endif // SEEED_SENSECAP_INDICATOR
