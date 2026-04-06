"""
Pytest configuration and fixtures for rMesh device testing.

Usage:
    pytest test/ --config test/nodes.yaml -v
    pytest test/ --config test/nodes.yaml --no-flash -v   (skip flashing)
"""

import subprocess
import sys
import pytest
import yaml
import time
from pathlib import Path
from rmesh_node import RMeshNode


# Project root (one level above test/)
PROJECT_ROOT = Path(__file__).resolve().parent.parent


def _log(msg: str) -> None:
    """Print status to stderr so it's always visible (not captured by pytest)."""
    print(msg, file=sys.stderr, flush=True)


# ── CLI options ──────────────────────────────────────────────────────────────

def pytest_addoption(parser):
    parser.addoption(
        "--config",
        required=True,
        help="Path to nodes.yaml config file",
    )
    parser.addoption(
        "--no-flash",
        action="store_true",
        default=False,
        help="Skip firmware flashing (use already flashed firmware)",
    )


# ── Custom markers ───────────────────────────────────────────────────────────

def pytest_configure(config):
    config.addinivalue_line(
        "markers",
        "min_nodes(n): skip test if fewer than n nodes are configured",
    )


# ── Firmware flashing (runs before any test) ─────────────────────────────────

def _load_config(config) -> dict:
    """Load nodes.yaml from the --config path."""
    config_path = Path(config.getoption("--config"))
    if not config_path.exists():
        pytest.fail(f"Config file not found: {config_path}")
    with open(config_path) as f:
        return yaml.safe_load(f)


