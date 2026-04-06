/*
 * hal_SEEED_SenseCAP_Indicator.cpp
 *
 * HAL for the Seeed Studio SenseCAP Indicator D1L (ESP32-S3R8 + SX1262).
 *
 * SX1262 connection:
 *   SPI data lines:  SCK=GPIO41, MOSI=GPIO48, MISO=GPIO47  (hardware SPI FSPI)
 *   Control pins via PCA9535 I2C expander (0x20, port 0):
 *     Bit 0 = CS   (output, active low)
 *     Bit 1 = RST  (output, active low)
 *     Bit 2 = BUSY (input, HIGH = chip busy)
 *     Bit 3 = DIO1 (input, IRQ signal from SX1262)
 *
 * Custom RadioLib HAL routes digitalRead/Write for virtual pins 200-203
 * through the PCA9535.
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
#include "logging.h"

// ─── Global flags ────────────────────────────────────────────────────────────
bool txFlag = false;
bool rxFlag = false;

// ─── SPI instance for SX1262 ─────────────────────────────────────────────────
static SPIClass loraSPI(FSPI);

// ─── Custom RadioLib HAL: routes virtual pins 200-203 through PCA9535 ────────
class SenseCAPLoRaHal : public ArduinoHal {
public:
    SenseCAPLoRaHal() : ArduinoHal(loraSPI) {}

    // Returns the PCA9535 bit index for a virtual pin (-1 = not a vPin)
    static int vpinToBit(uint32_t pin) {
        switch (pin) {
            case LORA_NSS:  return 0;   // CS
            case LORA_RST:  return 1;   // RST
            case LORA_BUSY: return 2;   // BUSY (input)
            case LORA_DIO1: return 3;   // DIO1 (input)
            default:        return -1;
        }
    }

    void spiBegin() override {
        loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
    }

    void pinMode(uint32_t pin, uint32_t mode) override {
        if (vpinToBit(pin) >= 0) return;   // Direction via PCA9535, not GPIO
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

    // PCA9535 pins cannot trigger hardware interrupts
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

// ─── Helper function for error output ────────────────────────────────────────
static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "FAILED code %d", state);
    }
}

// ─── HAL implementation ──────────────────────────────────────────────────────

void setWiFiLED(bool value) {
    (void)value;  // no dedicated WiFi LED
}

bool getKeyApMode() {
    return false;  // GPIO0 is pulled LOW by RP2040 -> always false
}

void initHal() {
    // Only initialize display once (Bus_RGB cannot handle lcd.init() a 2nd time)
    static bool displayInited = false;
    if (!displayInited) {
        displayInited = true;
        initDisplay();
    }

    txFlag = false;
    rxFlag = false;

    // Reconfigure LoRa on every settings change
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    // PCA9535: configure LoRa pins as outputs/inputs (supplements LCD pins)
    // Bit0=LORA_CS(out), Bit1=LORA_RST(out), Bit2=LORA_BUSY(in), Bit3=LORA_DIO1(in)
    static bool pcaLoraInited = false;
    if (!pcaLoraInited) {
        pcaLoraInited = true;
        const uint8_t outputs = PCA9535_LCD_CS_BIT | PCA9535_LCD_RST_BIT |
                                PCA9535_LORA_CS_BIT | PCA9535_LORA_RST_BIT |
                                PCA9535_TP_RST_BIT;  // TP_RST stays HIGH (no re-reset)
        Wire.beginTransmission(PCA9535_ADDR);
        Wire.write(0x06);
        Wire.write(~outputs);
        Wire.endTransmission();
        // LORA_CS and LORA_RST high (inactive)
        pca9535_write_bit(0, true);
        pca9535_write_bit(1, true);
    }

    // Only initialize SPI pins when LoRa is actually needed
    // (loraSPI.begin reconfigures GPIO41/48 which LovyanGFX uses for display init)
    static bool spiInited = false;
    if (!spiInited) {
        spiInited = true;
        loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
    }

    radio.reset();
    delay(100);

    int beginState = radio.begin();
    if (beginState != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "radio.begin() failed (code %d)", beginState);
        loraReady = false;
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

    loraReady = true;
    logPrintf(LOG_INFO, "LoRa", "SenseCAP Indicator D1L – SX1262 ready.");
}

// ─── Reception (polling via IRQ flags) ───────────────────────────────────────
bool checkReceive(Frame &f) {
    if (!loraReady) return false;

    uint16_t irqFlags = radio.getIrqFlags();

    // RX busy flag for status display
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (!rxFlag) { rxFlag = true;  statusTimer = 0; }
    } else {
        if (rxFlag)  { rxFlag = false; statusTimer = 0; }
    }

    // TX complete -> back to RX
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        radio.startReceive();
        txFlag = false;
        statusTimer = 0;
    }

    // Packet received
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        uint8_t rxBuffer[256];
        size_t  rxLen = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxLen);
        //Read RSSI/SNR BEFORE startReceive(), as startReceive() can reset the registers
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

// ─── Transmit ────────────────────────────────────────────────────────────────
void transmitFrame(Frame &f) {
    if (!loraReady) { logPrintf(LOG_WARN, "LoRa", "loraReady=false, TX aborted"); return; }

    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx        = true;
    f.timestamp = time(NULL);
    f.port      = 0;

    if (strlen(f.nodeCall) == 0) {
        logPrintf(LOG_WARN, "LoRa", "nodeCall empty (mycall not set), TX aborted");
        return;
    }

    uint8_t txBuf[255];
    size_t  txLen = f.exportBinary(txBuf, sizeof(txBuf));

    logPrintf(LOG_INFO, "LoRa", "Sending %s->%s type=%d len=%d freq=%.3f",
              f.nodeCall, f.dstCall, f.frameType, (int)txLen, settings.loraFrequency);

    // Duty cycle check for public band (10% / 60s)
    if (isPublicBand(settings.loraFrequency)) {
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txLen) / 1000);
        if (!dutyCycleAllowed(toa)) {
            logPrintf(LOG_WARN, "LoRa", "Duty cycle limit reached, TX skipped.");
            return;
        }
    }

    txFlag = true;
    int16_t state = radio.startTransmit(txBuf, txLen);
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "startTransmit ERROR: %d", state);
        txFlag = false;
    } else {
        logPrintf(LOG_INFO, "LoRa", "startTransmit OK");
    }
    f.monitorJSON();
}

#endif // SEEED_SENSECAP_INDICATOR
