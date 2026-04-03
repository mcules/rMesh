"""
RMeshNode — Serial communication wrapper for rMesh device testing.

Connects to a single rMesh node via serial port and provides methods
for sending commands, reading debug events, and querying device state.
"""

import json
import sys
import time
import threading
import serial


DBG_PREFIX = "DBG:"
DEBUG_SERIAL = False  # Set True to see all serial traffic on stderr


class RMeshNode:
    """Serial interface to a single rMesh node."""

    def __init__(self, name: str, port: str, baudrate: int = 115200,
                 timeout: float = 1.0):
        self.name = name
        self.port = port
        self.baudrate = baudrate
        self._ser = serial.Serial(port, baudrate, timeout=timeout)
        self._events: list[dict] = []
        self._lines: list[str] = []
        self._lock = threading.Lock()
        self._running = True
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def close(self):
        """Close serial connection."""
        self._running = False
        self._reader.join(timeout=2.0)
        self._ser.close()

    # ── Low-level I/O ────────────────────────────────────────────────────────

    def _read_loop(self):
        """Background thread: continuously read serial lines and sort into
        debug events vs. plain output."""
        while self._running:
            try:
                if self._ser.in_waiting > 0:
                    raw = self._ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    if DEBUG_SERIAL:
                        print(f"  [{self.name}] {line}",
                              file=sys.stderr, flush=True)
                    with self._lock:
                        # Find DBG: anywhere in line (echo prefix may
                        # precede it on the same line due to CR without LF)
                        dbg_pos = line.find(DBG_PREFIX)
                        if dbg_pos >= 0:
                            json_str = line[dbg_pos + len(DBG_PREFIX):]
                            try:
                                event = json.loads(json_str)
                                self._events.append(event)
                            except json.JSONDecodeError:
                                print(f"  [{self.name}] BAD JSON: {line}",
                                      file=sys.stderr, flush=True)
                                self._lines.append(line)
                        else:
                            self._lines.append(line)
                else:
                    time.sleep(0.01)
            except serial.SerialException:
                break
            except Exception:
                continue

    def send_command(self, cmd: str) -> None:
        """Send a command string followed by CR to the node."""
        self._ser.write((cmd + "\r").encode("utf-8"))
        self._ser.flush()

    def read_lines(self, timeout: float = 2.0) -> list[str]:
        """Collect all non-DBG lines received within timeout."""
        time.sleep(timeout)
        with self._lock:
            lines = list(self._lines)
            self._lines.clear()
        return lines

    def drain_events(self) -> list[dict]:
        """Return and clear all buffered debug events."""
        with self._lock:
            events = list(self._events)
            self._events.clear()
        return events

    def read_debug_events(self, timeout: float = 5.0,
                          filter_event: str | None = None) -> list[dict]:
        """Collect debug events for up to timeout seconds.

        Args:
            timeout: Maximum wait time in seconds.
            filter_event: If set, only return events with this "event" value.

        Returns:
            List of matching debug event dicts.
        """
        deadline = time.time() + timeout
        results: list[dict] = []
        while time.time() < deadline:
            with self._lock:
                for ev in self._events:
                    if filter_event is None or ev.get("event") == filter_event:
                        results.append(ev)
                self._events.clear()
            if results:
                return results
            time.sleep(0.1)
        # Final sweep
        with self._lock:
            for ev in self._events:
                if filter_event is None or ev.get("event") == filter_event:
                    results.append(ev)
            self._events.clear()
        return results

    def wait_for_event(self, event: str, timeout: float = 10.0,
                       **match) -> dict | None:
        """Wait for a specific debug event, optionally matching extra fields.

        Args:
            event: The "event" field value to wait for (e.g. "rx", "ack").
            timeout: Maximum wait time in seconds.
            **match: Additional key=value pairs that must match in the event.

        Returns:
            The matching event dict, or None if timeout.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                for i, ev in enumerate(self._events):
                    if ev.get("event") != event:
                        continue
                    if all(ev.get(k) == v for k, v in match.items()):
                        self._events.pop(i)
                        return ev
            time.sleep(0.1)
        return None

    def wait_for_query(self, query_name: str, timeout: float = 3.0) -> dict | None:
        """Wait for a DBG query response (e.g. peers, routes, acks, txbuf).

        Args:
            query_name: The "query" field value.
            timeout: Maximum wait time.

        Returns:
            The query response dict, or None if timeout.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                for i, ev in enumerate(self._events):
                    if ev.get("query") == query_name:
                        self._events.pop(i)
                        return ev
            time.sleep(0.1)
        return None

    # ── Setup ────────────────────────────────────────────────────────────────

    def configure(self, callsign: str, freq_preset: str = "868",
                  wifi: dict | None = None) -> None:
        """Set callsign, frequency preset, and optionally WiFi on the node.

        If no wifi config is provided, WiFi is cleared and AP mode disabled
        to prevent NTP time sync from invalidating peer timestamps.
        """
        self.send_command(f"call {callsign}")
        time.sleep(0.3)
        self.send_command(f"freq {freq_preset}")
        time.sleep(0.3)
        if wifi is None:
            # Disable WiFi to prevent NTP sync killing peers
            self.send_command("wifi clear")
            time.sleep(0.3)
            self.send_command("a 0")
            time.sleep(0.3)
        else:
            ssid = wifi.get("ssid", "")
            password = wifi.get("password", "")
            if password:
                self.send_command(f"wifi add {ssid} {password}")
            else:
                self.send_command(f"wifi add {ssid}")
            time.sleep(0.3)

    def enable_debug(self) -> None:
        """Enable serial debug JSON output."""
        self.send_command("dbg 1")
        time.sleep(0.2)

    def disable_debug(self) -> None:
        """Disable serial debug JSON output."""
        self.send_command("dbg 0")
        time.sleep(0.2)

    def reboot(self) -> None:
        """Reboot the node."""
        self.send_command("reb")

    def wait_ready(self, timeout: float = 15.0) -> dict | None:
        """Wait for the node to emit the ready event after boot.

        Returns:
            The ready event dict, or None on timeout.
        """
        return self.wait_for_event("ready", timeout=timeout)

    # ── Messaging ────────────────────────────────────────────────────────────

    def send_message(self, dst: str, text: str) -> None:
        """Send a direct text message to a callsign."""
        self.send_command(f"msg {dst} {text}")

    def send_group(self, group: str, text: str) -> None:
        """Send a group text message."""
        self.send_command(f"xgrp {group} {text}")

    def send_trace(self, dst: str) -> None:
        """Send a trace to a callsign."""
        self.send_command(f"xtrace {dst}")

    def trigger_announce(self) -> None:
        """Trigger a manual announce beacon."""
        self.send_command("announce")

    # ── Queries ──────────────────────────────────────────────────────────────

    def get_peers(self) -> list[dict]:
        """Query the peer list."""
        self.drain_events()
        self.send_command("peers")
        resp = self.wait_for_query("peers")
        return resp.get("data", []) if resp else []

    def get_routes(self) -> list[dict]:
        """Query the routing table."""
        self.drain_events()
        self.send_command("routes")
        resp = self.wait_for_query("routes")
        return resp.get("data", []) if resp else []

    def get_acks(self) -> list[dict]:
        """Query the ACK list."""
        self.drain_events()
        self.send_command("acks")
        resp = self.wait_for_query("acks")
        return resp.get("data", []) if resp else []

    def get_txbuf(self) -> dict:
        """Query the TX buffer status."""
        self.drain_events()
        self.send_command("xtxbuf")
        resp = self.wait_for_query("txbuf")
        return resp if resp else {"count": 0, "data": []}

    def get_version(self) -> str:
        """Query the version string."""
        self.drain_events()
        self._lines.clear()
        self.send_command("v")
        lines = self.read_lines(timeout=1.0)
        # Version output: board name, "rMesh vX.X.X", "READY."
        for line in lines:
            if "rMesh" in line:
                return line.strip()
        return ""

    def get_settings(self) -> str:
        """Query all settings as raw text."""
        self.send_command("se")
        lines = self.read_lines(timeout=2.0)
        return "\n".join(lines)
