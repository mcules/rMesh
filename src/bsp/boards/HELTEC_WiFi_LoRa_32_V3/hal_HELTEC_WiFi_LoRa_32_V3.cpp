#include "hal/hal.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "network/webFunctions.h"
#include "hal/dutycycle.h"
#include "util/logging.h"


SX1262 radio = new Module( LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY );


#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

bool txFlag = false;
bool rxFlag = false;

void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state);}
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
    if (!board->hasBatteryMonitor()) return -1.0f;
    analogSetPinAttenuation(board->pinBatteryADC(), ADC_11db);
    int enablePin = board->pinBatteryADCEnable();
    if (enablePin >= 0) digitalWrite(enablePin, board->batteryADCEnableActiveLow() ? LOW : HIGH);
    int sum = 0;
    for (int i = 0; i < 8; i++) sum += analogRead(board->pinBatteryADC());
    if (enablePin >= 0) digitalWrite(enablePin, board->batteryADCEnableActiveLow() ? HIGH : LOW);
    return (sum / 8.0f / 4095.0f) * 3.3f * board->batteryVoltageMultiplier();
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    //SPI init
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    //Outputs
    if (board->hasWiFiLED()) { pinMode(board->pinWiFiLED(), OUTPUT); digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW); }
    if (board->hasBatteryMonitor() && board->pinBatteryADCEnable() >= 0) { pinMode(board->pinBatteryADCEnable(), OUTPUT); digitalWrite(board->pinBatteryADCEnable(), board->batteryADCEnableActiveLow() ? HIGH : LOW); }

    //Inputs
    if (board->hasUserButton()) { pinMode(board->pinUserButton(), INPUT); }

    // Only initialize RF module if frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    //Reset flags
    int state;

    //Init
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

    //Enable RX
    printState(radio.startReceive());
    loraReady = true;

}



bool checkReceive(Frame &f) {
    if (!loraReady) return false;
    //Read IRQ flags
    uint16_t irqFlags = radio.getIrqFlags();
    //Check if channel is busy
    if (irqFlags & RADIOLIB_SX126X_IRQ_HEADER_VALID) {
        if (rxFlag == false) {
            rxFlag = true;
            statusTimer = 0;
        }
    } else {
        if (rxFlag == true) {
            rxFlag = false;
            statusTimer = 0;
        }
    }
    //Transmit complete
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        radio.startReceive();
        txFlag = false;
        statusTimer = 0;
    }
    //Data received -> rxBuffer
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        //Check if data was received
        uint8_t rxBuffer[256];
        size_t rxBufferLength;
        rxBufferLength = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxBufferLength);
        //Read RSSI/SNR BEFORE startReceive(), as startReceive() can reset the registers
        float rxRssi = radio.getRSSI();
        float rxSnr = radio.getSNR();
        float rxFrqError = radio.getFrequencyError();
        //Restart reception
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

    //Populate frame
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.timestamp = time(NULL);
    f.port = 0;

    //Transmit
    if (strlen(f.nodeCall) == 0) {return;}
    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty cycle check for public band (10% in 60s)
    if (isPublicBand(settings.loraFrequency)) {
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txBufferLength) / 1000);
        if (!dutyCycleAllowed(toa)) {
            logPrintf(LOG_WARN, "LoRa", "Duty cycle limit reached, TX skipped.");
            return;
        }
    }

    txFlag = 1;
    radio.startTransmit(txBuffer, txBufferLength);

    //Monitor frame
    f.monitorJSON();

}