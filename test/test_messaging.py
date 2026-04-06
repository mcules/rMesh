"""
Multi-node messaging tests — send, receive, ACK, trace.

These tests require at least 2 nodes connected.
LoRa communication timing depends on SF/BW — default timeouts assume SF7/125kHz.
"""

import time
import pytest


# Default timeout for LoRa message delivery + ACK round-trip
LORA_TIMEOUT = 30.0


@pytest.mark.min_nodes(2)
class TestDirectMessage:
    """Direct message delivery between two nodes."""

    def test_direct_message(self, node_a, node_b, config):
        """A sends message to B — B receives it."""
        node_b.drain_events()
        time.sleep(1.0)

        dst_call = config["nodes"][1]["call"]
        node_a.send_message(dst_call, "hello from A")

        # Wait for B to receive this specific message
        rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text="hello from A")
        assert rx is not None, "Node B did not receive the message"

    def test_direct_message_ack(self, node_a, node_b, config):
        """A sends message to B — A receives ACK back."""
        node_a.drain_events()
        node_b.drain_events()
        time.sleep(1.0)

        dst_call = config["nodes"][1]["call"]
        node_a.send_message(dst_call, "ack test msg")

        # Wait for A to receive the ACK
        ack = node_a.wait_for_event("ack", timeout=LORA_TIMEOUT)
        assert ack is not None, "Node A did not receive ACK"

    def test_direct_message_content(self, node_a, node_b, config):
        """Message text arrives unmodified."""
        node_b.drain_events()
        time.sleep(1.0)
        test_text = "The quick brown fox jumps over the lazy dog 123!@#"

        dst_call = config["nodes"][1]["call"]
        node_a.send_message(dst_call, test_text)

        rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text=test_text)
        assert rx is not None, \
            f"Node B did not receive message with expected content"

    def test_bidirectional(self, node_a, node_b, config):
        """Messages work in both directions: A->B and B->A."""
        call_a = config["nodes"][0]["call"]
        call_b = config["nodes"][1]["call"]

        # A -> B
        node_b.drain_events()
        time.sleep(1.0)
        node_a.send_message(call_b, "bidi ping")
        rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text="bidi ping")
        assert rx is not None, "B did not receive message from A"

        # B -> A
        time.sleep(3.0)
        node_a.drain_events()
        time.sleep(1.0)
        node_b.send_message(call_a, "bidi pong")
        rx = node_a.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text="bidi pong")
        assert rx is not None, "A did not receive message from B"


@pytest.mark.min_nodes(2)
class TestGroupMessage:
    """Group message delivery."""

    def test_group_message(self, node_a, node_b):
        """Group message from A is received by B."""
        node_b.drain_events()
        time.sleep(1.0)

        node_a.send_group("TEST", "group hello")

        rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text="group hello")
        assert rx is not None, "Node B did not receive group message"


@pytest.mark.min_nodes(2)
class TestTrace:
    """Trace message tests."""

    def test_trace(self, node_a, node_b, config):
        """A sends trace to B — A receives echo with path."""
        dst_call = config["nodes"][1]["call"]

        # Retry trace up to 3 times — needs 2x LoRa round-trip (hin + echo)
        # which makes it more susceptible to packet loss than a single message
        found_rx = None
        all_rx_events = []
        for attempt in range(3):
            node_a.drain_events()
            node_b.drain_events()
            time.sleep(1.0)

            node_a.send_trace(dst_call)
            time.sleep(LORA_TIMEOUT)

            # Collect ALL events from both nodes for diagnostics
            events_a = node_a.drain_events()
            events_b = node_b.drain_events()

            for ev in events_a:
                if ev.get("event") == "rx" and ev.get("frameType") == 3:
                    all_rx_events.append(ev)
                    text = ev.get("text", "")
                    if "ECHO" in text and ev.get("messageType") == 1:
                        found_rx = ev

            if found_rx is not None:
                break

            # Log what happened for debugging
            import sys
            print(f"\n  [Trace attempt {attempt+1}] A rx events: "
                  f"{[e for e in events_a if e.get('event') == 'rx']}",
                  file=sys.stderr)
            print(f"  [Trace attempt {attempt+1}] B rx events: "
                  f"{[e for e in events_b if e.get('event') == 'rx']}",
                  file=sys.stderr)
            print(f"  [Trace attempt {attempt+1}] B tx events: "
                  f"{[e for e in events_b if e.get('event') == 'tx']}",
                  file=sys.stderr)
            time.sleep(3.0)

        assert found_rx is not None, \
            f"Node A did not receive trace echo after 3 attempts. " \
            f"All RX on A: {all_rx_events}"


@pytest.mark.min_nodes(2)
class TestMessageEdgeCases:
    """Edge cases: long messages, special chars, multiple messages."""

    def test_long_message(self, node_a, node_b, config):
        """Message near max length (200 bytes) is delivered correctly."""
        node_b.drain_events()
        time.sleep(1.0)
        # Use 150 chars to stay within safe limits (serial buffer is 200)
        long_text = "L" * 150

        dst_call = config["nodes"][1]["call"]
        node_a.send_message(dst_call, long_text)

        rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text=long_text)
        assert rx is not None, \
            f"Node B did not receive long message"

    def test_special_chars(self, node_a, node_b, config):
        """ASCII special characters are preserved."""
        node_b.drain_events()
        time.sleep(1.0)
        special = "Hello World 123 !@#$%"

        dst_call = config["nodes"][1]["call"]
        node_a.send_message(dst_call, special)

        rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                    frameType=3, text=special)
        assert rx is not None, \
            f"Node B did not receive message with special chars"

    def test_multiple_messages(self, node_a, node_b, config):
        """Send 5 messages in sequence — all are delivered."""
        dst_call = config["nodes"][1]["call"]
        received = []

        for i in range(5):
            node_b.drain_events()
            time.sleep(1.0)
            msg_text = f"seq_{i}_{int(time.time())}"
            node_a.send_message(dst_call, msg_text)
            rx = node_b.wait_for_event("rx", timeout=LORA_TIMEOUT,
                                        frameType=3, text=msg_text)
            if rx is not None:
                received.append(i)
            # Wait between messages to avoid TX buffer congestion
            time.sleep(3.0)

        assert len(received) == 5, \
            f"Only {len(received)}/5 messages delivered: {received}"

    def test_message_ack_timing(self, node_a, node_b, config):
        """ACK arrives within reasonable timeframe."""
        node_a.drain_events()
        time.sleep(1.0)
        dst_call = config["nodes"][1]["call"]

        start = time.time()
        node_a.send_message(dst_call, f"timing_{int(time.time())}")
        ack = node_a.wait_for_event("ack", timeout=LORA_TIMEOUT)
        elapsed = time.time() - start

        assert ack is not None, "ACK not received"
        assert elapsed < LORA_TIMEOUT, \
            f"ACK took too long: {elapsed:.1f}s"
        print(f"ACK round-trip: {elapsed:.1f}s")
