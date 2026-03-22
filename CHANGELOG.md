# Changelog

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
