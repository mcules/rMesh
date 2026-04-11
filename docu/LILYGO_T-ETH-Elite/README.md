# LilyGO T-ETH-Elite ESP32-S3 + 868 MHz LoRa Shield

## Hardware

- **MCU**: ESP32-S3 (16 MB Flash, 8 MB PSRAM OPI)
- **Ethernet**: W5500 (SPI)
- **LoRa**: SX1262 868 MHz via aufsteckbarem Shield
- **SD-Card**: MicroSD (shared SPI mit W5500)
- **USB**: USB-C (CDC on boot)
- **LED**: Keine user-steuerbare LED vorhanden
- **Button**: BOOT (GPIO 0, active-low)

## Pin-Mapping

Quelle: [LilyGO-T-ETH-Series/boards.h](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)

### SX1262 LoRa Shield

| Funktion | GPIO |
|----------|------|
| SCK      | 10   |
| MISO     | 9    |
| MOSI     | 11   |
| CS/NSS   | 12   |
| RST      | 8    |
| DIO1/IRQ | 13   |
| BUSY     | 3    |

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

### SD-Card (shared SPI mit W5500)

| Funktion | GPIO |
|----------|------|
| SCK      | 48   |
| MISO     | 47   |
| MOSI     | 21   |
| CS       | 41   |

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
pio run -e LILYGO_T-ETH-Elite
```

Build-Flags: `-DLILYGO_T_ETH_ELITE -DHAS_WIFI -DHAS_ETHERNET -DBOARD_HAS_PSRAM`

Partition: `partitions_16MB.csv` (3 MB app0, 3 MB app1, ~10 MB LittleFS)

## Bekannte Eigenheiten

- Kein Hardware-Reset-Pin für den W5500 (`ETH_RST_PIN = -1`).
- SD-Card und W5500 teilen sich denselben SPI-Bus; Zugriff muss ggf. per Mutex serialisiert werden sobald SD genutzt wird.
- USB-CDC muss aktiv sein (`ARDUINO_USB_CDC_ON_BOOT=1`), sonst kein Serial-Output.
