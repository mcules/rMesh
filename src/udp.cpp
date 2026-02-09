#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "udp.h"
#include "frame.h"
#include "settings.h"
#include "webFunctions.h"


WiFiUDP udp;

void initUDP() {
    udp.begin(UDP_PORT);
    esp_log_level_set("NetworkUdp", ESP_LOG_NONE);
    esp_log_level_set("NetworkUdp", ESP_LOG_ERROR);

}


bool checkUDP(Frame &f) {
    size_t packetSize = udp.parsePacket();
    if (packetSize) {
        uint8_t packetBuffer[255];
        int len = udp.read(packetBuffer, sizeof(packetBuffer));
        f.importBinary(packetBuffer, len);
        f.tx = false;
        f.timestamp = time(NULL);
        f.rssi = 0;
        f.snr = 100;
        f.frqError = 0;
        f.port = 1;
        return true;
    }
    return false;
}

void sendUDP(Frame &f) {
    uint8_t txBuffer[255];
    size_t txBufferLength;

    //Frame ergänzen
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.port = 1;
    f.timestamp = time(NULL);

    //Senden
    if (strlen(f.nodeCall) == 0) {return;}
    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));
    uint8_t count = sizeof(extSettings.udpPeer) / sizeof(extSettings.udpPeer[0]);
    bool udpTX = false;
    for (int i = 0; i < count; i++) {
        if ((extSettings.udpPeer[i][0] != 0) && (extSettings.udpPeer[i][1] != 0) && (extSettings.udpPeer[i][2] != 0) && (extSettings.udpPeer[i][3] != 0)){
            Serial.printf("UDP TX %i: %d.%d.%d.%d\n", i, extSettings.udpPeer[i][0], extSettings.udpPeer[i][1], extSettings.udpPeer[i][2], extSettings.udpPeer[i][3]);
            udp.beginPacket(extSettings.udpPeer[i], UDP_PORT);
            udp.write(txBuffer, txBufferLength);
            udp.endPacket();
            udp.flush();
            udpTX = true;
        }
    }
    //Frame monitoren
    if (udpTX) {
        char* jsonBuffer = (char*)malloc(2048); 
        size_t len = f.monitorJSON(jsonBuffer, 2048);
        ws.textAll(jsonBuffer, len);  
        free(jsonBuffer);
        jsonBuffer = nullptr;
    }
}
