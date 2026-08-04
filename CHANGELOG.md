# Changelog

## [v26.8.0-dev]

Erstes Dev-Release seit v26.4.1a-dev (April 2026). Bündelt die bisher nur als Nightly verfügbaren Zwischenstände **v26.4.2**, **v26.4.3** und **v26.7.0** (Details in den jeweiligen Sektionen weiter unten) plus alles Neue seither.

### Highlights der gebündelten Zwischenstände

- **v26.4.2**: Board Support Package (BSP) mit `IBoardConfig`-Interface statt `#ifdef`-Wildwuchs, Display-Treiber nach Display-Typ konsolidiert, Source-Tree komplett reorganisiert
- **v26.4.3**: LittleFS-voll-Deadlock behoben, Loop-Task-Watchdog, NVS-Self-Heal, 8-MB-Partitionstabelle für Heltec-Boards, OTA-Größen-Guards (Brick-Schutz), Nachrichten-Limit nach realer Partitionsgröße
- **v26.7.0**: Ergebnis dreier tiefer Code-Review-Durchgänge — Mesh-/Stabilitäts-/Power-Hardening, abwärtskompatibler LoRa-Broadcast-Relay (`loraFloodSingle`), TX-Watchdog, Boot-Loop-Schutz/Safe-Mode, kritische OTA-Auth- und Data-Race-Fixes

### NEU

- NEU: Native Unit-Test-Suite (`pio test -e native`) mit GitHub-Actions-CI und AddressSanitizer — reine LoRa-Mathematik (Time-on-Air, Duty-Cycle), Frame-Serialisierung und CLI-Parser laufen als Host-Tests bei jedem Push
- NEU: Hardware-in-the-Loop-Testsuite ausgebaut (pytest, echte Nodes via USB/Serial) — exakte Assertions, Regressionstests, robuster UDP-/WiFi-Transport-Test mit sauberem Skip bei AP-Client-Isolation

### GEÄNDERT

