## rMesh

**rMesh** ist ein schlankes Messenger-Protokoll für LoRa-Netzwerke mit einem klaren Ziel: **Textnachrichten sicher und zuverlässig über Funk zustellen – so effizient wie möglich.**

## Die Philosophie: "Pure Messaging"

Viele Mesh-Projekte wachsen mit der Zeit zu komplexen Allzweck-Plattformen heran. rMesh geht bewusst den anderen Weg: **Weniger ist mehr.**

Alles, was nicht direkt zur Übermittlung einer Nachricht beiträgt, gehört nicht in den Funkkanal. Deshalb überträgt rMesh über das LoRa-Protokoll konsequent keine:

* **Positionen:** GPS-Koordinaten haben im Funkkanal eines reinen Messengers nichts zu suchen.
* **Telemetrie:** Batteriespannungen, RSSI-Graphen oder Temperaturen bleiben aus dem HF-Kanal heraus.
* **Fernsteuerung:** Keine Remote-Befehle über Funk, die wertvolle Bandbreite verbrauchen.
* **Debug-Daten:** Logs gehören an den USB-Port, nicht in die Luft.
* **Unnötiger Routing-Overhead:** Nur das absolute Minimum an Headern, um ein Paket von A nach B zu bringen.

Das Ergebnis ist ein System, das sich auf das Wesentliche konzentriert – und genau deshalb besonders gut darin ist: **Nachrichten durchzubringen, auch wenn andere Verbindungen längst versagen.**

## Anforderungen
* ESP32 / ESP8266
* LoRa-Modul (oder vergleichbare HF-Hardware)

## Installation / Webflasher

Der einfachste Weg, rMesh auf deine Hardware zu bringen: Einfach den **Webflasher** aufrufen, Gerät per USB anschließen und auf "Installieren" klicken – ganz ohne Entwicklungsumgebung oder Kommandozeile.

**[Zum rMesh Webflasher](https://www.rmesh.de/#installer)**

## Lizenz: GNU GPLv3
**rMesh** ist freie Software, lizenziert unter der **GNU General Public License v3.0 (GPLv3)**.
