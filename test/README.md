# rMesh Device Test Suite

Automated hardware-in-the-loop tests for rMesh nodes. Connects to physical nodes via USB/Serial, flashes the firmware, configures the nodes, and tests functionality and communication.

## Prerequisites

- Python 3.11+
- PlatformIO CLI (`pio`) in PATH
- At least 1 rMesh node connected via USB

## Setup

```bash
pip install -r test/requirements.txt
```

## Node Configuration

All nodes are configured in a YAML file. Per node:

| Field | Description | Example |
|-------|-------------|---------|
| `name` | Unique name (freely chosen) | `node_a` |
| `board` | PlatformIO environment name | `HELTEC_WiFi_LoRa_32_V3` |
| `port` | Serial port | `COM8` / `/dev/ttyUSB0` |
| `call` | Callsign for the test | `TEST-1` |
| `preset` | Frequency preset (`433` or `868`) | `"868"` |
| `wifi` | *(optional)* WiFi configuration | see below |
| `udp_peers` | *(optional, planned)* UDP peer IPs | `["192.168.1.100"]` |

### Example: `nodes.yaml`

```yaml
nodes:
  - name: node_a
    board: SEEED_XIAO_ESP32S3_Wio_SX1262
    port: COM12
    call: TEST-1
    preset: "868"

  - name: node_b
    board: HELTEC_WiFi_LoRa_32_V3
    port: COM8
    call: TEST-2
    preset: "868"
```

### Enabling WiFi

Without WiFi configuration, WiFi is disabled on the node (`wifi clear` + AP mode off). This prevents NTP sync after boot from abruptly changing the system time, which would trigger peer timeouts. If needed:

```yaml
  - name: node_a
    board: HELTEC_WiFi_LoRa_32_V3
    port: COM8
    call: TEST-1
    preset: "868"
    wifi:
      ssid: MyNetwork
      password: secret
```

### Supported Boards

All PlatformIO environments from `platformio.ini`, including:

- `HELTEC_WiFi_LoRa_32_V3`
- `HELTEC_WiFi_LoRa_32_V4`
- `HELTEC_Wireless_Stick_Lite_V3`
- `SEEED_XIAO_ESP32S3_Wio_SX1262`
- `SEEED_SenseCAP_Indicator`
- `LILYGO_T3_LoRa32_V1_6_1`
- `LILYGO_T_Beam`
- `LILYGO_T_LoraPager`
- `ESP32_E22_V1`

## Running Tests

### All Tests (with automatic flashing)

```bash
pytest test/ --config test/nodes.yaml -v
```

### Without Flashing (firmware already loaded)

```bash
pytest test/ --config test/nodes.yaml --no-flash -v
```

### Running a Single Test

```bash
pytest test/test_node_basic.py --config test/nodes.yaml --no-flash -v
```

### Single Test Class or Test Method

```bash
pytest test/test_messaging.py::TestDirectMessage --config test/nodes.yaml --no-flash -v
pytest test/test_messaging.py::TestTrace::test_trace --config test/nodes.yaml --no-flash -v
```

### With Serial Debug Output

```bash
pytest test/ --config test/nodes.yaml --no-flash -v -s
```

## Workflow

On startup, the following happens automatically:

1. **Build** -- Firmware is built once per board type
2. **Flash** -- Firmware is uploaded to each node
3. **Ready** -- Wait for boot-ready from each node (no ready event triggers automatic reboot)
4. **Board Check** -- Board name from the ready event is compared with `nodes.yaml`. **On mismatch, the run is aborted** (e.g. wrong firmware flashed or wrong port in the config)
5. **Configuration** -- Callsign, frequency preset, WiFi are set
6. **Reboot** -- Nodes are restarted so all settings (including LoRa radio) take effect cleanly
7. **Peer Discovery** -- Mutual announces before tests
8. **Tests** -- All test files are executed

With `--no-flash`, steps 1-2 are skipped. The board check still takes place.

## Test Files

| File | Min. Nodes | Description |
|------|-----------|-------------|
| `test_node_basic.py` | 1 | Boot, version, settings, callsign, LoRa parameters |
| `test_messaging.py` | 2 | Direct message, group, trace, ACK, timing |
| `test_peers.py` | 2 | Peer discovery, announce, signal quality |
| `test_routing.py` | 2-3 | Routing table, relay/repeat (3 nodes) |

Tests that require more nodes than configured are automatically skipped.

