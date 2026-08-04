"""
Single-node tests — boot, version, settings, configuration.

Requires at least 1 node connected. Assertions read the actual value back via
the structured `se` parser (get_settings_dict) instead of matching substrings in
the raw serial text, so a test only passes when the setting really took effect.
"""

import time
import pytest


@pytest.mark.min_nodes(1)
class TestBoot:
    """Boot and basic responsiveness tests."""

    def test_version(self, node_a):
        """Firmware reports a non-empty version string in settings."""
        version = node_a.get_settings_dict().get("version", "")
        assert len(version) > 0, "settings report no version"

    def test_settings_parse(self, node_a):
        """`se` output parses into a dict with the core keys."""
        s = node_a.get_settings_dict()
        assert "myCall" in s, f"settings dict missing myCall: {s}"
        assert "spreadingFactor" in s
        assert "frequency" in s

    def test_debug_mode_toggle(self, node_a):
        """Debug OFF -> plain text reply appears; debug ON -> DBG events flow again."""
        node_a.send_command("dbg 0")
        assert node_a.wait_for_line("false", timeout=2.0) is not None, "no plain reply after dbg 0"
        node_a.send_command("dbg 1")
        time.sleep(0.3)
        node_a.drain_events()
        node_a.send_command("peers")
        assert node_a.wait_for_query("peers", timeout=3.0) is not None, "no DBG events after dbg 1"


@pytest.mark.min_nodes(1)
class TestCallsign:
    """Callsign configuration — verified by reading myCall back."""

    def test_callsign_set(self, node_a, config):
        expected = config["nodes"][0]["call"].upper()
        node_a.send_command(f"call {expected}")
        time.sleep(0.4)
        assert node_a.get_settings_dict().get("myCall") == expected

    def test_callsign_uppercased(self, node_a, config):
        node_a.send_command("call test-x")
        time.sleep(0.4)
        assert node_a.get_settings_dict().get("myCall") == "TEST-X"
        # restore
        node_a.send_command(f"call {config['nodes'][0]['call']}")
        time.sleep(0.4)


@pytest.mark.min_nodes(1)
class TestFrequencyPreset:
    """The node's frequency band must match the configured preset (safety)."""

    def test_band_matches_preset(self, node_a, config):
        preset = str(config["nodes"][0].get("preset", "868"))
        freq = node_a.get_settings_dict().get("frequency", "").strip()
        band = "433" if freq.startswith("43") else ("868" if freq.startswith("86") else "?")
        assert band == preset, f"freq {freq} (band {band}) != preset {preset}"


@pytest.mark.min_nodes(1)
class TestLoRaParams:
    """LoRa parameter set/read-back — capture original, set, verify, restore."""

    def _roundtrip(self, node, key, cmd, value, prefix=False):
        orig = node.get_settings_dict().get(key)
        node.send_command(f"{cmd} {value}")
        time.sleep(0.4)
        got = node.get_settings_dict().get(key)
        if prefix:
            assert got is not None and got.startswith(str(value)), f"{key}={got}, want ~{value}"
        else:
            assert got == str(value), f"{key}={got}, want {value}"
        if orig is not None:
            # restore to a plausible original numeric (strip any unit suffix)
            node.send_command(f"{cmd} {orig.split()[0]}")
            time.sleep(0.4)

    def test_spreading_factor(self, node_a):
        self._roundtrip(node_a, "spreadingFactor", "lora sf", 9)

    def test_coding_rate(self, node_a):
        self._roundtrip(node_a, "codingRate", "lora cr", 7)

    def test_bandwidth(self, node_a):
        self._roundtrip(node_a, "bandwidth", "lora bw", 125, prefix=True)

    def test_tx_power(self, node_a):
        self._roundtrip(node_a, "outputPower", "lora op", 14, prefix=True)


@pytest.mark.min_nodes(1)
class TestRepeatSetting:
    """Repeat/relay toggle — verified via the 'repeat' setting value."""

    def test_repeat_toggle(self, node_a):
        node_a.send_command("rep 0")
        time.sleep(0.4)
        assert node_a.get_settings_dict().get("repeat") == "false"
        node_a.send_command("rep 1")
        time.sleep(0.4)
        assert node_a.get_settings_dict().get("repeat") == "true"


@pytest.mark.min_nodes(1)
class TestQueryCommands:
    """Query commands return valid structured responses."""

    def test_peers_query(self, node_a):
        assert isinstance(node_a.get_peers(), list)

    def test_routes_query(self, node_a):
        assert isinstance(node_a.get_routes(), list)

    def test_acks_query(self, node_a):
        assert isinstance(node_a.get_acks(), list)

    def test_txbuf_query(self, node_a):
        txbuf = node_a.get_txbuf()
        assert "count" in txbuf and "data" in txbuf
