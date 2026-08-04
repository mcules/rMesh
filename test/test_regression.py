"""
Regression tests for specific firmware fixes — single node.

Each test guards a concrete bug/feature from the 2026-07 review so it cannot
silently regress. Runs on 1 node; safe (no frequency-band switching).
"""

import time
import pytest

MAX_CALLSIGN_LENGTH = 9


@pytest.mark.min_nodes(1)
class TestCallsignClamp:
    def test_oversized_callsign_is_clamped(self, node_a, config):
        """CLI `call` with >9 chars must be clamped, not stored oversized
        (would corrupt on-air frames / dedup)."""
        node_a.send_command("call ABCDEFGHIJKLMNOP")   # 16 chars
        time.sleep(0.4)
        mycall = node_a.get_settings_dict().get("myCall", "")
        assert len(mycall) <= MAX_CALLSIGN_LENGTH, f"callsign not clamped: {mycall!r}"
        assert mycall == "ABCDEFGHI", f"expected clamp to 9 chars, got {mycall!r}"
        # restore
        node_a.send_command(f"call {config['nodes'][0]['call']}")
        time.sleep(0.4)


@pytest.mark.min_nodes(1)
class TestLoRaParamValidation:
    def test_invalid_sf_is_clamped(self, node_a):
        """An out-of-range SF must be clamped (sanitizeLoraParams), never stored
        as-is (would feed garbage into the airtime math)."""
        orig = node_a.get_settings_dict().get("spreadingFactor")
        node_a.send_command("lora sf 99")
        time.sleep(0.4)
        sf = node_a.get_settings_dict().get("spreadingFactor")
        assert sf == "12", f"SF 99 should clamp to 12, got {sf}"
        if orig:
            node_a.send_command(f"lora sf {orig}")
            time.sleep(0.4)


@pytest.mark.min_nodes(1)
class TestNewSettings:
    def test_flood_single_present(self, node_a):
        """Backward-compatible LoRa broadcast-relay setting is exposed."""
        val = node_a.get_settings_dict().get("loraFloodSingle")
        assert val in ("true", "false"), f"loraFloodSingle missing/invalid: {val}"

    def test_status_led_toggle(self, node_a):
        """Status-LED setting toggles and defaults are respected."""
        node_a.send_command("led 1")
        time.sleep(0.3)
        assert node_a.get_settings_dict().get("statusLedEnabled") == "true"
        node_a.send_command("led 0")
        time.sleep(0.3)
        assert node_a.get_settings_dict().get("statusLedEnabled") == "false"
