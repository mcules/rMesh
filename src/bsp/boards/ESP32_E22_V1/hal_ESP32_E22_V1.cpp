#include "hal/hal.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "network/webFunctions.h"
#include "hal/dutycycle.h"
#include "util/logging.h"



SX1268 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
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


void initHal() {
    txFlag = false;
    rxFlag = false;

    //SPI init
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);

    //Outputs
    if (board->hasWiFiLED()) {
        pinMode(board->pinWiFiLED(), OUTPUT);
        digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW);
    }
    pinMode(LORA_TX_ENA, OUTPUT);
    digitalWrite(LORA_TX_ENA, 0);
    pinMode(LORA_RX_ENA, OUTPUT);
    digitalWrite(LORA_RX_ENA, 1);

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
//    printState(radio.setDio2AsRfSwitch(true));
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

    //Register test PEER
    //Peer p;
    //p.lastRX = 0xFFFFFFFF;
    //strncpy(p.call, "DB0LUS", sizeof(p.call)-1);  //DB0LUS in p.call
    //p.available = true;
    //peerList.push_back(p);

}



bool checkReceive(Frame &f) {
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

        digitalWrite(LORA_TX_ENA, 0);
        digitalWrite(LORA_RX_ENA, 1);

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
    uint8_t txBuffer[255];
    size_t txBufferLength;

    digitalWrite(LORA_RX_ENA, 0); 
    digitalWrite(LORA_TX_ENA, 1); 
 
    //Populate frame
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.port = 0;
    f.timestamp = time(NULL);

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