def pytest_sessionstart(session):
    """Flash firmware to all nodes before any tests run."""
    config = session.config
    skip_flash = config.getoption("--no-flash", default=False)

    cfg = _load_config(config)
    node_configs = cfg.get("nodes", [])
    if not node_configs:
        return

    if skip_flash:
        _log("\n  Skipping firmware flash (--no-flash)\n")
        return

    _log("")
    _log("=" * 60)
    _log("  FLASHING FIRMWARE")
    _log("=" * 60)

    # Build once per board type, then upload to each node
    built_boards: set[str] = set()

    for nc in node_configs:
        board = nc["board"]
        port = nc["port"]
        name = nc["name"]

        # Build once per board type
        if board not in built_boards:
            _log(f"\n  [{board}] Building firmware...")
            result = subprocess.run(
                ["pio", "run", "-e", board],
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                _log(result.stdout)
                _log(result.stderr)
                pytest.exit(f"Build failed for {board}", returncode=1)
            _log(f"  [{board}] Build OK")
            built_boards.add(board)

        # Upload firmware
        _log(f"\n  [{name}] Uploading firmware to {port}...")
        result = subprocess.run(
            ["pio", "run", "-e", board, "-t", "upload", "--upload-port", port],
            cwd=str(PROJECT_ROOT),
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            _log(result.stdout)
            _log(result.stderr)
            pytest.exit(
                f"Upload failed for {name} ({board} on {port})", returncode=1
            )
        _log(f"  [{name}] Firmware uploaded")

    _log("")
    _log(f"  All {len(node_configs)} node(s) flashed successfully.")
    _log(f"  Waiting for nodes to boot...")
    _log("")

    # Wait for nodes to reboot after flashing
    time.sleep(5.0)


# ── Session-scoped fixtures ──────────────────────────────────────────────────

@pytest.fixture(scope="session")
def config(request) -> dict:
    """Load the YAML config file specified via --config."""
    return _load_config(request.config)


@pytest.fixture(scope="session")
def nodes(config) -> list[RMeshNode]:
    """Connect to all nodes, verify version, configure, establish peers.

    Steps:
    1. Open serial connections, wait for ready event
    2. Verify firmware version matches expected board
    3. Enable debug, set callsign + preset
    4. Reboot to apply all settings cleanly
    5. Trigger mutual announce to establish peers before tests

    Yields the list of connected RMeshNode instances.
    Closes all connections after the test session.
    """
    node_configs = config.get("nodes", [])
    if not node_configs:
        pytest.fail("No nodes defined in config file")

    # ── Step 1: Connect and wait for ready ────────────────────────────────
    _log("\n" + "-" * 60)
    _log("  CONNECTING TO NODES")
    _log("-" * 60)

    node_list: list[RMeshNode] = []
    for nc in node_configs:
        name = nc["name"]
        port = nc["port"]
        _log(f"  [{name}] Connecting to {port}...")
        node = RMeshNode(name=name, port=port, baudrate=115200)
        node_list.append(node)

    time.sleep(2.0)

    ready_events: dict[str, dict] = {}
    for node in node_list:
        ready = node.wait_for_event("ready", timeout=15.0)
        if ready:
            ready_events[node.name] = ready
            _log(f"  [{node.name}] Ready: {ready.get('call')} "
                 f"({ready.get('board')}, {ready.get('version')})")
        else:
            # No ready event — reboot to get one
            _log(f"  [{node.name}] No ready event, rebooting...")
            node.send_command("reb")
            time.sleep(5.0)
            ready = node.wait_for_event("ready", timeout=15.0)
            if ready:
                ready_events[node.name] = ready
                _log(f"  [{node.name}] Ready: {ready.get('call')} "
                     f"({ready.get('board')}, {ready.get('version')})")
            else:
                pytest.fail(
                    f"[{node.name}] Node not responding on {node.port}"
                )

    # ── Step 2: Verify firmware version ───────────────────────────────────
    _log("")
    _log("-" * 60)
    _log("  VERIFYING FIRMWARE")
    _log("-" * 60)

    for node, nc in zip(node_list, node_configs):
        expected_board = nc["board"]
        ready = ready_events[node.name]
        actual_board = ready.get("board", "")
        version = ready.get("version", "?")

        if actual_board != expected_board:
            pytest.fail(
                f"[{node.name}] Board mismatch: "
                f"expected '{expected_board}', got '{actual_board}'. "
                f"Check nodes.yaml or re-flash."
            )
        _log(f"  [{node.name}] OK: {actual_board} ({version})")

    # ── Step 3: Configure callsign, preset, debug ─────────────────────────
    _log("")
    _log("-" * 60)
    _log("  CONFIGURING NODES")
    _log("-" * 60)

    for node, nc in zip(node_list, node_configs):
        node.enable_debug()
        node.configure(
            callsign=nc["call"],
            freq_preset=nc.get("preset", "868"),
            wifi=nc.get("wifi"),
        )
        node.drain_events()
        node.read_lines(timeout=0.5)
        _log(f"  [{node.name}] Configured: call={nc['call']}, "
             f"preset={nc.get('preset', '868')}")

    # ── Step 4: Reboot to apply settings cleanly ──────────────────────────
    _log("")
    _log("  Rebooting all nodes to apply settings...")

    for node in node_list:
        node.reboot()

    # Close and reopen serial connections after reboot
    for node in node_list:
        node.close()

    time.sleep(5.0)

    node_list.clear()
    for nc in node_configs:
        node = RMeshNode(name=nc["name"], port=nc["port"], baudrate=115200)
        node_list.append(node)

    time.sleep(2.0)

    # Wait for ready after reboot
    for node in node_list:
        ready = node.wait_for_event("ready", timeout=15.0)
        if ready:
            _log(f"  [{node.name}] Rebooted: {ready.get('call')} "
                 f"({ready.get('board')})")
        else:
            _log(f"  [{node.name}] Warning: no ready event after reboot")

    # Re-enable debug (flag is not persisted across reboots)
    for node in node_list:
        node.enable_debug()
        node.drain_events()

    # ── Step 5: Establish peers via announce exchange ─────────────────────
    if len(node_list) >= 2:
        _log("")
        _log("-" * 60)
        _log("  PEER DISCOVERY")
        _log("-" * 60)

        # Trigger announces from all nodes with spacing
        for node in node_list:
            node.trigger_announce()
            time.sleep(3.0)

        # Wait for peer exchange to complete
        time.sleep(5.0)

        # Verify peers
        for node, nc in zip(node_list, node_configs):
            peers = node.get_peers()
            peer_calls = [p["call"] for p in peers]
            _log(f"  [{node.name}] Peers: {peer_calls}")

    # Final drain
    for node in node_list:
        node.drain_events()
        node.read_lines(timeout=0.5)

    _log(f"\n  {len(node_list)} node(s) ready for testing.\n")
    yield node_list

    # Teardown: close all serial connections
    for node in node_list:
        node.close()
    _log(f"\n  All serial connections closed.\n")


# ── Per-node convenience fixtures ────────────────────────────────────────────

@pytest.fixture
def node_a(nodes) -> RMeshNode:
    """First node (requires >= 1 node)."""
    if len(nodes) < 1:
        pytest.skip("Requires at least 1 node")
    return nodes[0]


@pytest.fixture
def node_b(nodes) -> RMeshNode:
    """Second node (requires >= 2 nodes)."""
    if len(nodes) < 2:
        pytest.skip("Requires at least 2 nodes")
    return nodes[1]


@pytest.fixture
def node_c(nodes) -> RMeshNode:
    """Third node (requires >= 3 nodes)."""
    if len(nodes) < 3:
        pytest.skip("Requires at least 3 nodes")
    return nodes[2]


# ── Auto-skip for @pytest.mark.min_nodes(N) ─────────────────────────────────

def pytest_collection_modifyitems(config, items):
    """Skip tests that require more nodes than are configured."""
    config_path = config.getoption("--config", default=None)
    if config_path is None:
        return

    try:
        with open(config_path) as f:
            cfg = yaml.safe_load(f)
        num_nodes = len(cfg.get("nodes", []))
    except Exception:
        return

    skip_marker = pytest.mark.skip(reason="Not enough nodes configured")
    for item in items:
        marker = item.get_closest_marker("min_nodes")
        if marker is not None:
            required = marker.args[0]
            if num_nodes < required:
                item.add_marker(skip_marker)
