#pragma once

// ── Frequenzband-Erkennung ────────────────────────────────────────────────────
// Amateurfunk 70-cm-Band (430–440 MHz), SyncWord 0x2B
#define AMATEUR_SYNCWORD       0x2B
// Public SRD 869 Sub-Band P (869,4–869,65 MHz), 500 mW, 10 % Duty Cycle
#define PUBLIC_SYNCWORD        0x12
#define PUBLIC_MAX_TX_POWER    22    // dBm (default für 868 MHz Public)

inline bool loraConfigured(float f) { return f > 1.0f; }
inline bool isAmateurBand(float f)  { return f >= 430.0f && f <= 440.0f; }
inline bool isPublicBand(float f)   { return f >= 869.4f && f <= 869.65f; }

// Liefert den korrekten SyncWord für die eingestellte Frequenz
inline uint8_t syncWordForFrequency(float f) {
    if (isPublicBand(f))  return PUBLIC_SYNCWORD;
    if (isAmateurBand(f)) return AMATEUR_SYNCWORD;
    return AMATEUR_SYNCWORD;  // Fallback
}

//Timing
#define ANNOUNCE_TIME 10 * 60 * 1000 + random(0, 2 * 60 * 1000)  //ANNOUNCE Baken
#define PEER_TIMEOUT 60 * 60 * 1000              //Zeit, nach dem ein Call aus der Peer-Liste gelöscht wird
#define TX_RETRY 10                              //Retrys beim Senden 
#define MAX_STORED_MESSAGES 1000                  //max. in "messages.json" gespeicherte Nachrichten
#define MAX_STORED_MESSAGES_RAM 100                  //max. in "messages.json" gespeicherte Nachrichten
#define MAX_STORED_ACK 100                       //max. ACK Frames in "ack.json"

//UDP Timing
#define UDP_TX_RETRY_TIME 2000

//Interner Quatsch
#define NAME "rMesh"                             //Versions-String
#include "version.h"
#define MAX_CALLSIGN_LENGTH 9                    //maximale Länge des Rufzeichens  1....15
#define TX_BUFFER_SIZE 50
#define PEER_LIST_SIZE 20
#define ROUTING_BUFFER_SIZE 100                  //50 Calls in Routing Liste
#define UDP_PORT 3333
