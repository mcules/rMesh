#include <Arduino.h>
#include <LittleFS.h>

#include "settings.h"
#include "helperFunctions.h"
#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"




void printHexArray(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0"); // Führende Null für Werte kleiner als 16
    }
    Serial.print(data[i], HEX); 
    Serial.print(" "); // Leerzeichen zur besseren Lesbarkeit
  }
  Serial.println(); // Zeilenumbruch am Ende
}



void sendFrame(Frame &f) {
    //Frame an alle Peers senden
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    f.id = millis();
    f.timestamp = time(NULL);
    f.tx = true;

    for (int port = 0; port <= 1; port++) {
        uint8_t availableNodeCount = 0;
        //An alle Peers senden
        bool first = true;
        if (txBuffer.size() > 0) {first = false;}
        for (int i = 0; i < peerList.size(); i++) {
            if ((peerList[i].available) && (peerList[i].port == port)) {
                availableNodeCount ++;
                memcpy(f.viaCall, peerList[i].nodeCall, sizeof(f.viaCall));
                f.retry = TX_RETRY;
                f.initRetry = TX_RETRY;
                f.syncFlag = first;
                f.port = peerList[i].port;
                txBuffer.push_back(f);
                first = false;
            }
        } 

        //Wenn keine Peers, Frame ohne Ziel und Retry senden
        #ifdef REPEAT_WITHOUT_PEER
            if (availableNodeCount == 0) {
                //Frame in Sendebuffer
                f.viaCall[0] = 0;
                f.retry = 1;
                f.initRetry = 1;
                f.syncFlag = false;            
                f.port = port;
                txBuffer.push_back(f);
            }
        #endif
    }

    //Message an Websocket senden & speichern
    char* jsonBuffer = (char*)malloc(2048);
    size_t len = f.messageJSON(jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);
    addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
    free(jsonBuffer);
    jsonBuffer = nullptr;
}

void sendMessage(const char* dstCall, const char* text, uint8_t messageType) {
    //Neuen Frame für alle Peers zusammenbauen
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = messageType;
    strncpy(f.dstCall, dstCall, sizeof(f.dstCall));
    strncpy((char*)f.message, text, sizeof(f.message));
    f.messageLength = strlen(text);
    sendFrame(f);
}

void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines) {
    //Zeilen zählen
    size_t lineCount = 0;
    File countFile = LittleFS.open(file, "r");
    if (countFile) {
        while (countFile.available()) {
            if (countFile.read() == '\n') lineCount++;
        }
        countFile.close();
    }
    size_t linesToSkip = (lineCount >= lines) ? (lineCount - lines - 1) : 0;

    File srcFile = LittleFS.open(file, "r");   
    File dstFile = LittleFS.open("/temp.json", "w");   
    //char lineBuffer[2048]; // Puffer für eine Zeile
    char* lineBuffer = (char*)malloc(2048);
    size_t currentLine = 0;
    if (srcFile) {
        while (srcFile.available()) {
            int len = srcFile.readBytesUntil('\n', lineBuffer, 2048);
            // Nur Zeilen kopieren, die nach dem Skip-Limit liegen
            if (currentLine >= linesToSkip) {
                dstFile.write((const uint8_t*)lineBuffer, len);
                dstFile.print("\n");
            }
            currentLine++;
        }
        srcFile.close();
    }
    free(lineBuffer);
    lineBuffer = nullptr;    

    if (buffer != nullptr && length > 0) {
        dstFile.write((const uint8_t*)buffer, length);
        dstFile.print("\n");
    }
    dstFile.close();

    LittleFS.remove(file);
    LittleFS.rename("/temp.json", file);
}

uint32_t getTOA(uint8_t payloadBytes) {
    //Parameter aus Settings holen
    uint8_t SF  = settings.loraSpreadingFactor;   // 7–12
    uint32_t BW = settings.loraBandwidth * 1000;  // kHz → Hz
    uint8_t CR  = settings.loraCodingRate;        // 1–4 (CR = 4/5 → 1, 4/6 → 2 ...)
    bool CRC    = true;         // true/false
    bool IH     = false;        // true/false
    uint16_t preamble = settings.loraPreambleLength;
    if (BW == 0) return 0;
    bool DE = (SF >= 11 && BW <= 125000);
    float Tsym = (float)(1 << SF) / (float)BW * 1000.0f;
    float Tpreamble = (preamble + 4.25f) * Tsym;
    float payloadBits =
        8.0f * payloadBytes
        - 4.0f * SF
        + 28.0f
        + (CRC ? 16.0f : 0.0f)
        - (IH ? 20.0f : 0.0f);
    float denominator = 4.0f * (SF - (DE ? 2 : 0));
    float payloadSymbols = 8.0f + fmaxf(ceilf(payloadBits / denominator) * (CR + 4), 0.0f);
    float total = Tpreamble + payloadSymbols * Tsym;
    return (uint32_t)roundf(total);
}




void safeUtf8Copy(char* dest, const uint8_t* src, size_t maxLength) {
    size_t d = 0; // Index für dest
    
    for (size_t i = 0; i < maxLength; i++) {
        uint8_t byte = src[i];

        // NEU: Abbrechen, wenn das Ende des C-Strings erreicht ist
        if (byte == 0x00) {
            break; 
        }

        // 1. Filter: 0xFF und 0xFE sind in UTF-8 niemals erlaubt
        if (byte >= 0xFE) {
            continue; // Byte überspringen
        }

        // 2. Prüfung der Byte-Sequenz
        if (byte <= 0x7F) {
            // Valides ASCII
            dest[d++] = (char)byte;
        } 
        else if (byte >= 0xC2 && byte <= 0xF4) {
            // Möglicher Start einer Mehrbyte-Sequenz
            dest[d++] = (char)byte;
        } 
        else if (byte >= 0x80 && byte <= 0xBF) {
            // Valides Folge-Byte
            dest[d++] = (char)byte;
        }
    }
    dest[d] = '\0'; // Null-Terminierung
}


void getFormattedTime(const char* format, char* outBuffer, size_t outSize) {
    time_t now = time(NULL);
    struct tm timeinfo;
    
    if (!localtime_r(&now, &timeinfo)) {
        snprintf(outBuffer, outSize, "Zeitfehler");
        return;
    }

    strftime(outBuffer, outSize, format, &timeinfo);
}


