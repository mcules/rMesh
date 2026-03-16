# Changelog

## [v1.0.23]
- FIX: WLAN verbindet sich nach Verbindungsabbruch nicht mehr neu (#2 behoben)
- FIX: LILYGO T-LoRa Pager Build korrigiert – fehlende LilyGoLib-Abhängigkeiten ergänzt, NFC-Guards automatisch gepatcht
- FIX: Autoupdater verwendet jetzt HTTPS statt HTTP (www.rMesh.de)
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
