#include <Arduino.h>
#include <LittleFS.h>

#include "settings.h"
#include "helperFunctions.h"
#include "frame.h"
#include "main.h"
#include "webFunctions.h"
#include "peer.h"
#include "config.h"
#include "routing.h"


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
    //Frame senden
    f.id = millis();
    f.timestamp = time(NULL);
    f.tx = true;

    //Nach Route suchen
    bool routing = false;
    char viaCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    getRoute(f.dstCall, viaCall, MAX_CALLSIGN_LENGTH + 1);    
    if (strlen(viaCall) > 0) { routing = true; }

    // Prüfen ob das geroutete Ziel per WiFi erreichbar ist
    bool routeViaWifi = false;
    if (routing) {
        for (size_t pi = 0; pi < peerList.size(); pi++) {
            if (strcmp(peerList[pi].nodeCall, viaCall) == 0 &&
                peerList[pi].port == 1 && peerList[pi].available) {
                routeViaWifi = true;
                break;
            }
        }
    }

    for (int port = 1; port >= 0; port--) {
        // LoRa komplett überspringen wenn geroutetes Ziel per WiFi erreichbar (WiFi = primär, HF = Fallback)
        if (port == 0 && routeViaWifi) continue;

        uint8_t availableNodeCount = 0;
        f.viaCall[0] = 0;
        f.retry = TX_RETRY;
        f.initRetry = TX_RETRY;
        f.syncFlag = false;
        f.port = port;
        //An alle Peers senden
        for (int i = 0; i < peerList.size(); i++) {
            if ((peerList[i].available) && (peerList[i].port == port)) {
                if ((routing == false) || (strcmp(peerList[i].nodeCall, viaCall) == 0)) {
                    availableNodeCount++;
                    f.port = peerList[i].port;
                    memcpy(f.viaCall, peerList[i].nodeCall, sizeof(f.viaCall));
                    if (txBuffer.size() == 0) {f.syncFlag = true;} else {f.syncFlag = false;}
                    txBuffer.push_back(f);
                }
            }
        }

        //Wenn keine Peers, Frame ohne Ziel und Retry senden (WiFi nur wenn Peers konfiguriert sind)
        if (availableNodeCount == 0 && !(port == 1 && udpPeers.empty())) {
            f.viaCall[0] = 0;
            f.retry = 1;
            f.initRetry = 1;
            f.syncFlag = false;
            f.port = port;
            txBuffer.push_back(f);
        }
    }

    //Message in Ringpuffer speichern
    strncpy(messages[messagesHead].srcCall, f.srcCall, MAX_CALLSIGN_LENGTH + 1);
    messages[messagesHead].id = f.id;
    messagesHead++;
    if (messagesHead >= MAX_STORED_MESSAGES_RAM) { messagesHead = 0; }                        

    //Message an Websocket senden & speichern
    char* jsonBuffer = (char*)malloc(2048);
    if (jsonBuffer == nullptr) {
        Serial.println("[OOM] sendFrame: malloc failed");
        return;
    }
    size_t len = f.messageJSON(jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);
    addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
    free(jsonBuffer);
    jsonBuffer = nullptr;
}

void sendMessage(const char* dst, const char* text, uint8_t messageType) {
    if (strlen(text) == 0) {return;}
    //Neuen Frame für alle Peers zusammenbauen
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = messageType;
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    safeUtf8Copy((char*)f.dstCall, (uint8_t*)dst, MAX_CALLSIGN_LENGTH);
    safeUtf8Copy((char*)f.message, (uint8_t*)text, sizeof(f.message));
    f.messageLength = strlen((char*)f.message);
    sendFrame(f);
}

void sendGroup(const char* dst, const char* text, uint8_t messageType) {
    if (strlen(text) == 0) {return;}
    //Neuen Frame für alle Peers zusammenbauen
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = messageType;
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    safeUtf8Copy((char*)f.dstGroup, (uint8_t*)dst, MAX_CALLSIGN_LENGTH);
    safeUtf8Copy((char*)f.message, (uint8_t*)text, sizeof(f.message));
    f.messageLength = strlen((char*)f.message);
    sendFrame(f);
}

// void addJSONtoFileTask(void * pvParameters) {
//     FileWriteParams* p = (FileWriteParams*) pvParameters;

//     // Warten, bis das Dateisystem frei ist (max 30 Sekunden warten)
//     if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
//         //Zeilen zählen
//         size_t lineCount = 0;
//         File countFile = LittleFS.open(p->fileName, "r");
//         if (countFile) {
//             while (countFile.available()) {
//                 if (countFile.read() == '\n') lineCount++;
//             }
//             countFile.close();
//         }
//         size_t linesToSkip = (lineCount >= p->maxLines) ? (lineCount - p->maxLines - 1) : 0;

