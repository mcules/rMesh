#ifdef HELTEC_WIFI_LORA_32_V3

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
    if (state != RADIOLIB_ERR_NONE) {Serial.printf("FAILED! code %d\n", state);}
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

    //SPI Init
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    //Ausgäne
    pinMode(PIN_WIFI_LED, OUTPUT); 
    digitalWrite(PIN_WIFI_LED, 0); 

    //Eingänge
    pinMode(PIN_AP_MODE_SWITCH, INPUT);

    // HF-Modul nur initialisieren wenn Frequenz konfiguriert ist
    if (!loraConfigured(settings.loraFrequency)) {
        Serial.println("[LoRa] Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    //Flags zurücksetzen
    int state;

    //Init
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

    //RX einschalten
    printState(radio.startReceive());
    loraReady = true;

}



bool checkReceive(Frame &f) {
    if (!loraReady) return false;
    //IRQ-Flags auslesen
    uint16_t irqFlags = radio.getIrqFlags();
    //Prüfen ob Kanal belegt
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
    //Senden fertig
    if (irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) {
        radio.startReceive();
        txFlag = false;
        statusTimer = 0;
    }      
    //Daten Empfangen -> rxBuffer
    if (irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        //Prüfen, ob was empfangen wurde
        uint8_t rxBuffer[256];
        size_t rxBufferLength;
        rxBufferLength = radio.getPacketLength();
        int16_t state = radio.readData(rxBuffer, rxBufferLength);
        //Empfang wieder starten
        radio.startReceive();
        if (state == RADIOLIB_ERR_NONE) {    
            f.importBinary(rxBuffer, rxBufferLength);
            f.tx = false;
            f.timestamp = time(NULL);
            f.rssi = radio.getRSSI();
            f.snr = radio.getSNR();
            f.frqError = radio.getFrequencyError();
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

    //Frame ergänzen
    txFlag = 1;
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.timestamp = time(NULL);
    f.port = 0;

    //Senden
    if (strlen(f.nodeCall) == 0) {return;}
    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty-Cycle-Check für Public-Band (10 % in 60 s)
    if (isPublicBand(settings.loraFrequency)) {
        uint32_t toa = (uint32_t)(radio.getTimeOnAir(txBufferLength) / 1000);
        if (!dutyCycleAllowed(toa)) {
            Serial.println("[LoRa] Duty-Cycle-Limit erreicht, TX übersprungen.");
            return;
        }
        dutyCycleTrackTx(toa);
    }

    radio.startTransmit(txBuffer, txBufferLength);

    //Frame monitoren
    f.monitorJSON();

}


#endif