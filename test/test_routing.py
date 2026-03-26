"""
Routing and relay tests.

These tests require 2+ or 3+ nodes connected.
"""

import time
import pytest
from test_peers import _ensure_peers


LORA_TIMEOUT = 30.0


@pytest.mark.min_nodes(2)
class TestRouteCreation:
    """Routing table tests (2+ nodes)."""

    def test_route_added_after_discovery(self, node_a, node_b, config):
        """After peer discovery, a route exists to the peer."""
        _ensure_peers(node_a, node_b, config)

        call_b = config["nodes"][1]["call"]
        routes = node_a.get_routes()
        route_to_b = next((r for r in routes if r["dst"] == call_b), None)
        assert route_to_b is not None, \
            f"No route to {call_b} in A's routing table: {routes}"

    def test_route_via_direct_peer(self, node_a, node_b, config):
        """Direct peer route has hop count 0 and via == destination."""
        _ensure_peers(node_a, node_b, config)

        call_b = config["nodes"][1]["call"]
        routes = node_a.get_routes()
        route_to_b = next((r for r in routes if r["dst"] == call_b), None)
        assert route_to_b is not None, f"No route to {call_b}: {routes}"
        assert route_to_b["via"] == call_b, \
            f"Direct route via should be {call_b}, got {route_to_b['via']}"
        assert route_to_b["hops"] == 0, \
            f"Direct route hops should be 0, got {route_to_b['hops']}"


@pytest.mark.min_nodes(3)
class TestRelay:
    """Relay/repeat tests (3+ nodes).

    Assumes:
    - node_b (middle node) has repeat enabled
    - node_a and node_c can reach node_b but not each other directly
      (or at least that relay path works)
    """

    @pytest.fixture(autouse=True)
    def setup_relay(self, node_a, node_b, node_c):
        """Enable repeat on node_b for relay tests."""
        node_b.send_command("rep 1")
        time.sleep(0.3)

        # Ensure all nodes discover each other
        node_a.trigger_announce()
        time.sleep(3.0)
        node_b.trigger_announce()
        time.sleep(3.0)
        node_c.trigger_announce()
        time.sleep(5.0)

    def test_relay_message(self, node_a, node_c, config):
        """A sends message to C — relayed via B."""
        node_c.drain_events()
        time.sleep(1.0)

        call_c = config["nodes"][2]["call"]
        node_a.send_message(call_c, "relayed hello")

        # C should receive the message (possibly with hopCount > 0)
        rx = node_c.wait_for_event("rx", timeout=LORA_TIMEOUT * 2,
                                    frameType=3, text="relayed hello")
        assert rx is not None, "Node C did not receive relayed message"

    def test_relay_increments_hop_count(self, node_a, node_c, config):
        """Relayed message has hopCount > 0."""
        node_c.drain_events()
        time.sleep(1.0)

        call_c = config["nodes"][2]["call"]
        node_a.send_message(call_c, "hop test")

        rx = node_c.wait_for_event("rx", timeout=LORA_TIMEOUT * 2,
                                    frameType=3, text="hop test")
        assert rx is not None, "Node C did not receive relayed message"
        assert rx.get("hopCount", 0) >= 1, \
            f"Expected hopCount >= 1 for relayed message, got {rx.get('hopCount')}"

    def test_relay_trace_shows_path(self, node_a, node_c, config):
        """Trace from A to C shows intermediate nodes in path."""
        node_a.drain_events()
        time.sleep(1.0)

        call_c = config["nodes"][2]["call"]
        call_b = config["nodes"][1]["call"]
        node_a.send_trace(call_c)

        # Wait for trace echo
        deadline = time.time() + LORA_TIMEOUT * 2
        found_rx = None
        while time.time() < deadline:
            rx = node_a.wait_for_event("rx", timeout=3.0, frameType=3)
            if rx is not None:
                text = rx.get("text", "")
                if "ECHO" in text and rx.get("messageType") == 1:
                    found_rx = rx
                    break
        if found_rx is not None:
            text = found_rx.get("text", "")
            assert call_b in text, \
                f"Expected {call_b} in trace path: {text}"