//         File srcFile = LittleFS.open(p->fileName, "r");   
//         File dstFile = LittleFS.open("/temp.json", "w");   
//         //char lineBuffer[2048]; // Puffer für eine Zeile
//         char* lineBuffer = (char*)malloc(2048);
//         size_t currentLine = 0;
//         if (srcFile) {
//             while (srcFile.available()) {
//                 int len = srcFile.readBytesUntil('\n', lineBuffer, 2048);
//                 // Nur Zeilen kopieren, die nach dem Skip-Limit liegen
//                 if (currentLine >= linesToSkip) {
//                     dstFile.write((const uint8_t*)lineBuffer, len);
//                     dstFile.print("\n");
//                 }
//                 currentLine++;
//             }
//             srcFile.close();
//         }
//         free(lineBuffer);
//         lineBuffer = nullptr;    

//         if (p->content != nullptr && p->length > 0) {
//             dstFile.write((const uint8_t*)p->content, p->length);
//             dstFile.print("\n");
//         }
//         dstFile.close();

//         LittleFS.remove(p->fileName);
//         LittleFS.rename("/temp.json", p->fileName);

//         xSemaphoreGive(fsMutex); //freigeben
//     } 

//     free(p->content);
//     delete p;
//     vTaskDelete(NULL);
// }


void addJSONtoFileTask(void * pvParameters) {
    FileWriteParams* p = (FileWriteParams*) pvParameters;
    // Warten, bis das Dateisystem frei ist (max 30 Sekunden warten)
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        // Datei im Append-Modus öffnen ("a")
        // Falls die Datei nicht existiert, wird sie automatisch erstellt.
        File file = LittleFS.open(p->fileName, "a"); 
        if (file) {
            if (p->content != nullptr && p->length > 0) {
                // Den neuen Inhalt ans Ende schreiben
                file.write((const uint8_t*)p->content, p->length);
                file.print("\n"); // Neue Zeile für den nächsten Eintrag
            }
            file.close();
        } else {
            Serial.printf("Fehler: Konnte Datei %s nicht zum Anhängen öffnen!\n", p->fileName);
        }
        xSemaphoreGive(fsMutex); // Semaphore wieder freigeben
    } else {
        Serial.println("Fehler: fsMutex Timeout in addJSONtoFileTask");
    }
   // Speicherbereinigung
    if (p->content != nullptr) {
        free(p->content);
    }
    delete p;
    // Task beenden
    vTaskDelete(NULL);
}


void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines) {
    // Parameter für den Task vorbereiten
    FileWriteParams* p = new FileWriteParams();
    if (p == nullptr) {
        Serial.println("[OOM] addJSONtoFile: new FileWriteParams failed");
        return;
    }
    // 1. Inhalt kopieren
    p->content = (char*)malloc(length);
    if (p->content != nullptr) {
        memcpy(p->content, buffer, length);
        p->length = length;
    } else {
        Serial.println("[OOM] addJSONtoFile: malloc content failed");
        p->length = 0;
    }
    // 2. Dateinamen kopieren (DAS FEHLTE)
    strncpy(p->fileName, file, sizeof(p->fileName) - 1);
    p->fileName[sizeof(p->fileName) - 1] = '\0'; // Sicherstellen der Null-Terminierung
    p->maxLines = lines;
    xTaskCreate(
        addJSONtoFileTask, 
        "FileWriteTask", 
        8192,      
        p,         
        1,         
        NULL
    );
}


void trimFileTask(void * pvParameters) {
    FileWriteParams* p = (FileWriteParams*) pvParameters;
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(30000))) {
        // 1. Zeilen zählen
        size_t lineCount = 0;
        File countFile = LittleFS.open(p->fileName, "r");
        if (countFile) {
            while (countFile.available()) {
                if (countFile.read() == '\n') lineCount++;
            }
            countFile.close();
        }
        // Berechnen, wie viele Zeilen übersprungen werden müssen
        if (lineCount > p->maxLines) {
            size_t linesToSkip = lineCount - p->maxLines;
            
            File srcFile = LittleFS.open(p->fileName, "r");
            File dstFile = LittleFS.open("/temp_trim.json", "w");
            
            if (srcFile && dstFile) {
                char* lineBuffer = (char*)malloc(4096);
                if (lineBuffer == nullptr) {
                    srcFile.close();
                    dstFile.close();
                    xSemaphoreGive(fsMutex);
                    delete p;
                    vTaskDelete(NULL);
                    return;
                }

                size_t currentLine = 0;

                while (srcFile.available()) {
                    // Zeile lesen
                    int len = srcFile.readBytesUntil('\n', lineBuffer, 4096);

                    // Nur behalten, wenn wir über dem Skip-Limit sind
                    if (currentLine >= linesToSkip) {
                        dstFile.write((const uint8_t*)lineBuffer, len);
                        dstFile.print("\n");
                    }
                    currentLine++;
                }

                srcFile.close();
                dstFile.close();
                free(lineBuffer);
                lineBuffer = nullptr;

                // Alte Datei ersetzen
                LittleFS.remove(p->fileName);
                LittleFS.rename("/temp_trim.json", p->fileName);

            } else {
                // Close whichever file was successfully opened
                if (srcFile) srcFile.close();
                if (dstFile) dstFile.close();
            }
        }
        xSemaphoreGive(fsMutex);
    }

    delete p; // p->content ist hier nullptr, daher nur p löschen
    vTaskDelete(NULL);
}


