"""
UDP / WiFi transport tests — requires 2 nodes, both with WiFi configured on the
same network (add a `wifi:` block per node in nodes.yaml).

IMPORTANT precondition: the two nodes must be able to reach each other over the
WiFi (form a port-1 peer). Many guest / Freifunk networks enable AP *client
isolation*, which blocks the client-to-client + broadcast traffic rMesh's UDP
mesh relies on. When no WiFi/UDP peer forms, these tests SKIP (rather than
falsely pass over the LoRa fallback) with a clear reason.
"""

import time
import pytest

UDP_TIMEOUT = 10.0


def _both_have_wifi(config) -> bool:
    nodes = config.get("nodes", [])
    return len(nodes) >= 2 and all(n.get("wifi") for n in nodes[:2])


def _wifi_peer(node, other_call: str) -> bool:
    """True if `node` sees `other_call` as an available WiFi (port 1) peer."""
    for p in node.get_peers():
        if p.get("call") == other_call and p.get("port") == 1 and p.get("available"):
            return True
    return False


@pytest.mark.min_nodes(2)
class TestUdpMessaging:

    @pytest.fixture(autouse=True)
    def require_udp_peer(self, node_a, node_b, config):
        """Skip the class unless a real WiFi/UDP peer relationship is established."""
        if not _both_have_wifi(config):
            pytest.skip("Both nodes need a `wifi:` config for UDP transport tests")
        # Announce over WiFi so the nodes learn each other as UDP peers.
        ca = config["nodes"][0]["call"]
        cb = config["nodes"][1]["call"]
        for _ in range(3):
            node_a.trigger_announce(); time.sleep(2.0)
            node_b.trigger_announce(); time.sleep(2.0)
            if _wifi_peer(node_a, cb) and _wifi_peer(node_b, ca):
                return
        pytest.skip(
            "No WiFi/UDP peer formed between the nodes — the network likely isolates "
            "clients (guest/Freifunk AP isolation blocks client-to-client + broadcast). "
            "UDP mesh needs a network that permits client-to-client traffic."
        )

    def test_udp_direct_message(self, node_a, node_b, config):
        """A -> B over WiFi/UDP."""
        node_b.drain_events()
        time.sleep(1.0)
        dst = config["nodes"][1]["call"]
        node_a.send_message(dst, "udp hello")
        rx = node_b.wait_for_event("rx", timeout=UDP_TIMEOUT,
                                   frameType=3, text="udp hello")
        assert rx is not None, "B did not receive the message"
        if "port" in rx:
            assert rx["port"] == 1, f"expected WiFi/UDP (port 1), got port {rx['port']}"

    def test_udp_ack(self, node_a, node_b, config):
        """Sender receives an ACK back over WiFi/UDP."""
        node_a.drain_events()
        time.sleep(1.0)
        dst = config["nodes"][1]["call"]
        node_a.send_message(dst, "udp ack test")
        ack = node_a.wait_for_event("ack", timeout=UDP_TIMEOUT)
        assert ack is not None, "A did not receive an ACK"
