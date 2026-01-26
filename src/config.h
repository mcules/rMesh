
//Timing
#define ANNOUNCE_TIME 5 * 60 * 1000 + random(0, 1 * 60 * 1000)  //ANNOUNCE Baken
#define PEER_TIMEOUT 30 * 60 * 1000              //Zeit, nach dem ein Call aus der Peer-Liste gelöscht wird
#define ACK_TIME random(0, 4000)                 //Zeit, bis ein ACK gesendet wird
#define TX_RETRY 10                              //Retrys beim Senden 
#define TX_RETRY_TIME 4000 + random(0, 4000)     //Pause zwischen wiederholungen (muss größer als ACK_TIME sein)
#define MAX_STORED_MESSAGES 1000                  //max. in "messages.json" gespeicherte Nachrichten
#define MAX_STORED_ACK 100                       //max. ACK Frames in "ack.json"
//#define REPEAT_WITHOUT_PEER                      //Wenn ein Node keine Peers hat, wird die Nachricht trotzdem wiederholt


//UDP Timing
#define UDP_TX_RETRY_TIME 2000

//Interner Quatsch
#define NAME "rMesh"                             //Versions-String
#define VERSION "V1.0.1-a"                       //Versions-String
#define MAX_CALLSIGN_LENGTH 9                    //maximale Länge des Rufzeichens  1....16
#define TX_BUFFER_SIZE 50
#define PEER_LIST_SIZE 20
#define UDP_PORT 3333
