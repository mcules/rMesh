#include <SPI.h>
#include "hal/hal.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "hal/dutycycle.h"
#include "util/logging.h"

// ── LoRa Radio (SX1262 on dedicated SPI bus) ────────────────────────────────

// LoRa uses dedicated SPIM3 (default SPI/SPIM2 is reserved for E-Paper display)
SPIClass loraSPI(NRF_SPIM3, LORA_MISO, LORA_SCK, LORA_MOSI);

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);

bool txFlag = false;
bool rxFlag = false;

static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state);
    }
}

void setWiFiLED(bool value) {
    if (!board->hasWiFiLED()) return;
    digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? !value : value);
}

bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

float getBatteryVoltage() {
    if (!board->hasBatteryMonitor()) return 0.0f;
    analogReference(AR_INTERNAL_3_0);   // 3.0V reference
    analogReadResolution(12);           // 12-bit ADC
    delay(1);
    int raw = analogRead(board->pinBatteryADC());
    // Convert: ADC value -> voltage with divider compensation
    return (raw / 4095.0f) * 3.0f * board->batteryVoltageMultiplier();
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    // LED outputs (WiFi LED = green LED on T-Echo)
    if (board->hasWiFiLED()) {
        pinMode(board->pinWiFiLED(), OUTPUT);
        digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW);
    }
    pinMode(PIN_LED_BLUE, OUTPUT);
    digitalWrite(PIN_LED_BLUE, LOW);

    // Button input (active low with internal pull-up)
    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT_PULLUP);
    }

    // Start LoRa SPI bus (SPIM3, separate from display on SPIM2)
    loraSPI.begin();

    // Check if LoRa frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "No frequency configured - RF disabled.");
        loraReady = false;
        return;
    }

    // Initialize SX1262 with TCXO voltage (1.8V) included so RadioLib
    // calibrates the image rejection filter for the correct frequency band.
    int8_t txPower = settings.loraOutputPower;
    if (txPower > 22) txPower = 22;  // SX1262 max is +22 dBm

    int state = radio.begin(
        settings.loraFrequency,
        settings.loraBandwidth,
        settings.loraSpreadingFactor,
        settings.loraCodingRate,
        settings.loraSyncWord,
        txPower,
        settings.loraPreambleLength,
        1.8,    // TCXO voltage
        false   // use DC-DC regulator (not LDO)
    );
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "SX1262 init failed (code %d)", state);
        loraReady = false;
        return;
    }

    printState(radio.setDio2AsRfSwitch(true));
    printState(radio.setCRC(true));
    printState(radio.setCurrentLimit(140));
    printState(radio.setRxBoostedGainMode(true));

    // Start continuous receive
    printState(radio.startReceive());
    loraReady = true;
    logPrintf(LOG_INFO, "LoRa", "Ready: %.3f MHz SF%d BW%.1f",
              settings.loraFrequency, settings.loraSpreadingFactor,
              settings.loraBandwidth);

    // Brief LED flash to confirm radio init
    if (board->hasWiFiLED()) {
        digitalWrite(board->pinWiFiLED(), HIGH);
        delay(100);
        digitalWrite(board->pinWiFiLED(), LOW);
    }
}


bool checkReceive(Frame &f) {
    if (!loraReady) return false;

    uint16_t irqFlags = radio.getIrqFlags();

    // Track channel activity
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (!rxFlag) { rxFlag = true; statusTimer = 0; }
    } else {
        if (rxFlag) { rxFlag = false; statusTimer = 0; }
    }

    // TX complete -> switch back to RX
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        radio.startReceive();
        txFlag = false;
        statusTimer = 0;
    }

    // Data received
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        uint8_t rxBuffer[256];
        size_t rxBufferLength = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxBufferLength);
        //Read RSSI/SNR BEFORE startReceive(), as startReceive() can reset the registers
        float rxRssi = radio.getRSSI();
        float rxSnr = radio.getSNR();
        float rxFrqError = radio.getFrequencyError();
        radio.startReceive();  // Always restart RX immediately

        if (state == RADIOLIB_ERR_NONE) {
            f.importBinary(rxBuffer, rxBufferLength);
            f.tx = false;
            f.timestamp = time(NULL);
            f.rssi = rxRssi;
            f.snr = rxSnr;
            f.frqError = rxFrqError;
            f.port = 0;
            return true;
        }
    }
    return false;
}


void transmitFrame(Frame &f) {
    if (!loraReady) return;

    uint8_t txBuffer[255];
    size_t txBufferLength;

    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.timestamp = time(NULL);
    f.port = 0;

    if (strlen(f.nodeCall) == 0) return;
    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty cycle check for public band
    if (isPublicBand(settings.loraFrequency)) {
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txBufferLength) / 1000);
        if (!dutyCycleAllowed(toa)) {
            logPrintf(LOG_WARN, "LoRa", "Duty cycle limit reached, TX skipped.");
            return;
        }
    }

    txFlag = true;
    radio.startTransmit(txBuffer, txBufferLength);

    // Log to monitor
    f.monitorJSON();
}
