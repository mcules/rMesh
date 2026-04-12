# rMesh – Button & BT-Manager: Feature-Übergabe

## Ziel

Integration zweier neuer Features in rMesh (`dev-next`):

1. BT-Modus-Verwaltung mit dynamischer WiFi-Koexistenz
2. Erweitertes Button-Konzept für Hardware mit einem einzelnen Button

---

## Geltungsbereich

Dieses Feature gilt für alle rMesh-kompatiblen Boards, die über mindestens einen Button sowie BT- oder WiFi-Unterstützung verfügen. Die Implementierung ist board-agnostisch. GPIO-Belegungen werden board-spezifisch über eine zentrale Pin-Konfigurationsdatei per `#ifdef` definiert.

---

## BT-Modi (NVS-persistent)

| Modus | Verhalten |
|---|---|
| `BT_MODE_OFF` | BT vollständig deaktiviert |
| `BT_MODE_COEX` | WiFi und BLE gleichzeitig aktiv, Priorität dynamisch über `esp_coex_wifi_bt_set_priority()` gesteuert |
| `BT_MODE_EXCLUSIVE` | WiFi wird bei BT-Verbindungsaufbau über `esp_wifi_stop()` pausiert und nach Verbindungsabbruch über `esp_wifi_start()` + Reconnect wiederhergestellt |

**Standard:** `BT_MODE_OFF` für reine Mesh-Nodes.

---

## Button-Verhalten

| Geste | Aktion |
|---|---|
| Einzelklick | Status anzeigen (Serial + LED-Feedback + Display falls vorhanden) |
| Doppelklick | BT-Modus cycling: `OFF → COEX → EXCLUSIVE → OFF` |
| Langer Druck (3 s) | WiFi AP ↔ Client wechseln (bestehendes Verhalten) |
| Sehr langer Druck (8 s) | Factory Reset |

---

## LED-Feedback

| Ereignis | LED |
|---|---|
| Status abfragen | 1x kurz |
| BT-Modus geändert | 3x schnell |
| WiFi-Modus geändert | 1x lang |
| Factory Reset Warnung | Dauerhaftes schnelles Blinken |

Boards mit Display zeigen den aktuellen Modus zusätzlich im Display an.

---

## Neue Dateien

- `bt_manager.c` / `bt_manager.h` – BT-Modus-Logik, Connect/Disconnect-Callbacks, NVS-Persistenz
- `button_manager.c` / `button_manager.h` – Einfach-/Doppel-/Long-Press-Erkennung, Aktions-Dispatch

---

## Anzupassende Dateien

- WiFi-Wechsel-Logik aus dem bisherigen Long-Press-Handler in `button_manager.c` überführen
- NVS-Schlüssel `bt_mode` in die bestehende Konfigurationsstruktur einfügen
- Board-spezifische GPIO-Defines in der zentralen Pin-Konfigurationsdatei ergänzen

---

## Konfiguration ohne Button

Boards ohne physischen Button können alle Modi über das WebUI einstellen. Die Einstellungen werden identisch per NVS persistiert. Das WebUI muss entsprechend um einen Abschnitt für BT-Modus und WiFi-Modus erweitert werden.

Die Button-Logik und die WebUI-Logik teilen sich dieselben Handler-Funktionen – es gibt keine doppelte Implementierung.