- GEÄNDERT: Tune sendet jetzt einen echten Dauerstrich-Träger (CW) von 5 Sekunden statt eines einzelnen 0xFF-Pakets über die normale TX-Warteschlange (#54) — der Träger startet sofort beim Klick (SX126x: `SetTxContinuousWave`; SX127x: FSK mit Hub 0), danach wird das Radio neu initialisiert und empfängt wieder. Auf dem 869,4-MHz-Band wird der Träger aufs Duty-Cycle-Budget angerechnet. Neuer Serial-Befehl `tune`

### FIX

- FIX: Announce-ACKs der Gegenstationen wurden im Monitor nie angezeigt (#55) — ein Announce erzeugt TX-Monitor-Frames auf WiFi UND LoRa im selben Rate-Limit-Fenster (max. 2 pro 500 ms), das Millisekunden später eintreffende ACK war immer das dritte Frame und wurde still verworfen. ACK-Frames sind jetzt vom Monitor-Rate-Limit ausgenommen; zusätzlich wird ein Announce-ACK wie ein Message-ACK im API-Event-Puffer registriert und als Debug-Event (`announce_ack`) ausgegeben
- FIX: `wifi add` akzeptiert SSIDs (und Passwörter) mit Leerzeichen über Anführungszeichen-Syntax, z.B. `wifi add "Mein Netz" geheim`

### CI

- CI: native Test-Environment von der Nightly-/Release-Build-Matrix ausgenommen (Host-Tests laufen im eigenen Test-Workflow)

## [v26.7.0]

Ergebnis von drei tiefen Code-Review-Durchgängen (Datei-Review, Flow-/Mesh-Analyse, Stabilität/Ressourcen/Power). Alle Änderungen bauen über ESP32-S3, klassischen ESP32 und nRF52.

### NEU

- NEU: Abwärtskompatibler LoRa-Broadcast-Relay (`loraFloodSingle`, Default an) — ein geflooded (unrouted) LoRa-Frame wird als EINE Kopie an den besten Nachbarn eingereiht statt als eine Kopie pro Nachbar. Eine LoRa-Übertragung erreicht physisch ohnehin alle Nachbarn (Relay/Consume sind viaCall-unabhängig, Mithören-ACK greift), daher identische Flood-Abdeckung bei ~1/N Airtime. Das Frame ist byteidentisch zur bisherigen Einzelkopie → ältere Firmware im Netz merkt keinen Unterschied. Abschaltbar per `lora floodsingle 0`
- NEU: Status-LED (blinkende WiFi-/Status-LED) als Einstellung — Default AUS zum Stromsparen; schaltbar in der WebUI (System-Sektion), per Serial `led 0|1` und über die API
- NEU: Boot-Loop-Schutz / Safe-Mode (ESP32) — nach mehreren schnellen Reboots in Folge (RTC-Zähler) wird das Wiederherstellen von Peers/Routes/Nachrichten übersprungen, sodass der Node mit erreichbarer WebUI/AP hochkommt statt in einer Endlos-Reboot-Schleife zu bricken; der Zähler wird nach stabiler Laufzeit zurückgesetzt (ein bewusstes Power-Cycle setzt ihn ohnehin zurück)
- NEU: TX-Watchdog — ein hängendes `txFlag`/`rxFlag` (fehlgeschlagenes `startTransmit()` oder verpasster TX_DONE/RX_DONE-IRQ) legte bisher die gesamte Übertragung auf ALLEN Ports (LoRa, WiFi, Ethernet) dauerhaft still, ohne dass Loop-Watchdog oder Recovery griffen; wird jetzt nach 15 s erkannt, zurückgesetzt und das Radio neu initialisiert

### GEÄNDERT

- GEÄNDERT: Peer-/Route-Wartung läuft jetzt auch vor NTP-Sync — bisher wurde die komplette Wartung übersprungen, solange die Zeit nicht plausibel war, sodass LoRa-only-, AP-Mode- und nRF52-Nodes (die nie NTP bekommen) tote Peers nie entfernten und die Peer-Liste voll lief (Boot-Sekunden dienen als Zeitbasis, der NTP-Sprung wird weiterhin korrigiert)
- GEÄNDERT: Routen altern jetzt aus und werden beim endgültigen Peer-Timeout entfernt (wenn das Callsign auf keinem Transport mehr erreichbar ist) — behebt dauerhafte Black-Holes, wenn ein Next-Hop lebt, aber seine Weiterleitung zum Ziel gestorben ist
- GEÄNDERT: Uplink-Erkennung berücksichtigt Ethernet — `hasInternetUplink()` und der OTA-Update-Check galten nur bei WiFi-Verbindung; ein T-ETH-Elite über LAN (WiFi aus) meldete keine Topologie, führte keine Remote-Commands aus und updatete nie
- GEÄNDERT: LoRa-Modem-Parameter (SF/CR/BW/Preamble) werden aus WS/CLI/NVS auf SX126x-gültige Bereiche geklemmt — ein ungültiger Wert (z.B. SF 0) wurde bisher gespeichert und in die Airtime-Berechnung gefüttert (UB), während das Radio still den alten Modem-Zustand behielt
- GEÄNDERT: Duty-Cycle-Zähler (869,4–869,65 MHz) rechnet mit Worst-Case-Header — der bisherige Schätzwert unterschätzte die reale Time-on-Air relayter Frames (bis zu 5 Callsign-Felder), wodurch das 10%-Budget zu niedrig verbucht wurde
- GEÄNDERT: Reliable-Frames (Sync) werden pro Port serialisiert statt global — ein zähes LoRa-Sync-Frame blockierte bisher neue Sync-Frames auf WiFi/LAN
- GEÄNDERT: OTA-Update-Check läuft nur noch beim ersten WiFi-Connect nach dem Boot — bei instabilem WiFi stallte er sonst bei jedem Reconnect den Loop (≥10 s, bei gefundenem Update Minuten) und machte das Mesh taub; der 24-h-Timer deckt den Steady-State
- GEÄNDERT: API-Event-Ringpuffer (`api_evts.bin`) wird auf einer eigenen ~30-Minuten-Kadenz persistiert statt alle 5 Minuten bei jeglichem Traffic — deutlich weniger Flash-Verschleiß für volatile Diagnostikdaten
- GEÄNDERT: Peer-/Route-Persistenz kopiert die Listen unter kurzem `listMutex`-Halten und schreibt danach ins Flash — bisher blockierte ein bis zu 30 s langer Flash-Write jeden Listen-Zugriff (Reporting, Display)
- GEÄNDERT: Loop gibt im Leerlauf eine Tick ab (lässt Idle-Task/Power-Management laufen); T-Echo schaltet den ungenutzten L76K-GPS ab (~20–30 mA gespart)
- GEÄNDERT: PRNG wird beim Boot geseedet — identische Boards nach gemeinsamem Stromausfall erzeugten sonst dieselbe Announce-/ACK-/Retry-Jitter-Sequenz und kollidierten wiederholt

### FIX

- FIX (kritisch): OTA-Upload-Endpoint prüfte die Authentifizierung erst im Completion-Handler — das Firmware-Image wurde bereits geschrieben und als Boot-Partition gesetzt, bevor der 401 kam. Auth erfolgt jetzt VOR `Update.begin()` im Body-Handler; ein unauthentifizierter Upload wird abgewiesen, bevor irgendetwas geschrieben wird
- FIX (kritisch): U_SPIFFS-OTA überschrieb die gemountete LittleFS-Partition, während Hintergrund-Tasks weiter hineinschrieben — jetzt werden alle FS-Writes während eines Filesystem-Updates ausgesetzt und ein Reboot erzwungen (auch im `noreboot`-Pfad), damit das frische Image sauber gemountet wird
- FIX (kritisch): Data-Race auf `peerList`/`routingList` — der Loop-Task mutierte die Listen ohne `listMutex`, während der Hintergrund-Task (Reporting/Persistenz) unter Lock darüber iterierte (Use-after-free bei Reallokation). Alle loop-seitigen Mutatoren sperren jetzt (RAII-Guard `ListLock`)
- FIX: Callsign-Overflow — der serielle `call`-Befehl akzeptierte bis zu 16 Zeichen, die Frame-Felder fassen aber nur `MAX_CALLSIGN_LENGTH` (9); ein längeres Rufzeichen wurde ohne Nullterminator kopiert (korrupte On-Air-Frames, kaputte Dedup/ACK-Vergleiche). Jetzt geklemmt
- FIX: VLA-Stack-Overflow in `Frame::messageJSON()` — `char text[messageLength+1]` mit einer ungeprüften uint16-Länge (bis 65535) konnte den Task-Stack sprengen; jetzt fester Puffer + Clamp, und `messageLength` wird auch am WS-Eingang geklemmt
- FIX: Out-of-Bounds-Bit-Write auf `udpPeerLegacy[-1]` bei einem Legacy-UDP-Paket von unbekannter IP bei vollem Peer-Table (Heap-Korruption) — jetzt `peerIdx >= 0`-Guard
- FIX: `Frame`-Objekt wurde zwischen LoRa- und UDP-Empfang im selben Loop-Durchlauf ohne Reset wiederverwendet — ein kürzeres UDP-Frame erbte stale `dstCall`/`message`/`id` des vorherigen LoRa-Frames (Fehlklassifikation, falsche Dedup/ACK). Frame wird jetzt vor dem UDP-Parse zurückgesetzt
- FIX: ACK-Purge im TX-Buffer ignorierte `srcCall` — da IDs nur pro Quelle eindeutig sind, konnte ein kollidierendes `(nodeCall,id)` einer fremden Quelle ein unbeteiligtes wartendes Relay löschen. Jetzt Match über das volle `(srcCall,viaCall,id)`-Tupel
- FIX: `Frame::exportBinary()` — fehlende Längenprüfung vor Message-Header/ID, und die Truncation überschrieb das `messageLength`-Member (verkürzte Retransmits dauerhaft); jetzt Bounds-Check und lokale Clamp-Variable
- FIX: Duty-Cycle-Bucket entleerte sich unter Last kaum — durch Integer-Truncation und unbedingtes Vorrücken des Fensteranfangs verpuffte die abzubauende Zeit, sodass alle LoRa-Sendungen dauerhaft um 5 s verschoben wurden, gerade wenn das Mesh ausgelastet war
- FIX: Flux-Guard (`break` statt `continue`) hielt sendebereite WiFi-/LAN-Frames zurück, während er nur das LoRa-Pacing betreffen sollte
- FIX: Topologie-Report wurde nach einem fehlgeschlagenen POST für ~24,8 Tage unterdrückt statt in 60 s erneut versucht
- FIX: Originierendes `sendFrame()` übersprang LoRa bei einer veralteten WiFi-Route (fehlender Freshness-Check) — nach einem WiFi-Drop ging die Nachricht ins Leere; jetzt derselbe 60-s-Freshness-Gate wie im Relay-Pfad
- FIX: Bulk-Import (`/api/import`) akkumuliert jetzt den über mehrere TCP-Fragmente ankommenden Body — bisher wurde jedes Fragment als kompletter JSON-Body geparst, wodurch das Feature bei seiner eigentlichen Nutzlast (100+ Nachrichten) scheiterte
- FIX: `POST /api/messages` fehlte der `heapGuard` der übrigen Endpunkte; `batteryFullVoltage` aus der WS wird geklemmt (verhinderte Division durch ~0 in der Akku-Prozentberechnung); AP-Passwort mit 1–7 Zeichen wird abgelehnt (WPA braucht ≥8, sonst startet der AP nicht → Selbst-Aussperrung); AP-Passwort wird in `/api/settings` maskiert
- FIX: `strlen(nullptr)`-Absturz im `sendFrame`-WS-Handler bei nicht-String-`messageText`
- FIX: udpPeers-Vektoren in den Display-`doSave()`-Funktionen (Pager/SenseCAP) wurden nur teilweise neu aufgebaut → unterschiedliche Längen der Parallel-Vektoren und OOB-Zugriff; jetzt werden alle vier synchron gebaut
- FIX: `trimFileTask` prüft jetzt Schreibfehler und den `xTaskCreate`-Rückgabewert (kein stiller Datenverlust/Leak); der Notfall-Trim hat einen Debounce und einen Last-Resort-Truncate, damit ein voll gelaufenes Dateisystem keinen Task-Respawn-Sturm mehr auslöst
- FIX: Heap-Watchdog wird auf ~1 Hz gedrosselt statt in jedem Loop-Durchlauf `getMaxAllocHeap()` (Freelist-Scan) aufzurufen
- FIX (Display): SenseCAP zeigte den ältesten Bildschirminhalt statt der neuesten Nachrichten und invertiert; Gruppen-Tab-Touch-Trefferzone stimmte nicht mit den gezeichneten Tabs überein; „Nachr. löschen"-Menüpunkt war tot; Pager-Chat konnte bei kleiner Schriftgröße den Stack überlaufen (`tmp[]`-Puffer geklemmt); Pager warnt jetzt bei leerem Rufzeichen statt still zu verwerfen; Pager-Monitor-Frame-Labels ans Frame-Enum angeglichen
- FIX (Display/Strom): T-Echo-E-Paper wurde durch den 5-s-Status-Timer zu ständigen Full-Refreshes getrieben (~17k/Tag, Batterie-/Panel-Verschleiß) — E-Paper läuft jetzt nur noch über die eigene Update-Loop
- FIX: Doppelte Display-Initialisierung beim Boot (2,5-s-Splash/Freeze pro Speichern/Reinit) durch Once-Guard in `initDisplay()` behoben; doppelte Radio-Initialisierung beim Boot behoben (`pendingLoraReinit` wird nach dem Setup gelöscht)
- FIX: SPI-Lock für den „Flashing"-Bildschirm des Pagers während OTA (paralleler Radio-SPI-Zugriff aus dem AsyncTCP-Task hätte den Bus korrumpieren können)
- FIX: T-ETH-Elite hatte eine 16-MB-Partitionstabelle bei 8-MB-deklariertem Board — LittleFS jenseits 8 MB unbenutzbar und SPIFFS-OTA-Brick-Gefahr; `flash_size`/`maximum_size` korrigiert
- FIX: Nightly-CI verglich zum Skip-Check den Branchnamen statt der Commit-SHA (`--target dev-next`) und schnitt daher jede Nacht ein Release, auch ohne Commits — jetzt Vergleich der aufgelösten Commit-SHA
- FIX: Boot-Preload und weitere file-gespeiste Callsign-Kopien nutzen `strlcpy` (garantierte Nullterminierung)

## [v26.4.3]

- FIX: LittleFS-voll-Deadlock — war das Dateisystem einmal voll (z.B. messages.json + WebUI-Assets auf der 448-KB-Partition des Heltec V3), wurden alle Datei-Writes dauerhaft übersprungen, ohne dass der Trim je etwas entfernte (Zeilenlimit noch nicht erreicht). Nachrichten erschienen ab dann nie mehr in der WebUI. Jetzt: Notfall-Trim halbiert messages.json bei Platzmangel unabhängig vom Zeilenlimit
- FIX: Wegen Platzmangel übersprungene Datei-Writes werden jetzt im `fileWriter.dropped`-Zähler (`/api/status`) gezählt — ein volles Dateisystem ist damit in der Diagnose sichtbar statt still zu scheitern
- GEÄNDERT: Nachrichten-Limit für messages.json wird beim Boot aus der tatsächlichen LittleFS-Partitionsgröße berechnet ((Partition − 300 KB Reserve) / 300 B, min. 500, max. 5000) — Altgeräte mit 448-KB-Partition bleiben bei ~500, per USB/Web-Flasher neu geflashte Geräte mit 3,9-MB-Partition bekommen automatisch 5000; sanfter Übergang ohne Zwangs-Neuflash (fester Fallback `MAX_STORED_MESSAGES` 1000 → 500, gilt auch für nRF52/InternalFS)
- GEÄNDERT: Heltec V3 / V4 / Wireless Stick Lite V3 / HT-Tracker V1.2 nutzen jetzt `partitions_8MB.csv` — LittleFS wächst von 448 KB auf 3,9 MB, App-Slots von 1,75 MB auf 2 MB (die bisherige Tabelle nutzte nur 4 MB des 8-MB-Flash). Gilt nur für per USB/Web-Flasher neu geflashte Geräte; per OTA aktualisierte Geräte behalten das alte Layout (Firmware-OTA funktioniert weiter, das LittleFS-Update wird auf Altgeräten übersprungen)
- NEU: Task-Watchdog (120 s) auf der Main-Loop — hängt die Loop (HTTP/WebSocket laufen dann weiter, während Mesh-Verarbeitung, Status-Push und Nachrichtenempfang stehen), rebootet der Node jetzt automatisch; während OTA-Downloads wird der Watchdog temporär ausgesetzt
- NEU: Loop-Heartbeat `system.loopAgeMs` in `/api/diagnostics` — Loop-Gesundheit ist remote prüfbar
- FIX: Fehlgeschlagenes `prefs.begin()` (NVS korrupt / Fremd-Firmware-Reste) wird jetzt erkannt: NVS wird einmalig gelöscht und neu initialisiert statt dass alle Speichervorgänge still scheitern — Ursache für „CLI speichert keine Einstellungen"
- FIX: `saveSettings()` verifiziert den geschriebenen Config-Blob und meldet „Settings saved." bzw. „SETTINGS SAVE FAILED" in der Konsole; auch `saveWifiNetworks()`/`saveUdpPeers()` melden Schreibfehler
- FIX: Factory-Reset-Befehl `de`/`defaults` reagiert nur noch auf exakte Eingabe — vorher löste JEDE Eingabe, die mit „de" beginnt (z.B. „debug", Tippfehler), einen kompletten NVS-Wipe mit Reboot aus
- FIX: Serielle Konsole filtert Terminal-Steuerzeichen — Backspace/DEL editieren jetzt den Eingabepuffer, ANSI-Escape-Sequenzen (Pfeiltasten) werden verworfen. Vorher landeten die Bytes ungefiltert in gespeicherten Einstellungen (z.B. Backspaces im Rufzeichen) und machten jeden WebSocket-Frame zu ungültigem JSON — die WebUI blieb komplett leer
- FIX: Beim Settings-Laden werden Steuerzeichen aus gespeicherten Strings entfernt (Rufzeichen, Position, NTP, SSIDs) — bereits betroffene Geräte heilen sich mit dem Update selbst
- FIX: `/ota`-Upload prüft die Image-Größe gegen die Ziel-Partition BEVOR geschrieben wird — ein zu großes Filesystem-Image (z.B. 3,9-MB-littlefs.bin auf 448-KB-Altlayout) hinterließ vorher einen halbgeschriebenen Superblock, LittleFS panicte bei jedem Boot (`lfs_fs_grow`-Assert) und der Node war bis zum USB-Neuflash gebrickt

## [v26.4.2]

- NEU: Board Support Package (BSP) — abstraktes `IBoardConfig`-Interface ersetzt verstreute `#ifdef BOARD_XYZ`-Abfragen; Board-Fähigkeiten werden zur Laufzeit abgefragt statt zur Compile-Zeit
- NEU: `BoardFactory` als einzige Stelle im Projekt mit Board-Identitätsprüfung — aller übriger Code fragt Fähigkeiten über das Interface ab
- NEU: Display-Treiber nach Display-Typ statt Board-Name — drei identische SSD1306-U8g2-Treiber zu einem generischen konsolidiert; Treiber für ThingPulse-SSD1306, ST7735-TFT und GxEPD2-E-Paper jeweils als eigenständige, wiederverwendbare Module
- NEU: `IBoardConfig` um TFT- und E-Paper-Pin-Methoden erweitert — Display-Treiber sind damit vollständig board-unabhängig
- NEU: Shared Utilities `matchesDisplayGroup()` und `utf8ToCP437()` eliminieren duplizierten Code in 6 bzw. 2 Display-Treibern
- GEÄNDERT: Source-Tree komplett reorganisiert — `src/bsp/boards/` mit per-Board-Ordnern, `src/display/` für Display-Treiber, `src/mesh/`, `src/network/`, `src/hal/`, `src/util/` für thematische Gruppierung; `src/`-Root enthält nur noch 4 Dateien
- GEÄNDERT: `platformio.ini` mit `[esp32_base]` extends-Pattern konsolidiert — gemeinsame lib_deps, build_flags und Platform-Version zentral definiert
- GEÄNDERT: `build_src_filter` pro Environment steuert, welche HAL- und Display-Dateien kompiliert werden — ersetzt die bisherigen `#ifdef`-Kompilierungsguards
- GEÄNDERT: Dateinamen normalisiert — Bindestriche durch Unterstriche ersetzt für konsistente Benennung
- FIX: nRF52-Kompatibilität für `portENTER_CRITICAL` (Single-Core vs Multi-Core API-Unterschied)
- FIX: nRF52 `Preferences::getUShort` nicht verfügbar — Fallback auf `getUChar` mit Skalierung
- ENTFERNT: T-ETH-Elite Board ohne SX1262 (orphaned, kein PlatformIO-Environment)

## [v26.4.1a]

- GEÄNDERT: ESP32_E22_V1 — maximale TX-Leistung auf 33 dBm angehoben (E22-Modul mit PA)
- GEÄNDERT: TX-Power über Board-Maximum wird nicht mehr still begrenzt — stattdessen Warnung in Serial, WebUI und beim Laden aus NVS
- NEU: WebUI zeigt beim Speichern einen Bestätigungsdialog wenn TX-Power das Board-Maximum überschreitet
- FIX: Duplizierte `handleImportMessages`-Funktion in api.cpp entfernt (Merge-Artefakt)
- FIX: OTA-Update schlug bei Versionen mit Build-Counter (`+bNNN`) fehl — `+` wurde in der URL als Leerzeichen interpretiert; URL-Parameter werden jetzt korrekt kodiert
- FIX: `newVersion`-Puffer von 32 auf 64 Bytes vergrößert — lange Version-Strings wurden abgeschnitten
## [v26.4.1]

- NEU: Support für LILYGO T-ETH-Elite + SX1262-Shield — Ethernet (W5500), WiFi und LoRa; neuer HAL, Pin-Mapping und Board-Dokumentation
- NEU: Ethernet-Support (W5500) — statische IP oder DHCP, eigene Settings-Sektion in der WebUI, persistiert in NVS
- NEU: Per-Interface Service-Toggles — WiFi und LAN lassen sich einzeln für Node-Kommunikation und WebUI aktivieren/deaktivieren; primäres Interface wählbar (Auto / WiFi / LAN)
- NEU: Port-Priorisierung — Announces und Sync-Frames werden in der Reihenfolge Primary → Secondary → LoRa eingereiht
- NEU: Alle Settings-Sektionen in der WebUI sind per Chevron-Toggle einklappbar — Akku- und OLED-Schalter wandern in den jeweiligen Sektions-Header, Deaktivieren klappt die Sektion automatisch zu
- NEU: Kategorisierte Dropped-Frame-Zähler — pro Verwurf-Grund und Frame-Typ aufgeschlüsselt, sichtbar in WebUI, CLI und per WebSocket
- NEU: API-Event-Ringpuffer wird auf LittleFS persistiert (`/api_evts.bin`) und überlebt Reboots; API-Nachrichten-Puffer wird beim Boot aus `/messages.json` reseedet
- NEU: API-Puffergrößen erhöht (Messages 5 → 32, Events 10 → 64), da kein ACK-Purge mehr nötig ist
- NEU: TX-Puffergröße plattformabhängig — Default 20 → 64 Frames (nRF52: 32), per Board überschreibbar via `TX_BUFFER_SIZE`
- NEU: Netzwerk-Tab in der WebUI umstrukturiert — eigene Sektionen für Allgemein, WiFi, Ethernet und UDP-Peers; Mesh-Sektion mit Sync Word, Repeat und Max-Hops aus dem LoRa-Tab hierher verschoben
- NEU: Serielles Kommando-Interface überarbeitet — `wifi`-Befehle um `wifi 0/1`, `wifi tx`, `wifi scan` erweitert; neue `eth`-Befehlsgruppe; `show settings` mit strukturierter, kategorisierter Ausgabe
- ENTFERNT: Aggregierter `/api/poll`-Endpunkt — Clients nutzen die Einzel-Endpunkte, spart ~10 KB Heap
- FIX: Relay-Pfad löschte beim Empfang eines Duplikats alle noch im TX-Buffer wartenden Relay-Kopien derselben Message-ID — auch bereits eingereihte, noch nicht gesendete. Jetzt werden nur neue Kopien per Dedup verhindert, bestehende bleiben intakt
- FIX: WiFi-Favorit wurde beim Speichern still auf das alte Netzwerk zurückgesetzt — `saveSettings()` behandelte das Legacy-Feld `settings.wifiSSID` als autoritativ und überschrieb den vom Benutzer gewählten Favoriten. Jetzt ist `wifiNetworks` die einzige Quelle der Wahrheit
- FIX: Deaktivierte UDP-Peers wurden nur beim Senden übersprungen — eingehende Pakete von ihnen wurden weiterhin verarbeitet. Jetzt werden auch empfangene Frames von deaktivierten Peers sofort verworfen
- FIX: T-ETH-Elite Pin-Mapping für SX1262-Shield korrigiert (CS, RST, IRQ, BUSY, SD_CS) — Quelle jetzt offizielles LilyGO `utilities.h`
- FIX: Uhr in der WebUI tickte nur alle 3 Sekunden (synchron mit dem Status-Push) — jetzt clientseitige Interpolation jede Sekunde, Server-Sync alle 3 s
- FIX: Redundante DOM-Lookups für `settingsMycall` in der Nachrichtenanzeige entfernt; Race-Condition beim initialen Datenabruf behoben
- NEU: TX-Leistung wird pro Board auf das Hardware-Maximum begrenzt (`LORA_MAX_TX_POWER`) — Clamping beim Laden, in Serial, WebUI und API; WebUI zeigt den erlaubten Maximalwert als Hinweis an
- NEU: LoRa-Recovery — schlägt die Radio-Initialisierung fehl, wird alle 30 s automatisch ein erneuter Versuch unternommen; Radio-Init auf T-ETH-Elite mit bis zu 3 Versuchen und SPI-Guard gegen blockiertes SPI-Peripheral
- NEU: Topologie-Report sendet Firmware-Version und Device-Typ an den Bridgeserver
- FIX: OTA-Update nutzt HTTP statt HTTPS — entfernt unnötigen TLS-Overhead und `WiFiClientSecure`-Heap-Verbrauch
- PERF: FileWriter gruppiert Writes nach Dateiname pro Mutex-Hold — weniger Flash-Metadata-Flushes und kürzere Stalls bei Bursts

## [v1.0.32]

- FIX: REST-API-Nachrichten-/Event-Puffer werden nicht mehr bei jedem Read destruktiv geleert — der `ack`-Parameter auf `/api/messages` und `/api/events` wird nur noch akzeptiert, aber ignoriert. Mehrere Clients (Browser, Bridgeserver, …) sehen jetzt denselben Live-Tail.
- ENTFERNT: Aggregierter `/api/poll`-Endpunkt — Clients nutzen die Einzel-Endpunkte (`/api/status`, `/api/peers`, `/api/routes`, `/api/messages`, `/api/events`, `/api/groups`, `/api/diagnostics`). Spart ~10 KB statischen JSON-Build-Buffer und damit freien Heap.
- NEU: API-Nachrichten-Ringpuffer wird beim Boot direkt aus `/messages.json` reseedet — kein separater `/api_msgs.bin` mehr nötig. Reduziert FS-Schreiblast und entfernt eine Quelle für `fsMutex`-Konkurrenz. Alte `/api_msgs.bin` wird beim ersten Boot automatisch entfernt.
- NEU: API-Event-Ringpuffer wird auf LittleFS persistiert (`/api_evts.bin`) und beim Boot wiederhergestellt — der Event-Tail überlebt jetzt Reboots
- NEU: API-Puffergrößen erhöht (Messages 5 → 32, Events 10 → 64), da kein ACK-Purge mehr nötig ist
- HEAP: `ApiEvent`-Struktur entschlackt — `source[12]`, `text[64]`, `action[8]`, `dest[7]`, `hops` und `event[8]` (jetzt 1-Byte-Enum) raus. Spart ~6,5 KB statisches BSS bei 64 Slots. `apiRecordErrorEvent` (war ungenutzt) und `apiRecordRoutingEvent` (Duplikat zum normalen Route-Log) entfernt.
- HEAP/FS: FileWriter-Slot-Pool 16 → 8 (~8,5 KB statisches BSS frei). Allokator findet jetzt einen freien Slot statt blind den ältesten zu überschreiben — kein stiller Datenverlust mehr bei Saturation.
- PERF/FS: FileWriter drained alle pending Slots pro `fsMutex`-Hold und gruppiert Writes nach Dateiname → 1× `open`/`close` pro Datei pro Batch statt pro Slot. Bei Bursts deutlich weniger Flash-Metadata-Flushes, niedrigere Fragmentierung, kürzere Stalls.
- NEU: FileWriter-Diagnostik in `/api/status` (`diagnostics.fileWriter`) — `pending`, `maxPending` (Lifetime-Watermark), `slots`, `writes`, `dropped`. Vom Bridgeserver in die Telemetrie-DB übernommen und im Dashboard als sechster Chart pro Node sichtbar.
- FIX: Routing-Update-Fall logged jetzt explizit `Updated route: …` (vorher nur „new route" wurde geloggt, Updates fielen unter den Tisch).
- FIX: Relay-Pfad löschte beim Empfang eines Duplikats *aller* noch im `txBuffer` wartenden Relay-Kopien für dieselbe Message-ID — auch der eigenen, die noch nie gesendet wurden. Dadurch gingen Messages häufig benutzter Quellen verloren, sobald innerhalb der WiFi-Retry-Wartezeit (2 s) eine zweite Relay-Quelle dieselbe Message rebroadcastete. Der Dedup beim *neuen* Enqueue über `found` reicht aus; bestehende, schon eingestellte Kopien werden jetzt nicht mehr abgewürgt.

## [v1.0.31b]

- NEU: Rotierende Multi-Screen-UI (`ID` / `NET` / `LoRa` / `MSG` / `SYS`) jetzt auch auf HELTEC WiFi LoRa 32 V3, LILYGO T3 LoRa32 V1.6.1 und LILYGO T-Beam — gemeinsame Page-Renderer und Rotations-Logik für alle U8g2-Boards
- NEU: LILYGO T-Echo nutzt dieselbe Rotation (Page-Mask + Button-Cycling, kein Auto-Advance um das E-Paper zu schonen) und zeigt jetzt Boot-Splash und Flashing-Screen
- NEU: Boot-Splash und „Flashing"-Screen während OTA/HTTP-Updates jetzt auch auf LILYGO T-LoraPager und SEEED SenseCAP Indicator
- NEU: Splash- und Flashing-Screens werden grundsätzlich angezeigt, auch wenn das Display in den Einstellungen deaktiviert ist
- NEU: Anzeige verworfener Frames in der WebUI — neuer Lifetime-Counter zählt Pakete, die nach Erschöpfen aller Retries verworfen wurden (sowohl Multi-Retry-Purge bei unerreichbaren Peers als auch einmalige ACK-Drops), sichtbar als „Verworfen" / „Dropped" in der Statusleiste

## [v1.0.31a]

- NEU: Routing ignoriert direkte Peers, deren SNR unter dem konfigurierten `minSnr`-Schwellwert liegt — der direkte Routen-Eintrag wird entfernt, sodass eine Mehrhop-Alternative über einen stärker empfangenen Nachbar-Node übernehmen kann
- NEU: Filterung und Sortierung in der Peer- und Routing-Tabelle der WebUI
- FIX: `addRoutingList` akzeptiert wieder 0-Hop-Einträge für direkte Nachbarn (war fälschlich als Loop verworfen worden)
- FIX: Heap-Statistik-Aufrufe für nRF52 abgesichert (kein ESP-spezifischer Heap-Code mehr auf nRF52-Plattformen)
- FIX: `bgWorker` nutzt den plattformkorrekten FreeRTOS-Include-Pfad für nRF52

## [v1.0.31]

- FIX: Heap- und Langzeitstabilität grundlegend verbessert — deutlich weniger kurzlebige Allokationen in den Hot-Paths (Topologie-Report, Status-/Peer-/Routing-Broadcasts, Auth, WiFi-Scan, Frame-Verarbeitung, UDP-Peer-Verwaltung). Behebt u. a. ein Memory-Leak in `sendPeerList()`, das nach ~3,5 h zum OOM-Crash führte, sowie eine Task-Stack-Fragmentierung, die nach längerer Laufzeit AsyncTCP zum Hängen brachte
- NEU: Heap-Watchdog — automatischer Reboot bei < 10 KB freiem Heap verhindert den Zombie-Zustand (LoRa läuft, WiFi/Web tot)
- FIX: LittleFS „No more free space"-Abstürze beim Schreiben von Logs/Nachrichten verhindert (Freiplatz wird jetzt vor dem Schreiben geprüft)

- NEU: WebUI grundlegend überarbeitet — Mobile und Desktop zu einem gemeinsamen responsiven Interface zusammengeführt, mit Mehrsprachigkeit (DE/EN), Uptime-Anzeige, einheitlichem Stylesheet, SVG-Icons, einklappbaren Settings-Bereichen und verbesserten Tabellen-Layouts
- NEU: CPU-Frequenz einstellbar (80 / 160 / 240 MHz, Default 240 MHz) — persistiert, sofort wirksam, konfigurierbar in der WebUI
- NEU: Channel 1 (all) und 2 (direct) können per Doppelklick stummgeschaltet werden
- NEU: Gruppennamen werden persistent auf dem Node gespeichert und zwischen allen verbundenen Clients synchronisiert (vorher nur pro Browser)
- UI: Setup-Tab neu sortiert — Allgemein → System → Online Update → Firmware Upload → Sicherheit → Akku → OLED Display → Debug

- NEU: Support für Heltec HT-Tracker V1.2 (Wireless Tracker) mit TFT-Statusanzeige und Button-Steuerung
- NEU: Platform-Abstraktion für nRF52840-basierte Boards (`NRF52_PLATFORM`), erster experimenteller Bringup für LILYGO T-Echo (noch nicht produktionsreif)
- NEU: SSD1306-OLED-Support für HELTEC WiFi LoRa 32 V3, LILYGO T3 LoRa32 V1.6.1, LILYGO T-Beam sowie das ESP32 E22 Multimodul (Rentner Gang)
- NEU: Display-Einstellung wird persistent gespeichert; kurzer Tastendruck schaltet das Display, langer Druck wechselt den WiFi-Modus; Nachrichten-Gruppe für die Display-Anzeige ist konfigurierbar
- NEU: Automatische Display-Erkennung beim T-Beam sowie Vext-Steuerung für HELTEC V3; diverse Display-Korrekturen für T-Beam und HELTEC V3
- NEU: Multi-Screen-UI für das ESP32-E22-Display — rotierende Seiten `ID` / `NET` / `LoRa` / `MSG` / `SYS`, neue Nachrichten springen automatisch auf die MSG-Seite. Seitenwechsel-Intervall, sichtbare Seiten und optionaler Taster-GPIO sind in der WebUI unter „OLED Display" einstellbar
- NEU: 5 s Boot-Splashscreen auf dem ESP32-E22-Display mit „rMesh"-Überschrift, Versions-String und Node-Callsign — unabhängig vom Display-Setting
- NEU: Während eines OTA-/HTTP-Firmware-Updates zeigt das ESP32-E22-Display „Flashing…" großflächig an

- NEU: Routing-Tabelle und Peer-Liste werden im Flash gespeichert und stehen nach Reboot direkt wieder zur Verfügung; Kapazität für gespeicherte Routen erhöht
- NEU: mDNS-Support — Nodes sind im lokalen Netzwerk per `<callsign>-rmesh.local` erreichbar
- NEU: Erweiterte WiFi- und AP-Verwaltung inklusive verbesserter WiFi-Client/AP-Tabelle in der WebUI

- NEU: Erweitertes serielles Kommando-Interface — neue Befehle: `msg`, `xgrp`, `xtrace`, `announce`, `dbg`, `uc`, `updf`, `peers`, `routes`, `acks`, `xtxbuf`; Hilfe und Befehlsliste in Kategorien gegliedert

- FIX: TX-Buffer-Handling verbessert — behebt verlorene Frames, Duplikate und festhängende Einträge bei unerreichbaren Peers
- FIX: Private WiFi/UDP-Nachrichten an fremde Callsigns werden nicht mehr lokal angezeigt oder gespeichert, sondern nur noch weitergeleitet
- FIX: Absturz bei DNS-/HTTP-Fehlern im Update- und Reporting-Pfad behoben
- FIX: Mehrere Stabilitätsprobleme behoben, u. a. bei Speicher-Allokationen, Timern, Reboot-Logik, Buffer-Grenzen, TRACE-Echo, File-Handling und Auth-Session-Verwaltung
- FIX: Gerichtete Nachrichten gingen verloren, wenn der geroutete Next-Hop unavailable oder identisch mit dem Absender war — Relay fällt jetzt auf Flooding zurück statt die Nachricht stillschweigend zu verwerfen
- FIX: Extrem langsame WiFi-Reaktion bei Retransmit-Fluten von Nachbar-Nodes — Duplikat-Erkennung für MESSAGE_FRAMEs greift jetzt vor der teuren Nachverarbeitung
- FIX: Eingabefeld wird nach dem Senden automatisch geleert
- FIX: UDP-Peer-Auflistung zeigt jetzt auch den Enabled-Status an; `udp add` Serial-Befehl setzt das Enabled-Flag nun korrekt
- FIX: Automatische Update-Prüfung wird bei Nightly-Builds unterdrückt (verhinderte unnötige Downgrade-Versuche)
- NEU: Peer-Cooldown (10 min) nach Retry-Exhaustion verhindert den Announce→Relay→Exhaust→Re-Announce-Zyklus bei einseitigen Funkverbindungen

- NEU: Nach LoRa-Sendungen wird eine zusätzliche Guard-Zeit eingehalten, damit Empfänger sicher in den RX-Modus zurückkehren können
- NEU: Duty-Cycle-Enforcement für das öffentliche 869,4–869,65-MHz-Band — überschrittene Sendungen werden verzögert statt verworfen
- NEU: Kapazitätslimits für Peer-, Routing- und UDP-Peer-Listen verhindern unkontrolliertes Wachstum
- NEU: Konfigurierbarer minimaler SNR-Schwellwert für die Peer-Liste
- CHANGE: ACK- und Retry-Timing für dichtere Mesh-Topologien angepasst

## [v1.0.30a]

- FIX: OTA-Update schlug auf langsamen Verbindungen mit „HTTP error: read Timeout" fehl – TCP-Read-Timeout für LittleFS- und Firmware-Download von 30 s auf 120 s erhöht; betrifft sowohl automatische als auch manuelle Updates

## [v1.0.30]

- NEU: Support für Seeed XIAO ESP32-S3 + Wio-SX1262 – neues HAL (`hal_SEEED_XIAO_ESP32S3_Wio_SX1262`) für das Seeed XIAO ESP32-S3 Board mit aufgestecktem Wio-SX1262 LoRa-Modul (B2B-Stecker); Build-Konfiguration in PlatformIO, Eintrag in `devices.json` für das Web-Flash-Tool
- NEU: Manueller Firmware-Upload über die WebUI – neuer `/ota`-Endpunkt im Webserver zum direkten Flashen eigener Firmware- und LittleFS-Binaries ohne OTA-Server; Desktop- und Mobile-Interface erhalten einen „Upload & Flash"-Button, der beide Dateien sequenziell hochlädt und die Node danach neu startet
- NEU: Akkustand-Anzeige für HELTEC WiFi LoRa 32 V3 und Wireless Stick Lite V3 – Spannung wird per ADC (GPIO1, VBAT_CTRL) mit 8-Sample-Mittelung gemessen; in der WebUI (Desktop & Mobile) als Akkubalken angezeigt; aktivierbar/deaktivierbar in den Einstellungen; Referenzspannung (Default 4,2 V) konfigurierbar
- NEU: Zweistufige Peer-Inaktivität – Peers werden nach 25 Minuten ohne Lebenszeichen zunächst als nicht verfügbar markiert (kein Routing mehr über diesen Peer), aber erst nach 60 Minuten vollständig aus der Liste entfernt; verhindert abrupte Routing-Ausfälle bei kurz nicht erreichbaren Nodes
- FIX: Peer-Timestamps nutzen jetzt `time()` (Unix-Sekunden) statt Millisekunden; `availablePeerList()` aktualisiert den Timestamp beim Reaktivieren eines Peers korrekt; `addPeerList()` verwendet `time(NULL)` statt `f.timestamp` für konsistente Wanduhr-Zeitstempel
- NEU: Toast-Benachrichtigungssystem in der Desktop-WebUI – Statusmeldungen und Aktions-Feedback werden als animierte Toast-Einblendungen angezeigt (Ein- und Ausblend-Animation, automatisches Ausblenden)
- NEU: Support für ESP32 E22 LoRa Multimodul V1 – neues HAL (`hal_ESP32_E22_V1`) für Eigenbauplatine mit ESP32 und E22 LoRa-Modul (SX1262); Build-Konfiguration in PlatformIO (`env:ESP32_E22_V1`), Eintrag in `devices.json` für Web-Flash-Tool; `-Os` Optimierungsflag für kompaktere Firmware
- DOKU: Technische Dokumentation für alle unterstützten Boards neu strukturiert – Verzeichnis `Doku/` nach `docu/` umbenannt (einheitlich englisch); Datenblätter und Schaltpläne für HELTEC WiFi LoRa 32 V3/V4, Wireless Stick Lite V3, LILYGO T-Beam und T3 ergänzt; ESP32 E22 Multimodul-Dokumentation (Schaltplan, Bestückungsplan, Gehäuse-3MF-Dateien) hinzugefügt
- CLEANUP: `build.bat` entfernt, ungenutztes LilyGoLib-ThirdParty-Submodul entfernt, PlatformIO-Boilerplate-README-Platzhalter entfernt

## [v1.0.29e]

- FIX: Serielle Konsole – `h`-Befehl (Hilfe) zeigte seit v1.0.29b keine Ausgabe mehr – `help.txt` wurde durch den Filesystem-Build per gzip komprimiert (`.txt` in `COMPRESS_EXTENSIONS`) und lag im LittleFS nur noch als `help.txt.gz`; der Code öffnete aber `/help.txt` – Datei wurde nicht gefunden, keine Ausgabe; `.txt` aus den komprimierten Erweiterungen entfernt, `help.txt` liegt jetzt wieder unkomprimiert im LittleFS

## [v1.0.29d]

- NEU: Serielle Konsole – `uc 0` / `uc 1` setzt den Update-Kanal (Release/Dev) und speichert ihn persistent; `updf` / `updf 0` / `updf 1` startet eine Force-Installation aus dem gewählten Kanal
- NEU: Frisch geflashte Nodes wählen den Update-Kanal automatisch passend zur Firmware: Dev-Builds (`-dev`-Suffix) setzen den Default auf „Dev", Release-Builds auf „Release" – ein bereits gespeicherter Wert im Flash bleibt erhalten
- FIX: WebUI wurde nach dem LittleFS-Komprimierungs-Update (v1.0.29b) nicht mehr angezeigt – der Webserver suchte `index.html`, im LittleFS lag aber nur noch `index.html.gz`; Exists-Prüfung und Auslieferung explizit korrigiert: `.gz`-Pfad direkt öffnen, Content-Type anhand der Original-Extension setzen, `Content-Encoding: gzip` Header manuell hinzufügen
- FIX: WebUI fehlte nach Installation über Web-Flash-Tool – LittleFS-Offset in `devices.json` war noch `0x290000` (alter Partitionstabellen-Stand vor v1.0.29b); korrekt ist `0x390000`; Flash-Manifest hat LittleFS an die falsche Adresse geschrieben
- FIX: Sendeverzögerung ohne UDP-Peers – ohne konfigurierte UDP-Peers wurde vor dem LoRa-Send unnötig ein WiFi-Blind-Frame gepusht; WiFi-Blind-Send wird jetzt nur ausgeführt wenn mindestens ein UDP-Peer konfiguriert ist (Announces werden weiterhin immer per WiFi-Broadcast gesendet)

## [v1.0.29c]

- FIX: OTA-Update von v1.0.29a → v1.0.29b schlug auf LILYGO T3 LoRa32 V1.6.1 mit „Not Enough Space" fehl – Firmware war 749 Bytes zu groß für die alte 1.280-KB-Partition; nicht benötigte Serial-Debug-Ausgaben entfernt (Trim-Task-Status, UDP-Peer-Migration, WiFi-Scan-Tabelle, Topologie-Reporting); Firmware um 1.252 Bytes reduziert und damit OTA-Update-Pfad auf Geräten mit alter Partitionstabelle wieder freigegeben

## [v1.0.29b]

- FIX: OTA-Update schlug in manchen Netzwerken mit "read Timeout" fehl – LittleFS- und Firmware-Download werden jetzt bei Fehler bis zu 3x wiederholt
- NEU: Update-Kanäle – in der WebUI (Desktop & Mobile) kann zwischen „Release" (Standard) und „Dev" (Pre-releases) gewählt werden; die Node aktualisiert sich automatisch aus dem gewählten Kanal
- NEU: Force-Install-Button – erzwingt ein Update aus dem eingestellten Kanal, auch wenn die installierte Version neuer ist oder ein lokaler Dev-Build aktiv ist
- NEU: Display-Geräte (T-LoraPager, SenseCAP Indicator) haben im Einstellungsmenü neue Einträge „Update Release" und „Update Dev" zum erzwungenen Installieren
- NEU: GitHub-Releases werden automatisch als Pre-release markiert, wenn der Tag ein `-` enthält (z. B. `v1.0.30-dev`) – stabile Tags ohne `-` bleiben normale Releases
- Abwärtskompatibilität: Nodes mit älterer Firmware erhalten weiterhin stabile Release-Updates; der Backend-Default ist der Release-Kanal
- FIX: Doppelte ACKs bei Nodes die gleichzeitig per WiFi und LoRa erreichbar sind – WiFi wird jetzt konsequent bevorzugt: ACKs, Announce-ACKs und weitergeleitete Nachrichten gehen nur noch über den jeweils verfügbaren Weg (WiFi oder LoRa, nie beide)
- NEU: WiFi ist primärer Übertragungsweg, LoRa ist Fallback – Nachrichten an Peers die per UDP erreichbar sind, werden ausschließlich per WiFi gesendet; LoRa wird nur genutzt wenn kein WiFi-Pfad zum Ziel existiert
- NEU: Announcements und Broadcast-Nachrichten werden weiterhin auf beiden Wegen gesendet (WiFi und LoRa), damit LoRa-only Nodes nicht ausgeschlossen werden
- NEU: Sendreihenfolge – WiFi wird vor LoRa in den Sendepuffer eingereiht, da UDP deutlich schneller übertragen wird
- NEU: UDP-Peer-Verfügbarkeit wird regelmäßig geprüft – beim Senden eines Announces werden alle WiFi-Peers auf „nicht verfügbar" gesetzt und erst durch den eintreffenden Announce-ACK wieder aktiviert; offline gegangene Nodes werden so spätestens nach einem Announce-Zyklus (~10 Min) erkannt
- NEU: Rufzeichen je UDP-Peer wird automatisch gelernt – sobald eine Node einen Frame sendet, wird ihr Rufzeichen der IP-Adresse zugeordnet und in der WebUI (Desktop & Mobile) bei den UDP-Peers angezeigt
- NEU: HF-Deaktivierungsschalter in der WebUI (Desktop & Mobile) – ist HF deaktiviert, werden alle LoRa-Frames still verworfen und es wird garantiert nichts über HF gesendet; Zustand wird persistent gespeichert
- NEU: Shutdown-Button in der WebUI (Desktop & Mobile) mit Sicherheitsabfrage – versetzt das Gerät in Tiefschlaf (kein Senden mehr); Aufwecken nur per Hardware-Reset oder Stromtrennung; nützlich für Akku-Geräte ohne Antenne
- FIX: Flash-Overflow bei LILYGO T3 LoRa32 V1.6.1 und T-Beam – Partitionstabelle neu ausbalanciert: App-Partition auf 1.792 KB vergrößert (war 1.280 KB), LittleFS auf 448 KB verkleinert; Firmware-Auslastung sinkt von 95 % auf 71 %
- Optimierung: WebUI-Assets (HTML, JS, CSS, TXT) werden beim Filesystem-Build automatisch per gzip komprimiert und als .gz-Dateien ins LittleFS-Image verpackt; ESPAsyncWebServer liefert sie transparent komprimiert aus – LittleFS-Inhalt um 61 % reduziert (275 KB → 110 KB); Quellfiles bleiben unverändert editierbar
- Optimierung: Retro-Font „Fixedsys Excelsior" (167 KB) aus dem LittleFS entfernt – Desktop-WebUI verwendet nun den systemseitigen Fallback-Font „Courier New" (optisch nahezu identisch)

## [v1.0.29a]

- FIX: Migration – UDP-Peers aus altem Firmware-Format werden beim ersten Boot automatisch in die neue dynamische Peer-Liste übernommen und gehen nicht mehr verloren

## [v1.0.29]

- NEU: UDP-Peer-Liste ist jetzt unbegrenzt dynamisch – vorher war sie auf 5 Einträge begrenzt; Verwaltung über WebUI, Display und serielle Konsole (`udp add`, `udp del`, `udp <N>`, `udp clear`)
- NEU: UDP-Peers aktivieren/deaktivieren – jeder Peer hat eine Aktiv-Checkbox; deaktivierte Peers werden beim Senden übersprungen
- NEU: Automatische Peer-Erkennung per Broadcast – Announcements werden immer auch per UDP-Broadcast gesendet; antwortende Nodes werden automatisch in die Peer-Liste eingetragen
- NEU: Legacy-Node-Erkennung – Nodes ohne SyncWord-Präfix (alte Firmware) werden automatisch erkannt, als Peer eingetragen und per Legacy-Flag markiert, damit sie weiterhin ohne SyncWord versorgt werden
- NEU: OTA-Update-Button in allen UIs (WebUI Desktop, WebUI Mobile, Display-Menü, serielle Konsole `update`) – startet die Update-Prüfung manuell
- NEU: Update-Statusmeldung per WebSocket – die UI zeigt ob das Gerät bereits aktuell ist, kein Server erreichbar war, oder ein Update installiert wird
- NEU: Aktions-Feedback in der WebUI – beim Betätigen von Reboot, Announce und Tune erscheint eine Bestätigungsmeldung
- NEU: UDP-Peers werden in der WebUI als Tabelle (mit Header-Zeile) dargestellt
- NEU: Gruppen stummschalten (Mute) – Nachrichten werden weiterhin angezeigt, lösen aber keinen Sound oder Ungelesen-Badge aus. Gilt für WebUI Desktop, WebUI Mobile und Display-Geräte (T-LoraPager, SenseCAP Indicator).
- NEU: Sammelgruppe – ein Channel-Tab (Desktop-WebUI) bzw. eine Gruppe (Mobile, Display) kann als Sammelgruppe definiert werden. Dort landen automatisch Nachrichten von Gruppen, die per Name eingetragen wurden und keinen eigenen Tab/Slot haben – sie erscheinen nicht mehr in „all". Einstellung über Doppelklick auf den Channel-Button (Desktop) bzw. Langdruck auf den Gruppen-Tab (Mobile) bzw. Gruppenmenü (Display).
- FIX: Nachrichten wurden weitergeleitet, obwohl der eigene Node das Ziel war – die Weiterleiten-Bedingung prüfte `tf.dstCall`/`tf.hopCount` statt `f.dstCall`/`f.hopCount`; `tf` war zu diesem Zeitpunkt noch nicht befüllt und enthielt Leer- oder Altwerte (Issue #6)
- NEU: Alle WebUI-Einstellungen sind jetzt auch über die serielle Konsole setzbar – neue Befehle: `call`, `pos`, `ntp`, `op`, `bw`, `sf`, `cr`, `pl`, `sw`, `rep`, `mhm`, `mhp`, `mht`, `udp` (Issue #5)
- NEU: WebUI-Passwort über die serielle Konsole setzbar/löschbar: `webpw <passwort>` bzw. `webpw -`
- NEU: LoRa-Frequenz- und SyncWord-Felder in der WebUI sind jetzt editierbar; bei manuellem Bandwechsel (433↔868 MHz) werden die Band-Defaults automatisch geladen, die eingetippte Frequenz bleibt erhalten
- NEU: SyncWord ist jetzt manuell setzbar (WebUI, Konsole `sw <hex>`) und wird nicht mehr automatisch aus der Frequenz überschrieben; Band-Presets setzen es weiterhin korrekt
- FIX: 868-MHz-Preset-Default-TX-Power korrigiert: war 22 dBm, ist jetzt korrekt 27 dBm (500 mW, regulatorisches Maximum)

## [v1.0.28]

- NEU: UDP-Netzwerktrennung – jedes UDP-Paket enthält jetzt das SyncWord als erstes Byte. Nodes akzeptieren per UDP nur noch Pakete aus dem eigenen Frequenzband (433 MHz oder 868 MHz). Verbindet man versehentlich Nodes aus verschiedenen Bändern per UDP, bleiben die LoRa-Netze trotzdem getrennt.
- Abwärtskompatibilität: Pakete ohne SyncWord-Präfix (alte Firmware) werden als 433-MHz-Netz (AMATEUR_SYNCWORD) behandelt und von 433-MHz-Nodes weiterhin akzeptiert.

## [v1.0.27a]

- NEU: Unterstützung für Seeed SenseCAP Indicator D1L ergänzt
- FIX: TX-Power-Begrenzung im 868-MHz-Public-Band auf korrekte 27 dBm (500 mW) angehoben – vorheriger Wert von 22 dBm war zu restriktiv

## [v1.0.27]

- NEU: Zweites, getrenntes 868-MHz-Public-Netz (869,525 MHz, Sub-Band P) ergänzt – ohne Amateurfunklizenz nutzbar; Trennung zum 433-MHz-Amateurfunknetz auf PHY-Ebene (SyncWord) und in der Software/Weboberfläche
- NEU: Frequenz-Presets für 433 MHz (Amateurfunk) und 868 MHz (Public) – Frequenz und passende LoRa-Parameter werden je Band automatisch gesetzt
- NEU: Serielle Konsole um freq 433 und freq 868 erweitert – setzt direkt das jeweilige Frequenz-Preset
- NEU: Topo-Ansicht überarbeitet – bessere Routen-Darstellung, Node-Suche und stabileres/verändertes Auto-Refresh-Verhalten
- NEU: TX-Power im Public-Band auf max. 22 dBm begrenzt und Duty-Cycle-Tracking für 868 MHz ergänzt
- Geändert: SyncWord wird jetzt automatisch aus dem Frequenzband abgeleitet (433: 0x2B, 868: 0x12) und kann nicht mehr manuell im UI geändert werden
- Geändert: HF-Modul bleibt bei Erstinstallation deaktiviert, bis ein Band gewählt wurde; bestehende 433-MHz-Geräte behalten ihre bisherigen Einstellungen
- Geändert: Reporting um chip_id, is_afu und band erweitert
- Website: Nicht mehr benötigte Topology- und Update-Endpunkte entfernt, ungenutzten Code bereinigt und Wartbarkeit verbessert

## [v1.0.26]

- NEU: Passwortschutz für das Web-Interface – optional, Challenge-Response-Verfahren über WebSocket (Server sendet Nonce, Client antwortet mit SHA-256(Passwort + Nonce)). Ohne gültiges Passwort werden keine Daten übertragen. Das Passwort wird als SHA-256-Hash im Flash gespeichert. Einrichtung, Änderung und Entfernung direkt im Setup-Bereich.
- NEU: Menüstruktur in gp, mobile und T-LoRa Pager vereinheitlicht – einheitliche Aufteilung in Network, LoRa (Funkparameter) und Setup (Rufzeichen, Position, Passwort, Chip ID, Neustart). Hardware-spezifische Einstellungen (Display) beim Pager ebenfalls in Setup integriert.
- NEU: rMesh-Logo im Login-Overlay von gp und mobile
- Website: Einheitlicher Header, rMesh-Logo und überarbeitete Navigation auf allen Seiten
- Website: OTA-Webinstaller überarbeitet und responsive gestaltet
- FIX: Firmware-Versionsstring erlaubt jetzt auch Buchstaben als Suffix (z. B. v1.0.25a)

## [v1.0.25a]

- NEU: T-LoRa Pager startet jetzt auch auf Boards ohne PSRAM (ESP32-S3FN8) – blockierende Endlosschleife in LilyGoLib bei fehlendem PSRAM durch Patch entfernt, Display-Buffer-Überlauf (426 KB → 213 KB) behoben
- NEU: T-LoRa Pager Menü – "Tune"-Button sendet ein Tune-Frame direkt aus dem Menü
- NEU: T-LoRa Pager Menü – "About"-Seite zeigt installierte Firmware-Version, WiFi-IP sowie Links zu [www.rMesh.de](https://www.rMesh.de) und GitHub
- NEU: T-LoRa Pager – "Ausschalten" ist jetzt der letzte Menüpunkt und erfordert eine Sicherheitsabfrage (Ja/Nein)
- NEU: T-LoRa Pager – Boot-Splash "rMesh wird gestartet" jetzt größer und zentriert
- FIX: UDP-Fehlerflut (`parsePacket: could not check for data`) wenn kein WLAN eingerichtet ist
- FIX: Topologie-Reporting wurde durch häufige RSSI/SNR-Updates blockiert – Debounce-Timer wird jetzt nur noch bei echten neuen Peers/Routen zurückgesetzt

## [v1.0.25]

- NEU: OTA-Debugging – jeder Update-Vorgang wird in der Datenbank protokolliert. Erfasst werden Versions-Anfragen, gefundene Updates, gestartete Downloads sowie Erfolg oder Misserfolg des Flashens mit Fehlermeldung und Gerätetyp.

## [v1.0.24]

- NEU: Netzwerk-Topologie-Karte auf [www.rMesh.de](https://www.rMesh.de) – Nodes mit Internetzugang melden ihren Namen, ihre Peers (LoRa/UDP) und die Routing-Tabelle stündlich (bzw. bei Änderung mit 30s Debounce) an den Server. Nodes ohne Internet erscheinen über die Berichte ihrer Nachbarn auf der Karte.
- NEU: Einstellungsfeld "Position" (Maidenhead-Locator oder Lat/Lon) in der Firmware, allen Web-UIs und dem T-LoRa Pager Menü.
- FIX: Web-Installer (esp-web-tools) konnte Firmware wegen CORS-Sperre nicht direkt von GitHub laden. Firmware-Binaries werden jetzt serverseitig über firmware.php als Proxy ausgeliefert; manifest.php generiert das Installationsmanifest dynamisch aus dem aktuellen GitHub-Release.
- FIX: Versionsstring-Injektion robuster gemacht – get_version.py schreibt jetzt eine src/version.h (statt CPPDEFINES), die direkt in config.h eingebunden wird. src/version.h ist gitignored (generierte Datei).

## [v1.0.23]

- FIX: WLAN verbindet sich nach Verbindungsabbruch nicht mehr neu (#2 behoben)
- FIX: LILYGO T-LoRa Pager Build korrigiert – fehlende LilyGoLib-Abhängigkeiten ergänzt, NFC-Guards automatisch gepatcht
- FIX: VERSION-Fallback und include-Pfad für Hal.h korrigiert
- GitHub Actions: Firmware wird bei jedem Release-Tag automatisch für alle Boards gebaut
- Versionsstring wird jetzt automatisch aus dem Git-Tag in die Firmware injiziert (kein manuelles Pflegen mehr in config.h)
- Webinstaller komplett auf statisches HTML/JS umgestellt (kein PHP mehr erforderlich)
- Webinstaller lädt Device-Liste, Bilder, Changelog und README direkt von GitHub
- Neue Boards können durch Eintrag in devices.json automatisch auf der Webseite erscheinen
- CHANGELOG.md als einzige Changelog-Quelle (wird automatisch als Release-Body verwendet)
- Bilder des Webinstallers nach website/images/ konsolidiert

## [v1.0.22]

- Neue URL für den Autoupdater

## [v1.0.21]

- Neues Device: T-LoRa Pager

## [v1.0.20a]

- Max. Message json länge 4096 bytes
- Keine Binärdaten mehr in messages und monitor

## [v1.0.19a]

- 3-Block-Layout integriert. Das komplette Interface basiert jetzt auf einem strikten CSS-Flexbox-System (100dvh). Es gibt einen festen Header, einen scrollbaren Mittelteil und einen festen Footer.
- iOS Safari Tastatur-Bug behoben. Die Eingabeleiste wird auf iPhones beim Öffnen der Tastatur nicht mehr weggeschoben oder verdeckt.
- ENTFERNT: Cookie-Speicher (document.cookie). Das fehleranfällige und auf 4 KB limitierte Speichern der Einstellungen per Cookie wurde restlos gelöscht.
- NEU: LocalStorage Integration. Die guiSettings (inklusive aller Chats und UI-Zustände) werden jetzt im modernen HTML5 localStorage des Browsers abgelegt. Dadurch hast du nun bis zu 10 Megabyte Speicherplatz, der das Netzwerk (ESP32) nicht belastet.

## [v1.0.18-a]

- Safari/iOS fixes
- ...weil es so schön ist....

## [v1.0.17-a]

- Safari/iOS fixes

## [v1.0.16-a]

- Safari/iOS fixes

## [v1.0.15-a]

- FIX: Safari/iOS Input-Bar Interactivity

## [v1.0.14-a]

- Die messages.json wird jetzt mit entsprechenden HTTP-Headern (no-cache) ausgeliefert. Verhindert, dass der Browser veraltete Nachrichten-Stände aus dem Cache lädt, anstatt die aktuelle Datei vom ESP32 abzurufen.
- Anpassung der Input-Bar Logik (CSS/JS). Fokus auf die Behebung von Darstellungsfehlern und Fokus-Problemen unter iOS (Safari). Status: Experimentell / Ungetestet.
- Der Hopcount wurde hart auf maximal 15 begrenzt.

## [v1.0.13-a]

- Der Seitentitel zeigt nun ein Nachrichtensymbol an, sobald neue Mitteilungen eingegangen sind.
- Die Scrollbars wurden optisch an das rMesh-Design angepasst.
- Rückkehr zum „harten Scrolling" für eine direktere und präzisere Navigation in langen Chat-Verläufen.
- Erweiterte Emoji-Palette für Meshtastic: Unterstützung für 🤮 (Kotzen) und 🤦 (Facepalm) hinzugefügt – für die Momente, in denen Worte nicht mehr ausreichen.
- Mute-Funktion: Einzelne Gruppen können nun stummgeschaltet werden, um die Benachrichtigungsflut in aktiven Kanälen zu bändigen.
- Parallele Quittierung (Multi-Path ACKs): Bestätigungen (ACKs) werden nun immer zeitgleich über WLAN (UDP) und LoRa versendet. Dies minimiert unnötige Retransmissions und erhöht die Zuverlässigkeit im Hybrid-Betrieb massiv.
- Angepasstes Announce-Timing: Das Intervall für Knoten-Ankündigungen wurde zur Schonung der Airtime auf 10 Minuten gesetzt.

## [v1.0.12-alpha]

- Optimierung der messages.json durch zeitgesteuertes Trimmen. Der erste Bereinigungszyklus startet nun 30 Minuten nach Systemstart, um die Boot-Phase nicht zu belasten. Danach erfolgt die Wartung automatisch in einem 24-Stunden-Intervall.
- Ungelesen-Markierung für den "All"-Gruppe.
- Kein akustisches Signal bei "All"-Gruppe.
- Automatisches Entfernen von führenden oder abschließenden Leerzeichen (Trim) bei der Eingabe von Rufzeichen und Gruppennamen.
- Beschleunigter Nachrichten-Display: Nachrichten aus der messages.json werden nun unmittelbar während des Ladevorgangs gerendert, was die wahrgenommene Ladezeit bei großen Archiven deutlich reduziert.

## [v1.0.11-alpha]

- LittleFS ist nicht thread-safe 🤮
- Mutex für Webserver
- messages.json: Nur noch Append (weil Längenbegrenzung bis zu 30Sek. dauert und dann der Webserver blockiert)
- Längenbegrenzung der messages.json -> als Task Nachts um 3:00
- Mobile GUI: Titel sollte besser passen

## [v1.0.10-alpha]

- Warten auf ACK bissel länger
- Mobile GUI
- Routing Tabelle nur noch kürzeste Route
- Bei "ALL" keine Geräusche mehr

## [v1.0.9-alpha]

- Nochmal große JSON Strings

## [v1.0.8-alpha]

- Nochmal große JSON Strings

## [v1.0.7-alpha]

- Große JSON Strings (PeerList und RoutungList) werden direkt in Websocket Puffer geschrieben
- Monitor Daten auch

## [v1.0.6-alpha]

- "erweiterte Einstellungen" -> WLAN Einstellungen bleiben bei FW-Update erhalten, wenn "erweiterte Einstellungen" geändert werden
- wifiBordcast ist jetzt UDP-Peer (maximal 5 IPs)
- Viele ACKs wieder weg (auf Stand von V1.0.4)
- Hoffentlich alle Rufzeichen UTF-8 sicher im Websocket
- Fehler in Routing Liste beseitigt (falsche Nodes, die nicht in Peer Liste sind)
- messages.json kann über GUI gelöscht werden
- Routing für Nachrichten mit dstCall aktiv

## [v1.0.5-alpha]

- Frames aus dem TX-Puffer löschen, wenn man merkt, dass ein anderes Node den Frame schon wiederholt.
- ACKs werden jetzt immer gesendet
- GUI: Datum in Peer Liste
- Routing Liste wird angezeigt, aber noch nicht verwendet

## [v1.0.4-alpha]

- Timing um ca. 25% verlangsamt
- Update Prüfung alle 24h
- ohne gesetztes Rufzeichen kein Senden möglich
- Default Rufzeichen = ""
- Nachrichten mit Länge = 0 werden nicht gesendet
- Keine Frames an srcCall repeaten
- Beim setzen von SSID oder PW über UART wird AP-Mode abgeschaltet
- Frames ohne nodeCall werden ignoriert
- ACK-Liste jetzt im RAM und nicht mehr im Flash
- Prüfung auf neue Nachrichten jetzt im RAM und nicht mehr im Flash
- "messages.json" wird als Task geschrieben
- Wenn ein anderes Node anfängt eine Nachricht zu repeaten, wird die Nachricht aus dem Sendepuffer gelöscht
- Timing für UDP wieder schneller
- Peer-List wird nur über Websocket gesendet, wenn auch wirklich geändert
- GUI: Hinweis im Fenstertitel, wenn neue Nachrichten
- GUI kann jetzt Messageboxen

## [v1.0.3-alpha]

- Timing für UDP langsamer
- Speichern Button hat QRG nicht übernommen
- HELTEC_WiFi_LoRa_32_V4
- WLAN AP Bandbreite 20MHz
- Keine einmalige Wiederholung von Frames, wenn niemand in der Peer Liste
- Bei direkt Adressierten Nachrichten landet der Absender in der Peer Liste
- getTOA gefixt. Hat fast die doppelte Zeit ausgegeben
- Dynamisches Timing
- Announce Timer in V1.0.2 war denke falsch

## [v1.0.2-alpha]

- mehr Hardware
- direkte Nachrichten
- Gruppen
- GUI: ungelesene Kanäle gelb
- Ton bei neuen Nachrichten

## [v1.0.1-alpha]

- erstes alpha Release