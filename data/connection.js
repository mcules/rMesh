// connection.js — WiFi (WebSocket) + BLE (Nordic NUS) transport with automatic fallback
// Replaces the direct initWebSocket() call. All existing code using sendWS() keeps working.

var Connection = (function () {

  // ── NUS UUIDs ──────────────────────────────────────────────────────────────
  var NUS_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
  var NUS_RX_CHAR = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // phone → node (write)
  var NUS_TX_CHAR = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // node → phone (notify)

  // ── State ──────────────────────────────────────────────────────────────────
  var mode        = 'disconnected'; // 'wifi' | 'ble' | 'disconnected'
  var bleDevice   = null;
  var bleTxChar   = null;  // node → phone (read notifications from this)
  var bleRxChar   = null;  // phone → node (write to this)
  var bleBuffer   = '';
  var reconnecting = false;

  function getMode() { return mode; }

  // ── Status display ─────────────────────────────────────────────────────────
  function setStatus(newMode) {
    mode = newMode;
    var el = document.getElementById('connection-status');
    if (!el) return;
    var map = {
      wifi:         '\uD83D\uDFE2 WiFi',
      ble:          '\uD83D\uDD35 Bluetooth',
      disconnected: '\uD83D\uDD34 Offline'
    };
    el.textContent = map[newMode] || newMode;
    el.className = 'conn-status conn-' + newMode;
  }

  // ── Send (unified) ─────────────────────────────────────────────────────────
  function send(text) {
    if (mode === 'wifi') {
      // Use existing WebSocket path
      try {
        if (websocket && websocket.readyState === WebSocket.OPEN) {
          websocket.send(text);
          console.log('TX: ' + text);
        }
      } catch (e) { console.warn('WS send failed:', e); }
    } else if (mode === 'ble' && bleRxChar) {
      var encoder = new TextEncoder();
      var bytes = encoder.encode(text + '\n');
      var off = 0;
      function writeNext() {
        if (off >= bytes.length) return;
        var chunk = bytes.slice(off, off + 20);
        off += 20;
        bleRxChar.writeValue(chunk).then(writeNext).catch(function (e) {
          console.warn('[BLE] write error:', e);
        });
      }
      writeNext();
      console.log('[BLE] TX: ' + text);
    }
  }

  // ── WiFi (WebSocket) ──────────────────────────────────────────────────────
  function connectWifi() {
    // Reuse existing initWebSocket logic
    if (!window.location.hostname.includes('127.0.0.1')) {
      gateway = 'ws://' + window.location.hostname + '/socket';
      baseURL = '';
    } else {
      gateway = 'ws://192.168.33.60/socket';
      baseURL = 'http://192.168.33.60/';
    }

    if (typeof setAntennaColor === 'function') setAntennaColor('#525252');

    websocket = new WebSocket(gateway);

    websocket.onopen = function (e) {
      setStatus('wifi');
      onOpen(e);
    };

    websocket.onclose = function (e) {
      setStatus('disconnected');
      onClose(e);
      // Reconnect after backoff
      setTimeout(connectWifi, _wsReconnectDelay);
      _wsReconnectDelay = Math.min(_wsReconnectDelay * 2, 30000);
    };

    websocket.onmessage = onMessage;
  }

  // ── BLE ────────────────────────────────────────────────────────────────────
  function connectBle() {
    if (!navigator.bluetooth) {
      console.warn('[BLE] WebBluetooth not supported');
      return Promise.reject(new Error('WebBluetooth not supported'));
    }
    return navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: 'rMesh' }],
      optionalServices: [NUS_SERVICE]
    })
    .then(function (device) {
      bleDevice = device;
      bleDevice.addEventListener('gattserverdisconnected', function () {
        setStatus('disconnected');
        if (typeof setAntennaColor === 'function') setAntennaColor('#525252');
        scheduleReconnect();
      });
      return device.gatt.connect();
    })
    .then(function (server)  { return server.getPrimaryService(NUS_SERVICE); })
    .then(function (service) {
      return Promise.all([
        service.getCharacteristic(NUS_TX_CHAR),
        service.getCharacteristic(NUS_RX_CHAR)
      ]);
    })
    .then(function (chars) {
      bleTxChar = chars[0];
      bleRxChar = chars[1];
      return bleTxChar.startNotifications();
    })
    .then(function () {
      bleTxChar.addEventListener('characteristicvaluechanged', function (e) {
        var chunk = new TextDecoder().decode(e.target.value);
        bleBuffer += chunk;
        var lines = bleBuffer.split('\n');
        bleBuffer = lines.pop(); // keep incomplete tail
        lines.forEach(function (line) {
          if (line.trim()) onMessage({ data: line.trim() });
        });
      });
      setStatus('ble');
      if (typeof setAntennaColor === 'function') setAntennaColor('#818cf8');
      console.log('[BLE] Connected via NUS');
    });
  }

  // ── Auto-reconnect ─────────────────────────────────────────────────────────
  function scheduleReconnect() {
    if (reconnecting) return;
    reconnecting = true;
    setTimeout(function () {
      reconnecting = false;
      // Only auto-reconnect BLE if we had a device before
      if (bleDevice && bleDevice.gatt) {
        bleDevice.gatt.connect()
          .then(function () { setStatus('ble'); })
          .catch(function () { setStatus('disconnected'); });
      }
    }, 3000);
  }

  // ── Entry point ────────────────────────────────────────────────────────────
  function start() {
    connectWifi();
  }

  // ── Show BLE button ────────────────────────────────────────────────────────
  function showBleButton() {
    var btn = document.getElementById('ble-connect-btn');
    if (btn) btn.style.display = 'inline-block';
  }

  return {
    send: send,
    start: start,
    getMode: getMode,
    connectBle: connectBle,
    showBleButton: showBleButton
  };

})();

// Override sendWS so all existing code keeps working
function sendWS(text) { Connection.send(text); }
