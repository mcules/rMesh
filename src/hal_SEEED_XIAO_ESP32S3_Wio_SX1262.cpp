#ifdef SEEED_XIAO_ESP32S3_WIO_SX1262

#include "hal.h"
#include "RadioLib.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "webFunctions.h"
#include "dutycycle.h"


SX1262 radio = new Module( LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY );


#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

bool txFlag = false;
bool rxFlag = false;

void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) { Serial.printf("FAILED! code %d\n", state); }
}

void setWiFiLED(bool value) {
    digitalWrite(PIN_WIFI_LED, value);
}

bool getKeyApMode() {
    return !digitalRead(PIN_AP_MODE_SWITCH);
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    // Outputs
    pinMode(PIN_WIFI_LED, OUTPUT);
    digitalWrite(PIN_WIFI_LED, 0);
    pinMode(LORA_ANT_SW, OUTPUT);
    digitalWrite(LORA_ANT_SW, HIGH);  // enable antenna switch

    // Inputs
    pinMode(PIN_AP_MODE_SWITCH, INPUT_PULLUP);

    // Only initialize the radio if a frequency has been configured
    if (!loraConfigured(settings.loraFrequency)) {
        Serial.println("[LoRa] No frequency configured – radio disabled.");
        loraReady = false;
        return;
    }

    // Let RadioLib initialize the SPI bus with the board defaults (GPIO7/8/9).
    // Do NOT call SPI.begin() here – a second call after RadioLib's internal
    // SPI.begin() disrupts the bus configuration on the XIAO ESP32-S3.
    int beginState = radio.begin();
    if (beginState != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] radio.begin() failed (code %d) – check wiring!\n", beginState);
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

    // Start receiving
    printState(radio.startReceive());
    loraReady = true;
}


bool checkReceive(Frame &f) {
    if (!loraReady) return false;

    uint16_t irqFlags = radio.getIrqFlags();

    // Track channel-busy state for the status LED
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (rxFlag == false) { rxFlag = true;  statusTimer = 0; }
    } else {
        if (rxFlag == true)  { rxFlag = false; statusTimer = 0; }
    }

    // TX complete – switch back to RX
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        radio.startReceive();
        txFlag = false;
        statusTimer = 0;
    }

    // Packet received
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        uint8_t rxBuffer[256];
        size_t rxBufferLength = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxBufferLength);
        //RSSI/SNR VOR startReceive() lesen, da startReceive() die Register zurücksetzen kann
        float rxRssi = radio.getRSSI();
        float rxSnr = radio.getSNR();
        float rxFrqError = radio.getFrequencyError();
        radio.startReceive();
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

    txFlag = 1;
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.timestamp = time(NULL);
    f.port = 0;

    if (strlen(f.nodeCall) == 0) { return; }
    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty-cycle check for EU public band (max 10 % in 60 s)
    if (isPublicBand(settings.loraFrequency)) {
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txBufferLength) / 1000);
        if (!dutyCycleAllowed(toa)) {
            Serial.println("[LoRa] Duty-cycle limit reached, TX skipped.");
            return;
        }
        dutyCycleTrackTx(toa);
    }

    radio.startTransmit(txBuffer, txBufferLength);
    f.monitorJSON();
}


#endif
