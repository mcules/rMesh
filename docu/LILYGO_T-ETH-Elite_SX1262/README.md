# LilyGO T-ETH-Elite ESP32-S3 + SX1262 868 MHz LoRa Shield

## Hardware

- **MCU**: ESP32-S3 (16 MB Flash, 8 MB PSRAM OPI)
- **Ethernet**: W5500 (SPI)
- **LoRa**: SX1262 868 MHz via aufsteckbarem Shield
- **SD-Card**: MicroSD (shared SPI mit W5500)
- **USB**: USB-C (CDC on boot)
- **LED**: Keine user-steuerbare LED vorhanden
- **Button**: BOOT (GPIO 0, active-low)

## Pin-Mapping

Quelle: [LilyGO T-ETH-Elite-LoRa-Shield/utilities.h](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/blob/master/examples/T-ETH-ELite-Shield/T-ETH-Elite-LoRa-Shield/utilities.h)

### SX1262 LoRa Shield

| Funktion | GPIO |
|----------|------|
| SCK      | 10   |
| MISO     | 9    |
| MOSI     | 11   |
| CS/NSS   | 40   |
| RST      | 46   |
| DIO1/IRQ | 8    |
| BUSY     | 16   |

### W5500 Ethernet

| Funktion | GPIO |
|----------|------|
| SCK      | 48   |
| MISO     | 47   |
| MOSI     | 21   |
| CS       | 45   |
| INT      | 14   |
| RST      | -1 (kein HW-Reset) |
| PHY_ADDR | 1    |

### SD-Card

| Funktion | GPIO |
|----------|------|
| CS       | 12   |

## SPI-Bus-Zuordnung

Das Board braucht **zwei getrennte SPI-Busse**, da LoRa-Shield und W5500 unterschiedliche Pins nutzen:

| SPI-Host | Verwendung | Pins (SCK/MISO/MOSI) |
|----------|------------|----------------------|
| HSPI (SPI3_HOST) | LoRa SX1262 | 10 / 9 / 11 |
| FSPI (SPI2_HOST) | W5500 + SD  | 48 / 47 / 21 |

**Achtung**: Die erste Firmware-Version hatte LoRa- und ETH-Pins vertauscht.
Beide SPI-Fehler (`radio.begin() failed code -2` + `w5500 send_command timeout`)
waren die Folge davon. Symptom: Endlosschleife von W5500-Timeouts im Log.

## Build

```
pio run -e LILYGO_T-ETH-Elite_SX1262
```

Build-Flags: `-DLILYGO_T_ETH_ELITE_SX1262 -DHAS_WIFI -DHAS_ETHERNET -DBOARD_HAS_PSRAM`

Partition: `partitions_16MB.csv` (3 MB app0, 3 MB app1, ~10 MB LittleFS)

## Netzwerk-Interfaces

Das Board hat drei Kommunikationswege:

| Interface | Port | Beschreibung |
|-----------|------|--------------|
| LoRa      | 0    | SX1262 868 MHz RF (Mesh) |
| WiFi      | 1    | ESP32-S3 WLAN (AP oder Client) |
| LAN       | 2    | W5500 Ethernet (kabelgebunden) |

### Dual-Interface-Betrieb (WiFi + LAN)

- Beide IP-Interfaces können gleichzeitig aktiv sein (verschiedene Subnetze).
- ANNOUNCE-Beacons werden auf allen aktiven Interfaces gesendet.
- UDP-Peers werden per Subnet dem richtigen Interface zugeordnet.
- Pro Interface einzeln steuerbar: Node-Kommunikation und WebUI an/aus.
- **Primäres Interface** bestimmt die Default-Route für ausgehenden Traffic (OTA, NTP).

### Sende-Reihenfolge

Netzwerk-Interfaces werden immer **vor** LoRa bedient. Innerhalb der Netzwerk-Interfaces
wird das als primär konfigurierte Interface zuerst angesprochen.

```
Primäres Interface → Sekundäres Interface → LoRa
```

## LoRa-Shield-Varianten

Das T-ETH-Elite unterstützt verschiedene aufsteckbare LoRa-Shields.
Jede Variante braucht ein eigenes Firmware-Target, da die Radio-Chips
unterschiedliche Treiber und Pin-Belegungen verwenden:

| Shield | RadioLib-Klasse | Besonderheit | Firmware-Target |
|--------|----------------|--------------|-----------------|
| **SX1262** | `SX1262` | BUSY-Pin, DIO1 IRQ | `LILYGO_T-ETH-Elite_SX1262` |
| SX1276/78 | `SX1276` | DIO0/DIO1 IRQ, kein BUSY | *nicht implementiert* |
| LR1121 | `LR1121` | Multi-Band, eigene API | *nicht implementiert* |

## Bekannte Eigenheiten

- Kein Hardware-Reset-Pin für den W5500 (`ETH_RST_PIN = -1`).
- SD-Card und W5500 teilen sich denselben SPI-Bus; Zugriff muss ggf. per Mutex serialisiert werden sobald SD genutzt wird.
- USB-CDC muss aktiv sein (`ARDUINO_USB_CDC_ON_BOOT=1`), sonst kein Serial-Output.
- LoRa-Shield und ETH nutzen getrennte SPI-Busse (HSPI vs FSPI). SPI-Datenpins (SCK/MISO/MOSI) sind niedrig (9-11), aber CS/RST/IRQ/BUSY liegen auf höheren GPIOs (8, 16, 40, 46).