void trimFile(const char* fileName, size_t maxLines) {
    FileWriteParams* p = new FileWriteParams();
    if (p == nullptr) {
        Serial.println("[OOM] trimFile: new FileWriteParams failed");
        return;
    }
    strncpy(p->fileName, fileName, sizeof(p->fileName) - 1);
    p->fileName[sizeof(p->fileName) - 1] = '\0'; // Null-Terminierung erzwingen
    p->maxLines = (uint16_t)maxLines;
    p->content = nullptr;
    p->length = 0;

    // Task starten (Priorität etwas niedriger, da es ein Hintergrundjob ist)
    xTaskCreate(trimFileTask, "trimFileTask", 8192, p, 1, NULL);
}




uint32_t getTOA(uint16_t payloadBytes) {
    uint8_t SF  = settings.loraSpreadingFactor; 
    uint32_t BW = settings.loraBandwidth * 1000; 
    uint8_t CR = (settings.loraCodingRate > 4) ? (settings.loraCodingRate - 4) : settings.loraCodingRate;
    if (BW == 0) return 0;
    bool DE = ( ( (1 << SF) * 1000 / BW ) > 16 ); 
    float Tsym = (float)(1 << SF) / (float)BW * 1000.0f;
    float Tpreamble = (settings.loraPreambleLength + 4.25f) * Tsym;
    float payloadBits = 8.0f * payloadBytes - 4.0f * SF + 28.0f + 16.0f; // +16 für CRC
    float bitsPerSymbol = 4.0f * (SF - (DE ? 2 : 0));
    float payloadSymbols = 8.0f + fmaxf(ceilf(payloadBits / bitsPerSymbol) * (CR + 4), 0.0f);
    return (uint32_t)roundf(Tpreamble + (payloadSymbols * Tsym));
}

uint32_t calculateAckTime() {
    uint32_t time = getTOA(10 + 2 * MAX_CALLSIGN_LENGTH); // Time for 1 ACK frame
    time = time * 20;   // 20 ACK frames
    time = random(0, time);
    return time;
}

uint32_t calculateRetryTime() {
    uint32_t time = 20 * getTOA(10 + 2 * MAX_CALLSIGN_LENGTH); // Wait for up to 20 ACK frames
    time = time + random(0, 5 * getTOA(255));  // 0...5 max-length message frames
    return time;
}


static inline bool isCont(uint8_t b) {
    return (b & 0xC0) == 0x80; // 0b10xxxxxx
}

void safeUtf8Copy(char* dest, const uint8_t* src, size_t maxLength) {
    size_t d = 0;

    for (size_t i = 0; i < maxLength; ) {
        uint8_t b = src[i];

        if (b == 0x00) break;

        // ASCII
        if (b < 0x80) {
            // JSON: nur erlaubte ASCII-Zeichen
            if (b >= 0x20 || b == '\n' || b == '\r' || b == '\t') {
                dest[d++] = b;
            }
            i++;
            continue;
        }

        // 2-Byte UTF-8
        if (b >= 0xC2 && b <= 0xDF) {
            if (i + 1 < maxLength && isCont(src[i+1])) {
                dest[d++] = b;
                dest[d++] = src[i+1];
            }
            i += 2;
            continue;
        }

        // 3-Byte UTF-8
        if (b >= 0xE0 && b <= 0xEF) {
            if (i + 2 < maxLength &&
                isCont(src[i+1]) &&
                isCont(src[i+2])) {

                dest[d++] = b;
                dest[d++] = src[i+1];
                dest[d++] = src[i+2];
            }
            i += 3;
            continue;
        }

        // 4-Byte UTF-8
        if (b >= 0xF0 && b <= 0xF4) {
            if (i + 3 < maxLength &&
                isCont(src[i+1]) &&
                isCont(src[i+2]) &&
                isCont(src[i+3])) {

                dest[d++] = b;
                dest[d++] = src[i+1];
                dest[d++] = src[i+2];
                dest[d++] = src[i+3];
            }
            i += 4;
            continue;
        }

        // Alles andere verwerfen
        i++;
    }

    dest[d] = '\0';
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


