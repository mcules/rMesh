# Changelog

## [v1.0.31-dev]

### Neue Hardware
- NEU: Heltec HT-Tracker V1.2 (Wireless Tracker) Support – ESP32-S3 + SX1262 + ST7735 TFT-Farbdisplay (160x80, Landscape); zeigt Callsign, Akkustand, WiFi-Modus, IP, SSID und letzte Nachricht in Farbe; Display-Treiber via LovyanGFX; Boot-Button (GPIO 0) für Display-Toggle (kurz) und AP/Client-Umschaltung (lang ≥2 s)

### Flash-Persistenz
- NEU: Routing-Tabelle wird im Flash gespeichert (`/routes.bin`) – nach Reboot sofort routingfähig ohne Route-Discovery; Kapazität von 100 auf 1000 Routen erhöht
- NEU: Peer-Liste wird im Flash gespeichert (`/peers.bin`) – bekannte Peers sind nach Reboot sofort verfügbar mit 2-Minuten Grace-Timeout
- NEU: Intelligente Speicher-Trigger – Flash-Write nur bei strukturellen Änderungen (neuer Peer/Route, Hop-Wechsel, Peer-Removal), nicht bei RSSI/SNR-Updates; periodischer 5-Minuten-Save-Timer für Dirty-Flags

### Web UI
- NEU: Uptime-Anzeige im About-Panel – zeigt die Laufzeit seit letztem Reboot (Tage, Stunden, Minuten, Sekunden), wird sekündlich aktualisiert

### Test Framework
- NEU: Automatisierte Hardware-in-the-Loop Test Suite (`test/`) mit pytest + pyserial – flasht Firmware, konfiguriert Nodes und testet Funktionalität und Kommunikation
- NEU: Node-Konfiguration via YAML-Datei (`nodes.yaml`) – Board-Typ, COM-Port, Callsign, Frequenz-Preset, optional WiFi; beliebig viele Nodes
- NEU: Serial Debug-Modus (`dbg 1/0`) – strukturierte JSON-Events über Serial für RX, TX, ACK, Peer-Änderungen und Boot-Ready
- NEU: Serial-Befehle für Tests – `msg`, `xgrp`, `xtrace`, `announce`, `peers`, `routes`, `acks`, `xtxbuf`
- NEU: Automatischer Build, Flash, Board-Verifikation und Reboot vor den Tests; `--no-flash` zum Überspringen
- NEU: Peer Discovery vor Teststart – gegenseitige Announces stellen sicher, dass Nodes sich kennen
- NEU: Tests für Einzel-Node (Boot, Settings, LoRa-Parameter), Messaging (Direct, Group, Trace, ACK), Peer Discovery und Routing
- FIX: TX-Buffer Cleanup löschte versehentlich wartende Frames – One-Shot-Frames (ACKs, retry=1) entfernen beim Aufräumen nicht mehr andere Frames zum selben Peer; behebt fehlende Trace-Echos und potenziell verlorene Nachrichten

