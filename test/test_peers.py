"""
Peer discovery and announce tests.

These tests require at least 2 nodes connected.
"""

import time
import pytest


# Timeout for announce/ACK exchange over LoRa
ANNOUNCE_TIMEOUT = 20.0


def _ensure_peers(node_a, node_b, config, timeout=30.0):
    """Trigger announces on both nodes and wait until they discover each other.

    Returns (peers_a, peers_b) peer lists.
    """
    call_a = config["nodes"][0]["call"]
    call_b = config["nodes"][1]["call"]

    # Drain old events
    node_a.drain_events()
    node_b.drain_events()
    time.sleep(1.0)

    # Trigger announces on both sides
    node_a.trigger_announce()
    time.sleep(2.0)
    node_b.trigger_announce()

    # Poll until both nodes see each other or timeout
    deadline = time.time() + timeout
    while time.time() < deadline:
        time.sleep(3.0)
        peers_a = node_a.get_peers()
        peers_b = node_b.get_peers()

        found_b_in_a = any(p["call"] == call_b for p in peers_a)
        found_a_in_b = any(p["call"] == call_a for p in peers_b)

        if found_b_in_a and found_a_in_b:
            return peers_a, peers_b

        # Retry announces if needed
        if not found_b_in_a:
            node_b.trigger_announce()
        if not found_a_in_b:
            node_a.trigger_announce()

    # Return whatever we have (tests will assert on these)
    return node_a.get_peers(), node_b.get_peers()


@pytest.mark.min_nodes(2)
class TestPeerDiscovery:
    """Peer discovery via announce beacons."""

    def test_peer_diagnostics(self, node_a, node_b, config):
        """Diagnostic: dump peer state and raw events from both nodes."""
        import sys

        # Reboot both nodes for a clean state
        print(f"\n  Rebooting both nodes...", file=sys.stderr)
        node_a.send_command("reb")
        node_b.send_command("reb")
        time.sleep(8.0)

        # Wait for ready
        ready_a = node_a.wait_for_event("ready", timeout=10.0)
        ready_b = node_b.wait_for_event("ready", timeout=10.0)
        print(f"  [A] Ready: {ready_a}", file=sys.stderr)
        print(f"  [B] Ready: {ready_b}", file=sys.stderr)

        # Re-enable debug + configure
        node_a.enable_debug()
        node_b.enable_debug()
        node_a.configure(callsign=config["nodes"][0]["call"],
                         freq_preset=config["nodes"][0].get("preset", "868"),
                         wifi=config["nodes"][0].get("wifi"))
        node_b.configure(callsign=config["nodes"][1]["call"],
                         freq_preset=config["nodes"][1].get("preset", "868"),
                         wifi=config["nodes"][1].get("wifi"))
        node_a.drain_events()
        node_b.drain_events()
        time.sleep(1.0)

        # Step 1: Initial peer discovery — all nodes announce
        print(f"\n  Step 1: Initial announce from all nodes...", file=sys.stderr)
        node_a.drain_events()
        node_b.drain_events()

        node_a.trigger_announce()
        time.sleep(3.0)
        node_b.trigger_announce()
        time.sleep(10.0)

        # Check state after initial exchange
        for label, node in [("A", node_a), ("B", node_b)]:
            peers = node.get_peers()
            routes = node.get_routes()
            print(f"  [{label}] Peers: {peers}", file=sys.stderr)
            print(f"  [{label}] Routes: {routes}", file=sys.stderr)

        # Step 2: Second announce from A, collect ALL events
        print(f"\n  Step 2: Second announce from A, watching events...",
              file=sys.stderr)
        node_a.drain_events()
        node_b.drain_events()
        time.sleep(0.5)

        node_a.trigger_announce()
        time.sleep(15.0)

        events_a = node_a.drain_events()
        events_b = node_b.drain_events()
        print(f"\n  [A] Events: {len(events_a)}", file=sys.stderr)
        for ev in events_a:
            print(f"    {ev}", file=sys.stderr)
        print(f"  [B] Events: {len(events_b)}", file=sys.stderr)
        for ev in events_b:
            print(f"    {ev}", file=sys.stderr)

        # Final state — query with raw output inspection
        print(f"\n  Querying peers with raw serial inspection...",
              file=sys.stderr)
        for label, node in [("A", node_a), ("B", node_b)]:
            node.drain_events()
            node.read_lines(timeout=0.5)
            node.send_command("peers")
            time.sleep(2.0)
            raw_lines = node.read_lines(timeout=0.5)
            events = node.drain_events()
            print(f"  [{label}] Raw lines after 'peers': {raw_lines}",
                  file=sys.stderr)
            print(f"  [{label}] Events after 'peers': {events}",
                  file=sys.stderr)

    def test_announce_triggers_ack(self, node_a, node_b):
        """Announce from A triggers an announce_ack from B (visible as TX on B)."""
        node_b.drain_events()
        time.sleep(1.0)

        node_a.trigger_announce()

        # B should transmit an ANNOUNCE_ACK_FRAME (frameType=1)
        tx = node_b.wait_for_event("tx", timeout=ANNOUNCE_TIMEOUT,
                                    frameType=1)
        assert tx is not None, "Node B did not send ANNOUNCE_ACK"

    def test_mutual_discovery(self, node_a, node_b, config):
        """Both nodes discover each other after announce exchange."""
        call_a = config["nodes"][0]["call"]
        call_b = config["nodes"][1]["call"]

        peers_a, peers_b = _ensure_peers(node_a, node_b, config)

        found_b_in_a = any(p["call"] == call_b for p in peers_a)
        found_a_in_b = any(p["call"] == call_a for p in peers_b)

        assert found_b_in_a, \
            f"B ({call_b}) not in A's peers: {peers_a}"
        assert found_a_in_b, \
            f"A ({call_a}) not in B's peers: {peers_b}"

    def test_peer_available(self, node_a, node_b, config):
        """After full announce exchange, peers are marked available."""
        call_b = config["nodes"][1]["call"]

        peers_a, _ = _ensure_peers(node_a, node_b, config)

        peer_b = next((p for p in peers_a if p["call"] == call_b), None)
        assert peer_b is not None, \
            f"Node B ({call_b}) not in A's peer list: {peers_a}"
        assert peer_b.get("available") is True, \
            f"Node B not marked available: {peer_b}"

    def test_peer_signal_quality(self, node_a, node_b, config):
        """Peer entries have RSSI and SNR values for LoRa connections."""
        call_b = config["nodes"][1]["call"]

        peers_a, _ = _ensure_peers(node_a, node_b, config)

        peer_b = next((p for p in peers_a if p["call"] == call_b), None)
        assert peer_b is not None, f"Peer B not found in A's list: {peers_a}"

        # LoRa peers (port 0) should have non-zero signal quality
        if peer_b.get("port") == 0:
            assert peer_b.get("rssi") != 0 or peer_b.get("snr") != 0, \
                f"LoRa peer has no signal data: {peer_b}"
