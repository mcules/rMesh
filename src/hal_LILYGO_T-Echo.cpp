#ifdef LILYGO_T_ECHO

#include <SPI.h>
#include "hal.h"
#include "RadioLib.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "dutycycle.h"

// ── LoRa Radio (SX1262 on dedicated SPI bus) ────────────────────────────────

// LoRa uses dedicated SPIM3 (default SPI/SPIM2 is reserved for E-Paper display)
SPIClass loraSPI(NRF_SPIM3, LORA_MISO, LORA_SCK, LORA_MOSI);

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);

bool txFlag = false;
bool rxFlag = false;

static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("FAILED! code %d\n", state);
    }
}

void setWiFiLED(bool value) {
    // T-Echo has no WiFi LED; use green LED as general status indicator
    digitalWrite(PIN_LED_GREEN, value ? HIGH : LOW);
}

bool getKeyApMode() {
    return !digitalRead(PIN_BUTTON);
}

float getBatteryVoltage() {
    analogReference(AR_INTERNAL_3_0);   // 3.0V reference
    analogReadResolution(12);           // 12-bit ADC
    delay(1);
    int raw = analogRead(PIN_VBAT_ADC);
    // Convert: ADC value → voltage with divider compensation
    return (raw / 4095.0f) * 3.0f * VBAT_DIVIDER_COMP;
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    // LED outputs
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_BLUE, LOW);

    // Button input (active low with internal pull-up)
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // Start LoRa SPI bus (SPIM3, separate from display on SPIM2)
    loraSPI.begin();

    // Check if LoRa frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        Serial.println("[LoRa] No frequency configured - RF disabled.");
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
        Serial.printf("[LoRa] SX1262 init failed (code %d)\n", state);
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
    Serial.printf("[LoRa] Ready: %.3f MHz SF%d BW%.1f\n",
                  settings.loraFrequency, settings.loraSpreadingFactor,
                  settings.loraBandwidth);

    // Brief LED flash to confirm radio init
    digitalWrite(PIN_LED_GREEN, HIGH);
    delay(100);
    digitalWrite(PIN_LED_GREEN, LOW);
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

    // TX complete → switch back to RX
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
        //RSSI/SNR VOR startReceive() lesen, da startReceive() die Register zurücksetzen kann
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

    txFlag = true;
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
            Serial.println("[LoRa] Duty cycle limit reached, TX skipped.");
            return;
        }
        dutyCycleTrackTx(toa);
    }

    radio.startTransmit(txBuffer, txBufferLength);

    // Log to monitor
    f.monitorJSON();
}


#endif