### Stabilität & Effizienz
- FIX: WiFi/UDP-Nachrichten an fremde Callsigns wurden an alle Nodes angezeigt – private Nachrichten mit `dstCall` ungleich dem eigenen Call werden jetzt nur noch weitergeleitet (Relay), aber nicht mehr in der WebUI angezeigt oder gespeichert (#9)
- FIX: Guru Meditation Error (StoreProhibited) bei DNS-Fehler behoben – `http.begin()`-Rückgabewert wird jetzt in Topology-Reporting und Update-Check geprüft; WiFi-Status-Guard in `checkForUpdates()` verhindert Aufrufe ohne Verbindung; OTA-Log-Timeout auf 5 s begrenzt
- FIX: Sichere malloc/new-Prüfungen – alle Heap-Allokationen in `helperFunctions.cpp`, `main.cpp` und `settings.cpp` werden auf `nullptr` geprüft; bei Fehlschlag wird sauber abgebrochen und ein `[OOM]`-Log geschrieben statt Absturz
- FIX: Doppelte Nachrichten (Duplikate) werden jetzt sofort aus dem TX-Buffer entfernt, wenn die Deduplikation im Ring-Buffer einen bereits bekannten Frame erkennt – verhindert unnötige Sendewiederholungen
- FIX: TX-Buffer-Verstopfung bei unerreichbarem Peer behoben – wenn alle Retries für einen Peer aufgebraucht sind, werden jetzt alle weiteren Frames an diesen viaCall aus dem txBuffer entfernt (nicht nur der aktuelle Frame)
- FIX: Overflow-sichere Timer – alle `millis()`-Vergleiche nutzen jetzt `timerExpired()` mit signed-Arithmetik; verhindert Timer-Ausfälle nach ~49 Tagen Uptime
- FIX: Reboot-Timer Race Condition behoben – neues `rebootRequested`-Flag verhindert unbeabsichtigten Sofort-Reboot bei `millis()`-Overflow
- FIX: Buffer-Overflow in `Frame::exportBinary()` – Bounds-Checking vor jedem Header-Write verhindert Schreibzugriffe über den Puffer hinaus
- FIX: TRACE-Echo Buffer-Overflow – Bounds-Prüfung beim Zusammenbauen der TRACE-Nachricht; TRACE-Pfad wird nur einmal angehängt statt pro Port-Iteration
- FIX: `messageLength` in `sendMessage()`/`sendGroup()` wird jetzt nach `safeUtf8Copy()` gemessen – vorher konnte die Länge größer als der tatsächlich kopierte Inhalt sein
- FIX: Auth-Session Eviction – wenn alle Slots voll sind, wird der erste unauthentifizierte Slot verdrängt statt den neuen Client stillschweigend abzuweisen; Hash-Längen-Validierung in `verifyAuthResponse()`
- FIX: File-Handle-Leak in `trimFileTask` – Dateien werden jetzt auch im Fehlerfall korrekt geschlossen
- FIX: `getTOA()` Parametertyp von `uint8_t` auf `uint16_t` erweitert – verhindert stille Abschneidung bei Frames mit Payload + Header > 255 Bytes
- NEU: Flux Guard – nach jeder LoRa-Sendung wird eine Pause von 1× ACK-TOA eingehalten, damit Empfänger sicher in den RX-Modus zurückkehren können; verbessert die effektive Reichweite
- NEU: Duty-Cycle-Enforcement für das öffentliche SRD-Band (869,4–869,65 MHz) – rollendes 60-s-Fenster mit max. 10 % Sendezeit; Frames werden bei erschöpftem Budget um 5 s verschoben statt verworfen
- NEU: Kapazitätslimits für Peer-Liste, Routing-Tabelle und UDP-Peer-Liste – verhindert unkontrolliertes Wachstum bei vielen Nodes im Mesh
- CHANGE: ACK-Timing auf 20× ACK-TOA erhöht (vorher 15×) – größeres Zeitfenster reduziert Kollisionen bei vielen Peers
- CHANGE: Retry-Timing auf 20× ACK-TOA + 0–5× max. Frame-TOA angepasst (vorher 10× ACK + 0–6× Frame) – besser abgestimmt auf reale Mesh-Topologien
- NEU: Konfigurierbarer minimaler SNR-Schwellwert für die Peer-Liste – LoRa-Peers unterhalb des eingestellten SNR-Werts werden automatisch als nicht verfügbar markiert; einstellbar in den LoRa-Einstellungen der WebUI (Aus, -20 bis +10 dB); Default: deaktiviert (-30 dB)
- CLEANUP: Doppelte `#include`- und `esp_log_level_set`-Einträge entfernt; doppelte `measureJson()`/`strlen()`-Aufrufe vermieden

### OLED Status-Display
- NEU: SSD1306-OLED-Support für HELTEC WiFi LoRa 32 V3, LILYGO T3 LoRa32 V1.6.1 und LILYGO T-Beam – Display zeigt Callsign, Akkustand, WiFi-Modus, IP-Adresse, SSID und letzte empfangene Nachricht
- NEU: Jedes Board hat eigene Display-HAL-Dateien – `display_LILYGO_T-Beam.cpp`, `display_LILYGO_T3_LoRa32_V1_6_1.cpp`, `display_HELTEC_WiFi_LoRa_32_V3.cpp` statt einer gemeinsamen Datei; vereinfacht Board-spezifische Anpassungen
- FIX: T-Beam Display-Treiber auf ThingPulse SSD1306Wire-Library umgestellt – behebt falsche Proportionen und Pixel-Artefakte die mit U8g2 auftraten; `flipScreenVertically()` korrigiert die Bildausrichtung
- FIX: T-Beam OLED-Reset-Pin entfernt – GPIO 16 ist am T-Beam nicht mit dem OLED verbunden (kein Hardware-Reset vorhanden)
- FIX: T-Beam User-Button von GPIO 0 (Boot) auf GPIO 38 (User-Button) umgestellt – Display-Toggle und WiFi-Umschaltung funktionieren jetzt über den mittleren Taster
- NEU: Boot-Button-Steuerung – kurzer Druck schaltet Display ein/aus, langer Druck (≥2 s) wechselt zwischen AP- und Client-Modus mit anschließendem Reboot
- NEU: Display-Einstellung persistent – Zustand überlebt Neustart; Synchronisation zwischen Hardware-Button und WebUI-Toggle in beide Richtungen
- NEU: Nachrichten-Gruppe für Display konfigurierbar – Dropdown in den Settings mit allen eingerichteten Gruppen (all, direct, Gruppen 3–10); letzte Nachricht aus gewählter Gruppe wird auf dem Display angezeigt
- NEU: Automatische Display-Erkennung per I2C-Probe beim T-Beam – wenn kein Display angeschlossen ist, wird die Funktionalität deaktiviert
- NEU: Vext-Steuerung (GPIO 36) für HELTEC V3 – OLED-Stromversorgung wird automatisch aktiviert

### Web UI
- NEU: Mobile und Desktop UI in ein einziges responsives Interface zusammengeführt
- NEU: Mehrsprachigkeit (i18n) mit Sprachumschaltung Deutsch/Englisch über `i18n`
- NEU: Digitale Uhr als Widget, altes Debug-Panel entfernt
- NEU: Einheitliches Stylesheet
- NEU: SVG-Icons (`announce.svg`, `logo.svg`) ersetzen PNG-Versionen
- NEU: Einklappbare Settings-Bereiche und verbesserte Tabellen-Layouts
- FIX: Eingabefeld wird nach dem Senden automatisch geleert
- CLEANUP: Obsolete Dateien entfernt

### WiFi & Netzwerk
- NEU: mDNS-Support – Node ist per `<callsign>-rmesh.local` im lokalen Netzwerk erreichbar (inkl. HTTP Service-Discovery)
- NEU: Erweiterte WiFi- und AP-Verwaltung mit neuen seriellen Befehlen
- NEU: Verbesserte WiFi-Client/AP-Tabelle in der WebUI
- NEU: WebSocket-Kommunikation überarbeitet

### System
- NEU: Erweitertes serielles Kommando-Interface
- NEU: Erweiterte Settings-Verwaltung

## [v1.0.30a-dev]

- FIX: OTA-Update schlug auf langsamen Verbindungen mit „HTTP error: read Timeout" fehl – TCP-Read-Timeout für LittleFS- und Firmware-Download von 30 s auf 120 s erhöht; betrifft sowohl automatische als auch manuelle Updates

## [v1.0.30-dev]

- NEU: Support für Seeed XIAO ESP32-S3 + Wio-SX1262 – neues HAL (`hal_SEEED_XIAO_ESP32S3_Wio_SX1262`) für das Seeed XIAO ESP32-S3 Board mit aufgestecktem Wio-SX1262 LoRa-Modul (B2B-Stecker); Build-Konfiguration in PlatformIO, Eintrag in `devices.json` für das Web-Flash-Tool
- NEU: Manueller Firmware-Upload über die WebUI – neuer `/ota`-Endpunkt im Webserver zum direkten Flashen eigener Firmware- und LittleFS-Binaries ohne OTA-Server; Desktop- und Mobile-Interface erhalten einen „Upload & Flash"-Button, der beide Dateien sequenziell hochlädt und die Node danach neu startet
- NEU: Akkustand-Anzeige für HELTEC WiFi LoRa 32 V3 und Wireless Stick Lite V3 – Spannung wird per ADC (GPIO1, VBAT_CTRL) mit 8-Sample-Mittelung gemessen; in der WebUI (Desktop & Mobile) als Akkubalken angezeigt; aktivierbar/deaktivierbar in den Einstellungen; Referenzspannung (Default 4,2 V) konfigurierbar
- NEU: Zweistufige Peer-Inaktivität – Peers werden nach 25 Minuten ohne Lebenszeichen zunächst als nicht verfügbar markiert (kein Routing mehr über diesen Peer), aber erst nach 60 Minuten vollständig aus der Liste entfernt; verhindert abrupte Routing-Ausfälle bei kurz nicht erreichbaren Nodes
- FIX: Peer-Timestamps nutzen jetzt `time()` (Unix-Sekunden) statt Millisekunden; `availablePeerList()` aktualisiert den Timestamp beim Reaktivieren eines Peers korrekt; `addPeerList()` verwendet `time(NULL)` statt `f.timestamp` für konsistente Wanduhr-Zeitstempel
- NEU: Toast-Benachrichtigungssystem in der Desktop-WebUI – Statusmeldungen und Aktions-Feedback werden als animierte Toast-Einblendungen angezeigt (Ein- und Ausblend-Animation, automatisches Ausblenden)
- NEU: Support für ESP32 E22 LoRa Multimodul V1 – neues HAL (`hal_ESP32_E22_V1`) für Eigenbauplatine mit ESP32 und E22 LoRa-Modul (SX1262); Build-Konfiguration in PlatformIO (`env:ESP32_E22_V1`), Eintrag in `devices.json` für Web-Flash-Tool; `-Os` Optimierungsflag für kompaktere Firmware
- DOKU: Technische Dokumentation für alle unterstützten Boards neu strukturiert – Verzeichnis `Doku/` nach `docu/` umbenannt (einheitlich englisch); Datenblätter und Schaltpläne für HELTEC WiFi LoRa 32 V3/V4, Wireless Stick Lite V3, LILYGO T-Beam und T3 ergänzt; ESP32 E22 Multimodul-Dokumentation (Schaltplan, Bestückungsplan, Gehäuse-3MF-Dateien) hinzugefügt
- CLEANUP: `build.bat` entfernt, ungenutztes LilyGoLib-ThirdParty-Submodul entfernt, PlatformIO-Boilerplate-README-Platzhalter entfernt

## [v1.0.29e-dev]

- FIX: Serielle Konsole – `h`-Befehl (Hilfe) zeigte seit v1.0.29b keine Ausgabe mehr – `help.txt` wurde durch den Filesystem-Build per gzip komprimiert (`.txt` in `COMPRESS_EXTENSIONS`) und lag im LittleFS nur noch als `help.txt.gz`; der Code öffnete aber `/help.txt` – Datei wurde nicht gefunden, keine Ausgabe; `.txt` aus den komprimierten Erweiterungen entfernt, `help.txt` liegt jetzt wieder unkomprimiert im LittleFS

## [v1.0.29d-dev]

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
