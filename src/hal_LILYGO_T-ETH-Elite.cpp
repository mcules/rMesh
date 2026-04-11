#ifdef LILYGO_T_ETH_ELITE

#include "hal.h"
#include "RadioLib.h"
#include "settings.h"
#include "frame.h"
#include "main.h"
#include "helperFunctions.h"
#include "webFunctions.h"
#include "dutycycle.h"
#include "logging.h"


SPIClass loraSPI(HSPI);
SX1262 radio = new Module( LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI );


#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

bool txFlag = false;
bool rxFlag = false;

void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) { logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state); }
}

void setWiFiLED(bool value) {
    // T-ETH-Elite has no user LED – ignore
    (void)value;
}

bool getKeyApMode() {
    return !digitalRead(PIN_AP_MODE_SWITCH);
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    // Inputs
    pinMode(PIN_AP_MODE_SWITCH, INPUT_PULLUP);

    // Initialise the dedicated LoRa SPI bus (separate from ETH/SD SPI)
    loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    // Only initialize the radio if a frequency has been configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "No frequency configured – radio disabled.");
        loraReady = false;
        return;
    }

    radio.reset();
    delay(100);
    int beginState = radio.begin();
    if (beginState != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "radio.begin() failed (code %d) – check wiring!", beginState);
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
        //Read RSSI/SNR BEFORE startReceive(), as startReceive() can reset the registers
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
            logPrintf(LOG_WARN, "LoRa", "Duty-cycle limit reached, TX skipped.");
            return;
        }
    }

    txFlag = 1;
    radio.startTransmit(txBuffer, txBufferLength);
    f.monitorJSON();
}


#endif
