"""
Single-node tests — boot, version, settings, configuration.

These tests require at least 1 node connected.
"""

import time
import pytest


@pytest.mark.min_nodes(1)
class TestBoot:
    """Boot and basic responsiveness tests."""

    def test_version(self, node_a):
        """Node responds to 'v' command with version string."""
        version = node_a.get_version()
        assert "rMesh" in version, f"Expected 'rMesh' in version, got: {version}"

    def test_settings_show(self, node_a):
        """Node responds to 'se' command with settings output."""
        settings = node_a.get_settings()
        assert len(settings) > 0, "Settings output is empty"

    def test_debug_mode_toggle(self, node_a):
        """Debug mode can be enabled and disabled."""
        node_a.send_command("dbg 0")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=0.5)
        found_off = any("off" in line.lower() for line in lines)
        assert found_off, "Expected 'off' in debug disable response"

        node_a.send_command("dbg 1")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=0.5)
        found_on = any("on" in line.lower() for line in lines)
        assert found_on, "Expected 'on' in debug enable response"


@pytest.mark.min_nodes(1)
class TestCallsign:
    """Callsign configuration tests."""

    def test_callsign_set(self, node_a, config):
        """Setting a callsign persists and is shown correctly."""
        expected = config["nodes"][0]["call"]
        node_a.send_command(f"call {expected}")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any(expected.upper() in line.upper() for line in lines)
        assert found, f"Callsign '{expected}' not found in response: {lines}"

    def test_callsign_uppercase(self, node_a, config):
        """Callsign is stored in uppercase."""
        node_a.send_command("call test-x")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("TEST-X" in line for line in lines)
        assert found, f"Expected uppercase callsign 'TEST-X' in response: {lines}"

        # Restore original callsign from config
        original = config["nodes"][0]["call"]
        node_a.send_command(f"call {original}")
        time.sleep(0.3)


@pytest.mark.min_nodes(1)
class TestFrequencyPresets:
    """Frequency preset configuration tests.

    Only the preset from the config is tested to avoid hardware damage
    (e.g. transmitting on 433 MHz with 868 MHz hardware or vice versa).
    """

    def test_freq_preset_from_config(self, node_a, config):
        """Configured preset can be set and is confirmed."""
        preset = config["nodes"][0].get("preset", "868")
        node_a.send_command(f"freq {preset}")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any(preset in line for line in lines)
        assert found, f"Expected '{preset}' confirmation in response: {lines}"


@pytest.mark.min_nodes(1)
class TestLoRaParams:
    """Individual LoRa parameter set/read tests."""

    def test_spreading_factor(self, node_a):
        """Spreading factor can be set and read back."""
        node_a.send_command("sf 9")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("9" in line for line in lines)
        assert found, f"Expected SF 9 in response: {lines}"

        # Restore
        node_a.send_command("sf 7")
        time.sleep(0.3)

    def test_bandwidth(self, node_a):
        """Bandwidth can be set and read back."""
        node_a.send_command("bw 125")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("125" in line for line in lines)
        assert found, f"Expected BW 125 in response: {lines}"

    def test_coding_rate(self, node_a):
        """Coding rate can be set and read back."""
        node_a.send_command("cr 6")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("6" in line for line in lines)
        assert found, f"Expected CR 6 in response: {lines}"

        # Restore
        node_a.send_command("cr 5")
        time.sleep(0.3)

    def test_tx_power(self, node_a):
        """TX power can be set and read back."""
        node_a.send_command("op 14")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("14" in line for line in lines)
        assert found, f"Expected TX Power 14 in response: {lines}"

        # Restore
        node_a.send_command("op 20")
        time.sleep(0.3)


@pytest.mark.min_nodes(1)
class TestRepeatSetting:
    """Repeat/relay toggle tests."""

    def test_repeat_enable(self, node_a):
        """Repeat can be enabled."""
        node_a.send_command("rep 1")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("true" in line.lower() for line in lines)
        assert found, f"Expected 'true' in repeat response: {lines}"

    def test_repeat_disable(self, node_a):
        """Repeat can be disabled."""
        node_a.send_command("rep 0")
        time.sleep(0.3)
        lines = node_a.read_lines(timeout=1.0)
        found = any("false" in line.lower() for line in lines)
        assert found, f"Expected 'false' in repeat response: {lines}"

        # Restore
        node_a.send_command("rep 1")
        time.sleep(0.3)


@pytest.mark.min_nodes(1)
class TestQueryCommands:
    """Test that query commands return valid JSON."""

    def test_peers_query(self, node_a):
        """Peers query returns a valid response."""
        peers = node_a.get_peers()
        assert isinstance(peers, list), "Peers response should be a list"

    def test_routes_query(self, node_a):
        """Routes query returns a valid response."""
        routes = node_a.get_routes()
        assert isinstance(routes, list), "Routes response should be a list"

    def test_acks_query(self, node_a):
        """ACKs query returns a valid response."""
        acks = node_a.get_acks()
        assert isinstance(acks, list), "ACKs response should be a list"

    def test_txbuf_query(self, node_a):
        """TX buffer query returns a valid response."""
        txbuf = node_a.get_txbuf()
        assert "count" in txbuf, "TX buffer response should have 'count'"
        assert "data" in txbuf, "TX buffer response should have 'data'"