**Safety:** Frequency tests exclusively use the preset configured in `nodes.yaml`. The frequency is never switched between 433/868, as transmitting on mismatched hardware can cause damage.

## Writing Custom Tests

```python
import pytest

@pytest.mark.min_nodes(2)
class TestMyFeature:

    def test_something(self, node_a, node_b, config):
        """node_a and node_b are RMeshNode instances."""
        # Clear events
        node_b.drain_events()

        # Send message
        dst = config["nodes"][1]["call"]
        node_a.send_message(dst, "hello")

        # Wait for reception (with text matching)
        rx = node_b.wait_for_event("rx", timeout=30.0,
                                    frameType=3, text="hello")
        assert rx is not None
```

### Available Fixtures

| Fixture | Scope | Description |
|---------|-------|-------------|
| `config` | session | Dict from the YAML file |
| `nodes` | session | List of all `RMeshNode` instances |
| `node_a` | function | First node (min. 1 node) |
| `node_b` | function | Second node (min. 2 nodes) |
| `node_c` | function | Third node (min. 3 nodes) |

### RMeshNode API

**Setup:**
- `configure(callsign, freq_preset, wifi=None)` -- Configure the node
- `enable_debug()` / `disable_debug()` -- Enable/disable debug mode
- `reboot()` -- Restart the node
- `wait_ready(timeout)` -- Wait for boot-ready

**Messaging:**
- `send_message(dst, text)` -- Send a direct message
- `send_group(group, text)` -- Send a group message
- `send_trace(dst)` -- Send a trace
- `trigger_announce()` -- Trigger an announce

**Queries:**
- `get_peers()` -- Query peer list
- `get_routes()` -- Query routing table
- `get_acks()` -- Query ACK list
- `get_txbuf()` -- Query TX buffer status
- `get_version()` -- Query firmware version
- `get_settings()` -- Get all settings as text

**Events:**
- `drain_events()` -- Clear all buffered events
- `wait_for_event(event, timeout, **match)` -- Wait for a specific event
- `read_lines(timeout)` -- Read raw serial lines

### Debug Events (from the node)

| Event | Fields | Description |
|-------|--------|-------------|
| `ready` | `call`, `version`, `board` | Node has booted |
| `rx` | `frameType`, `srcCall`, `nodeCall`, `text`, `rssi`, `snr`, ... | Frame received |
| `tx` | `frameType`, `nodeCall`, `viaCall`, `port`, `retry`, ... | Frame sent |
| `ack` | `srcCall`, `nodeCall`, `id` | ACK received |
| `peer` | `action`, `call`, `port`, `rssi`, `snr` | Peer added/changed |

### Marker

```python
@pytest.mark.min_nodes(3)  # Only run test with >= 3 nodes
```

## Firmware Serial Commands (test-specific)

These commands were added for the test framework:

| Command | Description |
|---------|-------------|
| `dbg 1/0` | Enable/disable debug JSON output |
| `msg <CALL> <TEXT>` | Send a direct message |
| `xgrp <GROUP> <TEXT>` | Send a group message |
| `xtrace <CALL>` | Send a trace |
| `announce` | Trigger a manual announce |
| `peers` | Output peer list as JSON |
| `routes` | Output routing table as JSON |
| `acks` | Output ACK list as JSON |
| `xtxbuf` | Output TX buffer as JSON |

Commands with `x` prefix avoid collisions with existing commands (`grp` vs `g`, `trace`/`txbuf` vs `t`).

## Known Limitations

- **WiFi + Peer Timeouts**: When a node has WiFi access and syncs via NTP, `time()` jumps from ~0 to the current Unix time. Peers added before the NTP sync are then immediately removed by `checkPeerList()` (timestamp difference > 60 min). Therefore, WiFi is disabled by default in tests.
- **Serial Command Conflicts**: The existing firmware uses prefix matching (`strncmp`) without `else-if`. New commands must not collide with existing prefixes (e.g. `de` = factory reset, `t` = time set, `g` = gateway). Hence `dbg` instead of `debug`, `xgrp` instead of `grp`, etc.
- **Serial Echo**: The node echoes every character. The DBG response can end up on the same line as the echo (`peers\rDBG:{...}`). The parser therefore searches for `DBG:` anywhere in the line, not just at the beginning.
- **Frequency Safety**: Tests never switch the frequency preset. Only the preset configured in `nodes.yaml` is used to avoid hardware damage from transmitting on the wrong frequency.
