var websocket;
var settings;
var messages = [];
var baseURL = "";
var gateway = "";
var init = false;

function esc(s) {
    if (!s) return '';
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
}

// --- Message Cache (localStorage) ---
// Cache is scoped per node callsign so multiple nodes from the same browser
// don't collide. Key format: "rmesh_msgs_<MYCALL>".
// The legacy pre-scoping global key "rmesh_messages" is migrated on first
// contact with whichever node the user opens first after the upgrade.
var MSG_CACHE_PREFIX        = "rmesh_msgs_";
var MSG_CACHE_LEGACY_GLOBAL = "rmesh_messages";
var MSG_CACHE_KEY = null;
var MSG_CACHE_SAVE_INTERVAL = 60000;
var _msgCacheDirty = false;

function _sanitizeCallForKey(s) {
    return String(s).replace(/[^A-Za-z0-9_\-]/g, "_");
}

function setMsgCacheKey(mycall) {
    if (!mycall) return;
    var newKey = MSG_CACHE_PREFIX + _sanitizeCallForKey(mycall);
    if (MSG_CACHE_KEY === newKey) return;
    MSG_CACHE_KEY = newKey;
    try {
        if (localStorage.getItem(MSG_CACHE_KEY)) return;
        var legacy = localStorage.getItem(MSG_CACHE_LEGACY_GLOBAL);
        if (legacy) {
            localStorage.setItem(MSG_CACHE_KEY, legacy);
            localStorage.removeItem(MSG_CACHE_LEGACY_GLOBAL);
            console.log("Migrated legacy global cache → " + MSG_CACHE_KEY);
        }
    } catch(e) {}
}

function loadCachedMessages() {
    try {
        if (!MSG_CACHE_KEY) return [];
        var raw = localStorage.getItem(MSG_CACHE_KEY);
        if (!raw) return [];
        return JSON.parse(raw);
    } catch(e) { return []; }
}

function isCacheableMessage(m) {
    // only cache TEXT (0) and TRACE (1) — skip COMMAND (15), delimiters, etc.
    return !m.delimiter && (m.messageType === 0 || m.messageType === 1);
}

function saveCachedMessages() {
    try {
        if (!MSG_CACHE_KEY) return;
        // Merge: never let the in-memory tail erase older entries that are
        // still in localStorage but no longer in `messages` (e.g. when the
        // node returned a smaller window). Old cache + current = union.
        var prev = loadCachedMessages();
        var byKey = {};
        prev.forEach(function(m) {
            if (m && m.id && m.srcCall) byKey[m.srcCall + "_" + m.id] = m;
        });
        messages.filter(isCacheableMessage).forEach(function(m) {
            if (m && m.id && m.srcCall) byKey[m.srcCall + "_" + m.id] = m;
        });
        var merged = Object.keys(byKey).map(function(k) { return byKey[k]; });
        merged.sort(function(a, b) { return (a.timestamp || 0) - (b.timestamp || 0); });
        if (merged.length > 1000) merged = merged.slice(merged.length - 1000);
        localStorage.setItem(MSG_CACHE_KEY, JSON.stringify(merged));
        _msgCacheDirty = false;
    } catch(e) { /* localStorage full or unavailable */ }
}

function mergeWithCache(deviceMessages) {
    // merge cached messages that are newer than what the device returned
    var cached = loadCachedMessages();
    if (cached.length === 0) return deviceMessages;

    // build a set of known message IDs from device for fast lookup
    var knownIds = {};
    deviceMessages.forEach(function(m) { if (m.id) knownIds[m.srcCall + "_" + m.id] = true; });

    // find cached messages not present in device response
    var extra = cached.filter(function(m) {
        return m.id && !knownIds[m.srcCall + "_" + m.id];
    });

    if (extra.length === 0) return deviceMessages;

    // append missing messages and sort by timestamp
    var merged = deviceMessages.concat(extra);
    merged.sort(function(a, b) { return (a.timestamp || 0) - (b.timestamp || 0); });
    return merged;
}

// periodic save
setInterval(function() {
    if (_msgCacheDirty) saveCachedMessages();
}, MSG_CACHE_SAVE_INTERVAL);

// Mute and collection group per channel (stored as cookies)
// channelMuted[i]  = true → no sound/badge for channel i
// channelSammel[i] = true → channel i is a collection group (receive-only)
// sammelGroups[i]  = array of group name strings routed to collection group i
// sammelNames[i]   = display name for collection group i
var channelMuted   = new Array(11).fill(false);
var channelSammel  = new Array(11).fill(false);
var sammelGroups   = {};
var sammelNames    = {};

function updateMinSnrLabel(v) {
    var label = document.getElementById("settingsMinSnrValue");
    if (label) label.textContent = (v <= -20) ? t('lora.snr_off') : v + " dB";
}

function updateWifiTxPowerLabel(v) {
    var label = document.getElementById("settingsWifiTxPowerValue");
    if (label) label.textContent = v + " dBm";
}

function updateDispBrightnessLabel(v) {
    var label = document.getElementById("settingsDisplayBrightnessValue");
    if (label) label.textContent = v;
}

// CPU frequency graph (rolling 120 samples = ~2 min at 1 Hz status)
var cpuFreqHistory = [];
var CPU_FREQ_MAX_SAMPLES = 120;

function pushCpuFreq(mhz) {
    cpuFreqHistory.push(mhz);
    if (cpuFreqHistory.length > CPU_FREQ_MAX_SAMPLES) cpuFreqHistory.shift();
    var el = document.getElementById("aboutCpuFreq");
    if (el) el.textContent = mhz + " MHz";
    drawCpuFreqChart();
}

function drawCpuFreqChart() {
    var canvas = document.getElementById("cpuFreqChart");
    if (!canvas || !canvas.getContext) return;
    var ctx = canvas.getContext("2d");
    var w = canvas.width, h = canvas.height;
    var pad = 24, graphH = h - pad - 4;

    ctx.clearRect(0, 0, w, h);

    // Y-axis labels and grid
    var steps = [80, 160, 240];
    ctx.font = "10px monospace";
    ctx.textAlign = "right";
    for (var s = 0; s < steps.length; s++) {
        var y = pad + graphH - (steps[s] / 240) * graphH;
        ctx.strokeStyle = "rgba(255,255,255,0.08)";
        ctx.beginPath(); ctx.moveTo(pad, y); ctx.lineTo(w - 2, y); ctx.stroke();
        ctx.fillStyle = "#666";
        ctx.fillText(steps[s], pad - 3, y + 3);
    }

    if (cpuFreqHistory.length < 2) return;

    var n = cpuFreqHistory.length;
    var stepX = (w - pad - 2) / (CPU_FREQ_MAX_SAMPLES - 1);
    var offsetX = (CPU_FREQ_MAX_SAMPLES - n) * stepX;

    // Filled area
    ctx.beginPath();
    ctx.moveTo(pad + offsetX, pad + graphH);
    for (var i = 0; i < n; i++) {
        var x = pad + offsetX + i * stepX;
        var y = pad + graphH - (cpuFreqHistory[i] / 240) * graphH;
        ctx.lineTo(x, y);
    }
    ctx.lineTo(pad + offsetX + (n - 1) * stepX, pad + graphH);
    ctx.closePath();
    ctx.fillStyle = "rgba(56, 189, 248, 0.15)";
    ctx.fill();

    // Line
    ctx.beginPath();
    for (var i = 0; i < n; i++) {
        var x = pad + offsetX + i * stepX;
        var y = pad + graphH - (cpuFreqHistory[i] / 240) * graphH;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "#38bdf8";
    ctx.lineWidth = 1.5;
    ctx.stroke();
}

function loadChannelFlags() {
    for (let i = 1; i <= 10; i++) {
        channelMuted[i] = Cookie.get("chMute" + i) === "1";
    }
    // Migrate old single-Sammelgruppe format
    var oldCol = Cookie.get("chSamCol");
    var oldGrps = Cookie.get("chSamGrps");
    if (oldCol && oldCol !== "0" && !Cookie.get("chSamFlags")) {
        var idx = parseInt(oldCol);
        if (idx >= 3 && idx <= 10) {
            channelSammel[idx] = true;
            try { sammelGroups[idx] = JSON.parse(oldGrps || "[]"); } catch(e) { sammelGroups[idx] = []; }
            sammelNames[idx] = "";
        }
        saveChannelFlags();
        Cookie.set("chSamCol", "0");
        Cookie.set("chSamGrps", "[]");
        return;
    }
    try {
        var flags = JSON.parse(Cookie.get("chSamFlags") || "{}");
        for (var k in flags) channelSammel[parseInt(k)] = !!flags[k];
    } catch(e) {}
    try { sammelGroups = JSON.parse(Cookie.get("chSamGrpsM") || "{}"); } catch(e) { sammelGroups = {}; }
    try { sammelNames  = JSON.parse(Cookie.get("chSamNames") || "{}"); } catch(e) { sammelNames = {}; }
}
function saveChannelFlags() {
    for (let i = 1; i <= 10; i++) {
        Cookie.set("chMute" + i, channelMuted[i] ? "1" : "0");
    }
    var flags = {};
    for (let i = 3; i <= 10; i++) { if (channelSammel[i]) flags[i] = true; }
    Cookie.set("chSamFlags", JSON.stringify(flags));
    Cookie.set("chSamGrpsM", JSON.stringify(sammelGroups));
    Cookie.set("chSamNames", JSON.stringify(sammelNames));
}

// ── Auth-State ────────────────────────────────────────────────────────────────
var authRequired = false;
var authNonce    = "";



// ── Render functions (shared between WebSocket push and API fetch) ───────────

var peerSort = { key: null, dir: 1 };
var routingSort = { key: null, dir: 1 };
var peerFilter = "";
var routingFilter = "";

function setPeerFilter(v) {
    peerFilter = v.toLowerCase();
    if (lastPeerData) renderPeerList(lastPeerData);
}
function setRoutingFilter(v) {
    routingFilter = v.toLowerCase();
    if (lastRoutingData) renderRoutingList(lastRoutingData);
}

function ensureTableSkeleton(containerId, tableId, filterId, colspan, placeholder, setter) {
    var c = document.getElementById(containerId);
    if (document.getElementById(tableId)) return;
    var html = "<table id='" + tableId + "' class='mesh-table' style='width:95%;max-width:800px;'>"
             + "<thead>"
             + "<tr class='filter-row'><th colspan='" + colspan + "' style='padding:6px 8px;background:#2e2e2e;text-transform:none;letter-spacing:normal;'>"
             + "<input id='" + filterId + "' type='text' placeholder='" + placeholder + "' "
             + "style='width:100%;box-sizing:border-box;padding:6px 8px;background:#1e1e1e;color:#e0e0e0;border:1px solid #555;border-radius:3px;font-size:0.95em;' />"
             + "</th></tr>"
             + "<tr class='header-row'></tr>"
             + "</thead><tbody></tbody></table>";
    c.innerHTML = html;
    var inp = document.getElementById(filterId);
    inp.addEventListener('input', function() { window[setter](this.value); });
}

function sortBy(arr, key, dir) {
    if (!key) return arr;
    return arr.slice().sort(function(a, b) {
        var av = a[key], bv = b[key];
        if (av == null) av = "";
        if (bv == null) bv = "";
        if (typeof av === "number" && typeof bv === "number") return (av - bv) * dir;
        return String(av).localeCompare(String(bv), undefined, {numeric: true}) * dir;
    });
}

function setPeerSort(key) {
    if (peerSort.key === key) peerSort.dir = -peerSort.dir;
    else { peerSort.key = key; peerSort.dir = 1; }
    if (lastPeerData) renderPeerList(lastPeerData);
}

function setRoutingSort(key) {
    if (routingSort.key === key) routingSort.dir = -routingSort.dir;
    else { routingSort.key = key; routingSort.dir = 1; }
    if (lastRoutingData) renderRoutingList(lastRoutingData);
}

function sortIndicator(sort, key) {
    if (sort.key !== key) return "";
    return sort.dir > 0 ? " \u25B2" : " \u25BC";
}

var lastPeerData = null;
var lastRoutingData = null;

function renderPeerList(data) {
    lastPeerData = data;
    var peerArray = data.peers || (data.peerlist && data.peerlist.peers) || [];
    // Normalize timestamp for sorting
    peerArray = peerArray.map(function(p) {
        var ts = p.timestamp || p.lastSeen || 0;
        return Object.assign({}, p, { _ts: ts, _port: (p.port == 0) ? "LoRa" : "Wifi", _frq: parseInt(p.frqError || 0) });
    });
    if (peerFilter) {
        peerArray = peerArray.filter(function(p) {
            return (String(p.call || "") + " " + p._port + " " + p.rssi + " " + p.snr).toLowerCase().indexOf(peerFilter) !== -1;
        });
    }
    peerArray = sortBy(peerArray, peerSort.key, peerSort.dir);
    ensureTableSkeleton("peer", "peerTable", "peerFilterInput", 6, t('filter.search'), "setPeerFilter");
    var headerRow = document.querySelector("#peerTable .header-row");
    headerRow.innerHTML = ""
        + "<th style='cursor:pointer' onclick=\"setPeerSort('_port')\">" + t('peer.port') + sortIndicator(peerSort, '_port') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setPeerSort('call')\">" + t('peer.call') + sortIndicator(peerSort, 'call') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setPeerSort('_ts')\">" + t('peer.last_rx') + sortIndicator(peerSort, '_ts') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setPeerSort('rssi')\">" + t('peer.rssi') + sortIndicator(peerSort, 'rssi') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setPeerSort('snr')\">" + t('peer.snr') + sortIndicator(peerSort, 'snr') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setPeerSort('_frq')\">" + t('peer.frq_err') + sortIndicator(peerSort, '_frq') + "</th>";
    var body = "";
    peerArray.forEach(function(p) {
        var port = (p.port == 0) ? "LoRa" : "Wifi";
        var ts = p.timestamp || p.lastSeen || 0;
        var lastRX = new Date(ts * 1000);
        body += "<tr>";
        body += "<td><span class='mesh-badge" + (p.port == 0 ? " badge-lora" : " badge-wifi") + "'>" + port + "</span>";
        if (p.preferred === true) body += " <span class='badge-preferred' title='" + t('peer.preferred') + "'>&#9733;</span>";
        body += "</td>";
        var cls = p.available ? 'green' : (p.preferred === false ? 'suppressed' : 'red');
        body += "<td class='" + cls + "'>" + esc(p.call) + "</td>";
        body += "<td>" + lastRX.toLocaleString("de-DE", {day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit"}).replace(",", "") + "</td>";
        body += "<td>" + p.rssi + "</td>";
        body += "<td>" + p.snr + "</td>";
        body += "<td>" + parseInt(p.frqError || 0) + "</td>";
        body += "</tr>";
    });
    document.querySelector("#peerTable tbody").innerHTML = body;
}

function renderRoutingList(data) {
    lastRoutingData = data;
    var routeArray = data.routes || (data.routingList && data.routingList.routes) || [];
    routeArray = routeArray.map(function(r) {
        return Object.assign({}, r, {
            _src: r.srcCall || r.dest || "",
            _via: r.viaCall || r.via || "",
            _hops: (r.hopCount != null ? r.hopCount : (r.hops || 0)),
            _ts: r.timestamp || 0
        });
    });
    // Hide direct peers (already shown in peer list): route where via == src
    routeArray = routeArray.filter(function(r) {
        return r._src && r._via && r._src !== r._via;
    });
    if (routingFilter) {
        routeArray = routeArray.filter(function(r) {
            return (r._src + " " + r._via + " " + r._hops).toLowerCase().indexOf(routingFilter) !== -1;
        });
    }
    routeArray = sortBy(routeArray, routingSort.key, routingSort.dir);
    ensureTableSkeleton("routing", "routingTable", "routingFilterInput", 4, t('filter.search'), "setRoutingFilter");
    if (!document.getElementById("sendRoutesBtn")) {
        var filterTh = document.querySelector("#routingTable .filter-row th");
        var input = document.getElementById("routingFilterInput");
        filterTh.innerHTML = "";
        var wrap = document.createElement("div");
        wrap.style.cssText = "display:flex;align-items:center;justify-content:space-between;gap:8px;";
        input.style.width = "25%";
        input.style.boxSizing = "border-box";
        wrap.appendChild(input);
        var btn = document.createElement("button");
        btn.id = "sendRoutesBtn";
        btn.type = "button";
        btn.textContent = t('route.send_now');
        btn.style.cssText = "padding:6px 10px;background:#1e1e1e;color:#e0e0e0;border:1px solid #555;border-radius:3px;font-size:0.9em;cursor:pointer;";
        btn.onclick = function() {
            sendWS(JSON.stringify({sendRoutes: true}));
            if (typeof msgBox === "function") msgBox(t('route.send_now') + " sent.");
        };
        wrap.appendChild(btn);
        filterTh.appendChild(wrap);
    } else {
        document.getElementById("sendRoutesBtn").textContent = t('route.send_now');
    }
    var headerRow = document.querySelector("#routingTable .header-row");
    headerRow.innerHTML = ""
        + "<th style='cursor:pointer' onclick=\"setRoutingSort('_src')\">" + t('route.call') + sortIndicator(routingSort, '_src') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setRoutingSort('_via')\">" + t('route.node') + sortIndicator(routingSort, '_via') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setRoutingSort('_hops')\">" + t('route.hops') + sortIndicator(routingSort, '_hops') + "</th>"
        + "<th style='cursor:pointer' onclick=\"setRoutingSort('_ts')\">" + t('route.last_rx') + sortIndicator(routingSort, '_ts') + "</th>";
    var body = "";
    routeArray.forEach(function(r) {
        var ts = r.timestamp || 0;
        var lastRX = new Date(ts * 1000);
        body += "<tr>";
        body += "<td>" + esc(r.srcCall || r.dest || "") + "</td>";
        body += "<td>" + esc(r.viaCall || r.via || "") + "</td>";
        body += "<td>" + (r.hopCount != null ? r.hopCount : (r.hops || 0)) + "</td>";
        body += "<td>" + lastRX.toLocaleString("de-DE", {day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit"}).replace(",", "") + "</td>";
        body += "</tr>";
    });
    document.querySelector("#routingTable tbody").innerHTML = body;
}

function onSettingsReceived(s) {
    // Merge with existing settings so partial updates don't lose fields
    if (settings) {
        for (var k in s) { settings[k] = s[k]; }
    } else {
        settings = s;
    }
    s = settings;
    function fmtIP(a) { return (a && a[0] !== undefined) ? a[0]+'.'+a[1]+'.'+a[2]+'.'+a[3] : '-'; }
    // Only call fillSettingsForm when full settings are available (has wifiIP)
    if (s.wifiIP) {
        try { fillSettingsForm(settings); }
        catch(e) { console.error("fillSettingsForm error:", e); }
    }
    var appText = document.getElementById("appNameText");
    if (appText && s.name) appText.textContent = s.name;
    if (s.version) document.getElementById("version").innerHTML = s.version;
    if (s.mycall != null) document.getElementById("myCall").innerHTML = s.mycall;
    if (s.loraMaxMessageLength) document.getElementById("settingsLoraMaxMessageLength").innerHTML = s.loraMaxMessageLength + " characters";
    if (settings.name && settings.mycall) {
        settings.titel = settings.name + " - " + settings.mycall;
        settings.altTitel = "\ud83d\udea8 " + settings.name + " - " + settings.mycall + " \ud83d\udea8";
    }
    document.getElementById("currentWiFiIP").textContent      = fmtIP(s.currentIP);
    document.getElementById("currentWifiNetMask").textContent  = fmtIP(s.currentNetMask);
    document.getElementById("currentWifiGateway").textContent  = fmtIP(s.currentGateway);
    document.getElementById("currentWifiDNS").textContent      = fmtIP(s.currentDNS);
    if (s.udpPeers) {
        renderUdpPeers(s.udpPeers);
    }
    captureSettingsSnapshot();
    var chipIdEl = document.getElementById("aboutChipId");
    if (chipIdEl) chipIdEl.innerHTML = s.chipId || "";
    if (s.mycall) setMsgCacheKey(s.mycall);
    var hwEl = document.getElementById("aboutHardware");
    if (hwEl) hwEl.innerHTML = s.hardware || "";
    var aboutVersionEl = document.getElementById("aboutVersion");
    if (aboutVersionEl) aboutVersionEl.innerHTML = (s.name || "") + " " + (s.version || "");
    var aboutChangelogEl = document.getElementById("aboutChangelog");
    if (aboutChangelogEl) aboutChangelogEl.textContent = s.changelog || "";
    var hasBat = s.hasBattery === true;
    var batEnabled = s.batteryEnabled !== false;
    var batGpRow = document.getElementById("batteryGpRow");
    if (batGpRow) batGpRow.style.display = (hasBat && batEnabled) ? "" : "none";
    var batSettingsSection = document.getElementById("batterySettingsSection");
    if (batSettingsSection) batSettingsSection.style.display = hasBat ? "" : "none";
    var pwStatus = document.getElementById("settingsWebPasswordStatus");
    var pwRemoveRow = document.getElementById("settingsWebPasswordRemoveRow");
    if (pwStatus) {
        if (s.webPasswordSet) {
            pwStatus.textContent = typeof t === 'function' ? t('pw.set') : 'Password is set';
            pwStatus.style.color = "#4ecca3";
            if (pwRemoveRow) pwRemoveRow.style.display = "";
        } else {
            pwStatus.textContent = typeof t === 'function' ? t('pw.not_set') : 'No password set';
            pwStatus.style.color = "";
            if (pwRemoveRow) pwRemoveRow.style.display = "none";
        }
    }
    if (s.groupNames) {
        var pushToDevice = {};
        var needsPush = false;
        for (var i = 3; i <= 10; i++) {
            var deviceName = s.groupNames[String(i)] || "";
            var cookieName = Cookie.get("channel" + i) || "";
            if (deviceName) {
                Cookie.set("channel" + i, deviceName);
            } else if (cookieName && cookieName !== "........") {
                pushToDevice[String(i)] = cookieName;
                needsPush = true;
            }
        }
        if (needsPush) {
            sendWS(JSON.stringify({settings: {groupNames: pushToDevice}}));
        }
    }
    if (init == false && s.wifiIP && s.mycall) {
        init = true;
        fetch(baseURL + "messages.json?" + Math.random())
            .then(function(response) {
                if (!response.ok) throw new Error("HTTP " + response.status);
                return response.text();
            })
            .then(function(text) {
                var loaded = [];
                if (text) {
                    var lines = text.split(/\r?\n/);
                    lines.forEach(function(line) {
                        if (line.trim().length === 0) return;
                        try {
                            var m = JSON.parse(line);
                            m.message.parsed = false;
                            loaded.push(m.message);
                        } catch(e) {}
                    });
                }
                // Always merge with the per-callsign cache so the browser
                // never *loses* history just because the node returned a
                // smaller window (or was wiped). The cache is treated as the
                // authoritative full archive on the client side; the node is
                // the cross-client sync layer.
                messages = mergeWithCache(loaded);

                // If the cache has entries the node doesn't know about — be
                // it because the node was wiped (FS flash, deleteMessages) or
                // because /messages.json got truncated — push the missing
                // ones back up so the node (and every other client) recovers.
                try {
                    var knownIds = {};
                    loaded.forEach(function(m) { if (m.id) knownIds[m.srcCall + "_" + m.id] = true; });
                    var extra = (loadCachedMessages() || []).filter(function(m) {
                        return m.id && m.srcCall && !knownIds[m.srcCall + "_" + m.id];
                    });
                    if (extra.length > 0) {
                        fetch(baseURL + "api/messages/import", {
                            method: "POST",
                            headers: {"Content-Type": "application/json"},
                            body: JSON.stringify({messages: extra})
                        }).then(function(r) {
                            if (r.ok) console.log("Restored " + extra.length + " messages to node");
                        }).catch(function(e) { console.warn("import failed:", e); });
                    }
                } catch(e) { /* localStorage unavailable */ }

                saveCachedMessages();
                messages.push({"delimiter": true});
                showMessages(true);
                for (let i = 0; i <= 10; i++) {channels[i] = false;}
                setUI(ui);
            })
            .catch(function(err) {
                console.warn("messages.json fetch failed, using cache:", err);
                var cached = loadCachedMessages();
                if (cached.length > 0) {
                    cached.forEach(function(m) { m.parsed = false; });
                    messages = cached;
                    // messages.json missing usually means the node was wiped
                    // (filesystem flash, deleteMessages). Push the cache back
                    // so the node and other clients can recover.
                    var extra = cached.filter(function(m) { return m.id && m.srcCall; });
                    if (extra.length > 0) {
                        fetch(baseURL + "api/messages/import", {
                            method: "POST",
                            headers: {"Content-Type": "application/json"},
                            body: JSON.stringify({messages: extra})
                        }).then(function(r) {
                            if (r.ok) console.log("Restored " + extra.length + " messages to node (catch)");
                        }).catch(function(e) { console.warn("import failed:", e); });
                    }
                }
                messages.push({"delimiter": true});
                showMessages(true);
                for (let i = 0; i <= 10; i++) {channels[i] = false;}
                setUI(ui);
            });
    }
}

function onMessage(event) {
    var d;
    try { d = JSON.parse(event.data); } catch(e) { console.error("Invalid JSON:", e); return; }
    if (d.status === undefined) {console.log("RX: " + event.data);}

    // ── Auth-Flow ─────────────────────────────────────────────────────────────
    if (d.auth) {
        if (d.auth.mycall) setMsgCacheKey(d.auth.mycall);
        if (d.auth.required) {
            authRequired = true;
            authNonce    = d.auth.nonce;
            showAuthOverlay(d.auth.mycall, d.auth.chipId);
        }
        if (d.auth.ok) {
            authRequired = false;
            hideAuthOverlay();
        }
        if (d.auth.error) {
            authNonce = d.auth.nonce;  // new nonce for next attempt
            showAuthError(d.auth.error);
        }
        return;
    }

    // ── Password saved ──────────────────────────────────────────────────
    if (d.passwordSaved !== undefined) {
        if (d.passwordSaved) {
            msgBox("Password saved. Reloading page...");
            setTimeout(() => location.reload(), 2000);
        } else {
            msgBox("Password removed.");
        }
        return;
    }

    // Ignore messages while auth is pending
    if (authRequired) return;

    // ── Notification events (lightweight, fetch data via REST API) ────────────
    if (d.notify) {
        var fetchUrl = baseURL + "api/" + d.notify;
        console.log("Fetching: " + fetchUrl);
        fetch(fetchUrl).then(function(r) {
            if (!r.ok) throw new Error("HTTP " + r.status);
            return r.json();
        }).then(function(data) {
            console.log("Fetched " + d.notify + ":", Object.keys(data));
            switch (d.notify) {
                case "peers":
                    console.log("Peers count:", (data.peers||[]).length);
                    renderPeerList(data);
                    break;
                case "routes":
                    console.log("Routes count:", (data.routes||[]).length);
                    renderRoutingList(data);
                    break;
                case "settings":
                    if (data.settings) onSettingsReceived(data.settings);
                    break;
            }
        }).catch(function(e) {
            console.error("Failed to fetch " + d.notify + ":", e);
        });
        return;
    }

    //RAW-RX
    if (d.monitor) {
        var f = d.monitor;
        var msg = ""; 
        //TX-Frame gelb
        if (d.monitor.tx == true) { msg += "<span class='monitor-tx'>→ "; } else { msg += "<span>← "; }
        //Port
        if (f.port == 0) {msg += "LoRa";} else {msg += "Wifi";}
        //Time
        const date = new Date(d.monitor.timestamp * 1000);
        msg += " " + date.toLocaleString("de-DE", {hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", "");		
        //Display readable
        if (typeof f.nodeCall !== "undefined") { msg += " " + f.nodeCall ; }
        if (typeof f.viaCall !== "undefined") { msg += " > " + f.viaCall ; }
        if (typeof f.hopCount !== "undefined") { 
            if (f.hopCount > 0) { 
                msg += " H" + f.hopCount; 
            }
        }
        switch (f.frameType) {
            case 0x00: msg += " Announce"; break;
            case 0x01: msg += " Announce ACK"; break;
            case 0x02: msg += " Tuning"; break;
            case 0x03: 
            case 0x05: 
                if (f.id) { msg += " ID: " + esc(f.id) ; }
                if (f.srcCall) { msg += " SRC: " + esc(f.srcCall); }
                if (f.dstCall) { msg += " DST: " + esc(f.dstCall); }
                if (f.dstGroup) { msg += " GRP: " + esc(f.dstGroup); }
                if (f.messageType == 0) {msg += " TEXT: ";}
                if (f.messageType == 1) {msg += " TRACE: ";}
                if (f.messageType == 14) {msg += " ROUTE: ";}
                if (f.messageType == 15) {msg += " COMMAND: ";}
                if (f.text) { msg += esc(f.text); }
                break;
            case 0x04: 
                msg += " Message ACK "; 
                if (f.id > 0) { msg += " ID: " + f.id + " "; }
                break;
        }
        msg += "</span>";
        var monitorEl = document.getElementById("monitor");
        monitorEl.insertAdjacentHTML('beforeend', msg);
        while (monitorEl.children.length > 200) { monitorEl.removeChild(monitorEl.firstChild); }
        monitorEl.scrollTop = monitorEl.scrollHeight;
    }

    //Message received
    if (d.message) {
        d.message.parsed = false;
        messages.push(d.message);
        _msgCacheDirty = true;
        showMessages();
    }

    //Peers (legacy WebSocket push fallback)
    if (d.peerlist) {
        renderPeerList(d);
    }

    //Routing list (legacy WebSocket push fallback)
    if (d.routingList) {
        renderRoutingList(d);
    }

    //Settings (legacy WebSocket push fallback)
    if (d.settings) {
        onSettingsReceived(d.settings);
    }

    //Status
    if (d.status) {
        drawClock(new Date(d.status.time * 1000));
        if (d.status.tx) {
            document.getElementById("TRX").innerHTML = "<span style='color: #d9ff00;'> << TX >> </span>"; 
        } else if (d.status.rx) {
            document.getElementById("TRX").innerHTML = "<span style='color: #00ff00;'> >> RX << </span>"; 
        } else {
            document.getElementById("TRX").innerHTML = "<span>stby</span>"; 
        }
        document.getElementById("txBuffer").innerHTML = d.status.txBufferCount;
        document.getElementById("retry").innerHTML = d.status.retry;
        if (d.status.dropped != null) {
            var dropEl = document.getElementById("dropped");
            if (dropEl) dropEl.innerHTML = d.status.dropped;
        }
        document.getElementById("heap").innerHTML = d.status.heap + (d.status.minHeap != null ? " (min: " + d.status.minHeap + ")" : "");
        if (d.status.uptime != null) {
            var s = d.status.uptime;
            var d2 = Math.floor(s / 86400); s %= 86400;
            var h = Math.floor(s / 3600); s %= 3600;
            var m = Math.floor(s / 60); s %= 60;
            var parts = [];
            if (d2 > 0) parts.push(d2 + "d");
            parts.push(h + "h " + m + "m " + s + "s");
            var upEl = document.getElementById("aboutUptime");
            if (upEl) upEl.innerHTML = parts.join(" ") + (d.status.resetReason ? " (" + d.status.resetReason + ")" : "");
        }
        if (d.status.cpuFreq != null) {
            pushCpuFreq(d.status.cpuFreq);
        }
        if (d.status.battery != null) {
            var bv = d.status.battery;
            var fullV = (settings && settings.batteryFullVoltage) || 4.2;
            var pct = Math.round(Math.min(100, Math.max(0, (bv - 3.0) / (fullV - 3.0) * 100)));
            var battEl = document.getElementById("battery");
            if (battEl) battEl.innerHTML = bv.toFixed(2) + " V (" + pct + "%)";
            var battElGp = document.getElementById("batteryGp");
            if (battElGp) battElGp.innerHTML = bv.toFixed(2) + " V (" + pct + "%)";
        }
    }

    //Update available
    if (d.update) {
        var el = document.getElementById("updateInfo");
        if (el) {
            el.innerHTML = 'Update ' + d.update.version + ' available! <a href="' + d.update.url + '" target="_blank">www.rMesh.de</a>';
            el.style.display = '';
        }
        var row = document.getElementById("updateRow");
        if (row) { row.style.display = ''; }
    }

    //Update-Status
    if (d.updateStatus !== undefined) {
        msgBox(d.updateStatus);
    }

    //WiFi Scan
    if (d.wifiScan) {
        const select = document.getElementById('settingsSSIDList');
        select.length = 0;
        d.wifiScan.forEach(n => {
            let opt = document.createElement('option');
            opt.value = n.ssid;
            opt.text = n.ssid + " (" + n.encryption + ", " + n.rssi +  "dBm)";
            select.add(opt);
        });
    }

}		

//Send message
async function sendMessage(text, channel) {
    // Collection groups are receive-only
    if (channel >= 3 && channelSammel[channel]) return;
    //Build destination callsign
    var dstCall = document.getElementById('dstCall').innerHTML;
    if (channel == 2) {
        dstCall = (await inputBox("Destination Call?", Cookie.get("channel2"), document.getElementById('messageText' + channel) )).toUpperCase();
        Cookie.set("channel2", dstCall);
    }
    if (dstCall == "") {return;}
    if (dstCall == "all") dstCall = "";
    //Prepare message and send via WebSocket
    var message = {};
    message["text"] = text;
    message["dst"] = dstCall;
    if (channel == 2) {
        //Private message
        sendWS(JSON.stringify({sendMessage: message}));                    
    } else {
        //Group
        sendWS(JSON.stringify({sendGroup: message}));                    
    }
}

// ── LoRa frequency presets ─────────────────────────────────────────────────────
const LORA_PRESETS = {
    '433': {
        frequency:       434.850,
        bandwidth:       62.5,
        spreadingFactor: 7,
        codingRate:      6,
        outputPower:     20,
        preambleLength:  10,
        syncWord:        '2B',
    },
    '868': {
        frequency:       869.525,
        bandwidth:       125,
        spreadingFactor: 7,
        codingRate:      5,
        outputPower:     27,
        preambleLength:  10,
        syncWord:        '12',
    }
};

function applyLoraPreset(band) {
    const p = LORA_PRESETS[band];
    if (!p) return;
    document.getElementById('settingsLoraFrequency').value       = p.frequency;
    document.getElementById('settingsLoraBandwidth').value       = p.bandwidth;
    document.getElementById('settingsLoraSpreadingFactor').value = p.spreadingFactor;
    document.getElementById('settingsLoraCodingRate').value      = p.codingRate;
    document.getElementById('settingsLoraOutputPower').value     = p.outputPower;
    document.getElementById('settingsLoraPreambleLength').value  = p.preambleLength;
    document.getElementById('settingsLoraSyncWord').value        = p.syncWord;
    document.getElementById('loraPreset').value                  = band;
}

function onFrequencyChange() {
    const newFreq = parseFloat(document.getElementById('settingsLoraFrequency').value);
    if (isNaN(newFreq) || newFreq === 0) return;

    let newBand = null;
    if (newFreq >= 430 && newFreq <= 440)        newBand = '433';
    else if (newFreq >= 869.4 && newFreq <= 869.65) newBand = '868';
    if (!newBand) return;

    const currentBand = document.getElementById('loraPreset').value;
    if (newBand !== currentBand) {
        applyLoraPreset(newBand);
        // Keep the entered frequency, only load remaining parameters from preset
        document.getElementById('settingsLoraFrequency').value = newFreq;
    }
}

function udpPeerToggle(checked) {
    return '<label class="toggle-switch">'
        + '<input type="checkbox"' + (checked ? ' checked' : '') + '>'
        + '<span class="toggle-track"><span class="toggle-thumb"></span></span>'
        + '</label>';
}

function renderUdpPeers(peers) {
    var list = document.getElementById('udpPeerList');
    if (!list) return;
    list.innerHTML = '<table class="udpPeerTable">'
        + '<thead><tr>'
        + '<th>' + t('peer.call') + '</th>'
        + '<th>IP</th>'
        + '<th>' + t('udp.legacy') + '</th>'
        + '<th>' + t('udp.active') + '</th>'
        + '<th></th>'
        + '</tr></thead>'
        + '<tbody id="udpPeerBody"></tbody></table>';
    peers.forEach(function(p) {
        var tbody = document.getElementById('udpPeerBody');
        var tr = document.createElement('tr');
        tr.className = 'udpPeerRow';
        var legacySwitch = udpPeerToggle(!!p.legacy);
        var enabledSwitch = udpPeerToggle(p.enabled !== false);
        tr.innerHTML = '<td><span class="udpPeerCall">' + (p.call || '–') + '</span></td>'
            + '<td><input class="input-box udpPeerIP" value="' + p.ip.join('.') + '"></td>'
            + '<td class="udpPeerLegacyCell"><span class="toggle-label">' + t('udp.legacy') + '</span> ' + legacySwitch + '</td>'
            + '<td class="udpPeerEnabledCell"><span class="toggle-label">' + t('udp.active') + '</span> ' + enabledSwitch + '</td>'
            + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()" title="' + t('btn.remove_peer') + '">&#128465;</button></td>';
        tbody.appendChild(tr);
    });
}

function addUdpPeer() {
    var tbody = document.getElementById('udpPeerBody');
    if (!tbody) { renderUdpPeers([]); tbody = document.getElementById('udpPeerBody'); }
    var tr = document.createElement('tr');
    tr.className = 'udpPeerRow';
    tr.innerHTML = '<td><span class="udpPeerCall">–</span></td>'
        + '<td><input class="input-box udpPeerIP" value=""></td>'
        + '<td class="udpPeerLegacyCell"><span class="toggle-label">' + t('udp.legacy') + '</span> ' + udpPeerToggle(false) + '</td>'
        + '<td class="udpPeerEnabledCell"><span class="toggle-label">' + t('udp.active') + '</span> ' + udpPeerToggle(true) + '</td>'
        + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()" title="' + t('btn.remove_peer') + '">&#128465;</button></td>';
    tbody.appendChild(tr);
}

// ── WiFi Network Management ──────────────────────────────────────────────────
function wifiFavRadio(checked) {
    return '<label class="toggle-switch">'
        + '<input type="checkbox"' + (checked ? ' checked' : '') + ' onchange="onWifiFavChange(this)">'
        + '<span class="toggle-track"><span class="toggle-thumb"></span></span>'
        + '</label>';
}

function onWifiFavChange(el) {
    // Only one favorite at a time: uncheck all others
    if (el.checked) {
        document.querySelectorAll('#wifiNetworkList .wifiNetFav input').forEach(function(cb) {
            if (cb !== el) cb.checked = false;
        });
    }
}

function renderWifiNetworks(nets) {
    var list = document.getElementById('wifiNetworkList');
    if (!list) return;
    list.innerHTML = '<table class="wifiNetTable">'
        + '<colgroup><col class="col-ssid"><col class="col-pw"><col class="col-fav"><col class="col-del"></colgroup>'
        + '<thead><tr>'
        + '<th>SSID</th>'
        + '<th>' + t('net.password') + '</th>'
        + '<th>' + t('net.favorite') + '</th>'
        + '<th></th>'
        + '</tr></thead>'
        + '<tbody id="wifiNetBody"></tbody></table>';
    nets.forEach(function(n) {
        var tbody = document.getElementById('wifiNetBody');
        var tr = document.createElement('tr');
        tr.className = 'wifiNetRow';
        tr.innerHTML = '<td><input class="input-box wifiNetSSID" value="' + (n.ssid || '') + '"></td>'
            + '<td><input class="input-box wifiNetPW" type="password" value="' + (n.password || '') + '"></td>'
            + '<td class="wifiNetFav"><span class="toggle-label">' + t('net.favorite') + '</span> ' + wifiFavRadio(!!n.favorite) + '</td>'
            + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()" title="' + t('btn.remove_peer') + '">&#128465;</button></td>';
        tbody.appendChild(tr);
    });
}

function addWifiNetwork() {
    var tbody = document.getElementById('wifiNetBody');
    if (!tbody) { renderWifiNetworks([]); tbody = document.getElementById('wifiNetBody'); }
    var tr = document.createElement('tr');
    tr.className = 'wifiNetRow';
    var isFav = (tbody.querySelectorAll('.wifiNetRow').length === 0);
    tr.innerHTML = '<td><input class="input-box wifiNetSSID" value=""></td>'
        + '<td><input class="input-box wifiNetPW" type="password" value=""></td>'
        + '<td class="wifiNetFav"><span class="toggle-label">' + t('net.favorite') + '</span> ' + wifiFavRadio(isFav) + '</td>'
        + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()" title="' + t('btn.remove_peer') + '">&#128465;</button></td>';
    tbody.appendChild(tr);
    tr.querySelector('.wifiNetSSID').focus();
}

function addWifiNetworkFromScan() {
    var select = document.getElementById('settingsSSIDList');
    if (!select || !select.value) return;
    var tbody = document.getElementById('wifiNetBody');
    if (!tbody) { renderWifiNetworks([]); tbody = document.getElementById('wifiNetBody'); }
    // Check if SSID already exists in list
    var exists = false;
    tbody.querySelectorAll('.wifiNetSSID').forEach(function(input) {
        if (input.value === select.value) exists = true;
    });
    if (exists) return;
    var tr = document.createElement('tr');
    tr.className = 'wifiNetRow';
    var isFav = (tbody.querySelectorAll('.wifiNetRow').length === 0);
    tr.innerHTML = '<td><input class="input-box wifiNetSSID" value="' + select.value + '"></td>'
        + '<td><input class="input-box wifiNetPW" type="password" value=""></td>'
        + '<td class="wifiNetFav"><span class="toggle-label">' + t('net.favorite') + '</span> ' + wifiFavRadio(isFav) + '</td>'
        + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()" title="' + t('btn.remove_peer') + '">&#128465;</button></td>';
    tbody.appendChild(tr);
    tr.querySelector('.wifiNetPW').focus();
}

function fillSettingsForm(s) {
    document.getElementById("settingsMycall").value = s.mycall;
    document.getElementById("settingsPosition").value = s.position || "";
    document.getElementById("settingsNTP").value = s.ntp;
    document.getElementById("settingsWiFiIP").value = s.wifiIP[0] + "." + s.wifiIP[1] + "." + s.wifiIP[2] + "." + s.wifiIP[3];
    document.getElementById("settingsWifiNetMask").value = s.wifiNetMask[0] + "." + s.wifiNetMask[1] + "." + s.wifiNetMask[2] + "." + s.wifiNetMask[3];
    document.getElementById("settingsWifiGateway").value = s.wifiGateway[0] + "." + s.wifiGateway[1] + "." + s.wifiGateway[2] + "." + s.wifiGateway[3];
    document.getElementById("settingsWifiDNS").value = s.wifiDNS[0] + "." + s.wifiDNS[1] + "." + s.wifiDNS[2] + "." + s.wifiDNS[3];
    document.getElementById("settingsDHCP").checked = s.dhcpActive;
    document.getElementById("settingsApMode").checked = s.apMode;
    document.getElementById("settingsApName").value = s.apName || "rMesh";
    document.getElementById("settingsApPassword").value = s.apPassword || "";
    // WiFi network list
    if (s.wifiNetworks) {
        renderWifiNetworks(s.wifiNetworks);
    }
    document.getElementById("settingsLoraFrequency").value = s.loraFrequency;
    document.getElementById("settingsLoraOutputPower").value = s.loraOutputPower;
    document.getElementById("settingsLoraBandwidth").value = s.loraBandwidth;
    document.getElementById("settingsLoraSyncWord").value = s.loraSyncWord.toString(16).padStart(2, '0').toUpperCase();
    document.getElementById("settingsLoraCodingRate").value = s.loraCodingRate;
    document.getElementById("settingsLoraSpreadingFactor").value = s.loraSpreadingFactor;
    document.getElementById("settingsLoraPreambleLength").value = s.loraPreambleLength;
    const presetEl = document.getElementById("loraPreset");
    if (presetEl) {
        const freq = s.loraFrequency;
        if (freq >= 430 && freq <= 440)            presetEl.value = "433";
        else if (freq >= 869.4 && freq <= 869.65)  presetEl.value = "868";
        else                                       presetEl.value = "";
    }
    document.getElementById("settingsLoraRepeat").checked = s.loraRepeat;
    document.getElementById("settingsLoraEnabled").checked = s.loraEnabled !== false;
    var minSnrEl = document.getElementById("settingsMinSnr");
    if (minSnrEl) {
        var v = (s.minSnr !== undefined) ? s.minSnr : -20;
        if (v < -30) v = -30;
        minSnrEl.value = v;
        updateMinSnrLabel(v);
    }
    document.getElementById("settingsUpdateChannel").value = s.updateChannel || 0;
    const batEnabledEl = document.getElementById("settingsBatteryEnabled");
    if (batEnabledEl) batEnabledEl.checked = s.batteryEnabled !== false;
    const batVoltEl = document.getElementById("settingsBatteryFullVoltage");
    if (batVoltEl) batVoltEl.value = s.batteryFullVoltage || 4.2;

    // WiFi TX power
    var wifiTxEl = document.getElementById("settingsWifiTxPower");
    if (wifiTxEl) {
        var maxPow = s.wifiMaxTxPower || 20;
        wifiTxEl.max = maxPow;
        wifiTxEl.value = s.wifiTxPower || maxPow;
        updateWifiTxPowerLabel(parseInt(wifiTxEl.value));
    }

    // Display brightness
    var dispBrEl = document.getElementById("settingsDisplayBrightness");
    if (dispBrEl) {
        dispBrEl.value = s.displayBrightness || 200;
        updateDispBrightnessLabel(parseInt(dispBrEl.value));
    }

    // CPU frequency
    var cpuFreqEl = document.getElementById("settingsCpuFrequency");
    if (cpuFreqEl) cpuFreqEl.value = s.cpuFrequency || 240;

    // Serial debug
    var serialDebugEl = document.getElementById("settingsSerialDebug");
    if (serialDebugEl) serialDebugEl.checked = s.serialDebug === true;

    // Heap debug (heap instrumentation / ring buffer)
    var heapDebugEl = document.getElementById("settingsHeapDebug");
    if (heapDebugEl) heapDebugEl.checked = s.heapDebug === true;

    // OLED display settings
    var oledEnabledEl = document.getElementById("settingsOledEnabled");
    if (oledEnabledEl) oledEnabledEl.checked = s.oledEnabled === true;
    var oledGroupEl = document.getElementById("settingsOledDisplayGroup");
    if (oledGroupEl) {
        var saved = s.oledDisplayGroup || "";
        oledGroupEl.innerHTML = '<option value="">---</option>';
        var allOpt = document.createElement("option");
        allOpt.value = "*";
        allOpt.textContent = "1: all";
        if (saved === "*") allOpt.selected = true;
        oledGroupEl.appendChild(allOpt);
        var dmOpt = document.createElement("option");
        dmOpt.value = "@DM";
        dmOpt.textContent = "2: direct";
        if (saved === "@DM") dmOpt.selected = true;
        oledGroupEl.appendChild(dmOpt);
        for (var i = 3; i <= 10; i++) {
            var grp = Cookie.get("channel" + i);
            if (grp) {
                var opt = document.createElement("option");
                opt.value = grp;
                opt.textContent = i + ": " + grp;
                if (grp === saved) opt.selected = true;
                oledGroupEl.appendChild(opt);
            }
        }
    }

    // OLED rotation interval (ms → s)
    var oledIvEl = document.getElementById("settingsOledPageInterval");
    if (oledIvEl && typeof s.oledPageInterval === "number") {
        oledIvEl.value = Math.round(s.oledPageInterval / 1000);
    }

    // OLED page mask → 5 checkboxes
    var oledMask = (typeof s.oledPageMask === "number") ? s.oledPageMask : 0xFF;
    var pageEls = [
        ["settingsOledPageIdentity", 0],
        ["settingsOledPageNetwork",  1],
        ["settingsOledPageLora",     2],
        ["settingsOledPageMessages", 3],
        ["settingsOledPageSystem",   4]
    ];
    pageEls.forEach(function(p) {
        var el = document.getElementById(p[0]);
        if (el) el.checked = (oledMask & (1 << p[1])) !== 0;
    });

    // OLED button GPIO
    var oledBtnEl = document.getElementById("settingsOledButtonPin");
    if (oledBtnEl && typeof s.oledButtonPin === "number") {
        oledBtnEl.value = s.oledButtonPin;
    }

    settingsVisibility();
}

function saveSettings() {
    // ── Check password fields ────────────────────────────────────────────────
    var pw1 = document.getElementById("settingsWebPassword").value;
    var pw2 = document.getElementById("settingsWebPasswordConfirm").value;
    if (pw1 !== pw2) {
        msgBox("Passwords do not match!");
        return;
    }

    var s = {};
    s["mycall"] = document.getElementById("settingsMycall").value;
    s["position"] = document.getElementById("settingsPosition").value;
    s["ntp"] = document.getElementById("settingsNTP").value;
    s["dhcpActive"] = document.getElementById("settingsDHCP").checked;
    s["apMode"] = document.getElementById("settingsApMode").checked;
    s["apName"] = document.getElementById("settingsApName").value;
    s["apPassword"] = document.getElementById("settingsApPassword").value;
    // Collect WiFi networks from table
    s["wifiNetworks"] = [];
    document.querySelectorAll('#wifiNetworkList .wifiNetRow').forEach(function(row) {
        var ssid = row.querySelector('.wifiNetSSID').value || "";
        var pw   = row.querySelector('.wifiNetPW').value || "";
        var fav  = row.querySelector('.wifiNetFav input').checked;
        if (ssid !== "") {
            s["wifiNetworks"].push({ "ssid": ssid, "password": pw, "favorite": fav });
        }
    });
    s["wifiIP"] = document.getElementById("settingsWiFiIP").value.split('.').map(Number);
    s["wifiNetMask"] = document.getElementById("settingsWifiNetMask").value.split('.').map(Number);
    s["wifiGateway"] = document.getElementById("settingsWifiGateway").value.split('.').map(Number);
    s["wifiDNS"] = document.getElementById("settingsWifiDNS").value.split('.').map(Number);
    s["loraFrequency"] = parseFloat(document.getElementById("settingsLoraFrequency").value);
    s["loraOutputPower"] = parseInt(document.getElementById("settingsLoraOutputPower").value);
    s["loraBandwidth"] = parseFloat(document.getElementById("settingsLoraBandwidth").value);
    s["loraSyncWord"] = parseInt(document.getElementById("settingsLoraSyncWord").value, 16);
    s["loraCodingRate"] = parseInt(document.getElementById("settingsLoraCodingRate").value);
    s["loraSpreadingFactor"] = parseInt(document.getElementById("settingsLoraSpreadingFactor").value);
    s["loraPreambleLength"] = parseInt(document.getElementById("settingsLoraPreambleLength").value);
    s["loraRepeat"] = document.getElementById("settingsLoraRepeat").checked;
    s["loraEnabled"] = document.getElementById("settingsLoraEnabled").checked;
    var minSnrEl = document.getElementById("settingsMinSnr");
    if (minSnrEl) s["minSnr"] = parseInt(minSnrEl.value);
    s["updateChannel"] = parseInt(document.getElementById("settingsUpdateChannel").value);
    var batEnabledEl = document.getElementById("settingsBatteryEnabled");
    if (batEnabledEl) s["batteryEnabled"] = batEnabledEl.checked;
    var batVoltEl = document.getElementById("settingsBatteryFullVoltage");
    if (batVoltEl) s["batteryFullVoltage"] = parseFloat(batVoltEl.value);
    var wifiTxEl = document.getElementById("settingsWifiTxPower");
    if (wifiTxEl) s["wifiTxPower"] = parseInt(wifiTxEl.value);
    var dispBrEl = document.getElementById("settingsDisplayBrightness");
    if (dispBrEl) s["displayBrightness"] = parseInt(dispBrEl.value);
    var oledEnabledEl = document.getElementById("settingsOledEnabled");
    if (oledEnabledEl) s["oledEnabled"] = oledEnabledEl.checked;
    var cpuFreqEl = document.getElementById("settingsCpuFrequency");
    if (cpuFreqEl) s["cpuFrequency"] = parseInt(cpuFreqEl.value);
    var serialDebugEl = document.getElementById("settingsSerialDebug");
    if (serialDebugEl) s["serialDebug"] = serialDebugEl.checked;
    var heapDebugEl = document.getElementById("settingsHeapDebug");
    if (heapDebugEl) s["heapDebug"] = heapDebugEl.checked;
    var oledGroupEl = document.getElementById("settingsOledDisplayGroup");
    if (oledGroupEl) s["oledDisplayGroup"] = oledGroupEl.value;
    var oledIvEl = document.getElementById("settingsOledPageInterval");
    if (oledIvEl) {
        var iv = parseInt(oledIvEl.value);
        if (!isNaN(iv)) s["oledPageInterval"] = Math.max(1, Math.min(60, iv)) * 1000;
    }
    var pageBits = [
        ["settingsOledPageIdentity", 0],
        ["settingsOledPageNetwork",  1],
        ["settingsOledPageLora",     2],
        ["settingsOledPageMessages", 3],
        ["settingsOledPageSystem",   4]
    ];
    var mask = 0;
    pageBits.forEach(function(p) {
        var el = document.getElementById(p[0]);
        if (el && el.checked) mask |= (1 << p[1]);
    });
    if (mask === 0) mask = 0xFF;
    s["oledPageMask"] = mask;
    var oledBtnEl = document.getElementById("settingsOledButtonPin");
    if (oledBtnEl) {
        var pin = parseInt(oledBtnEl.value);
        if (!isNaN(pin)) s["oledButtonPin"] = Math.max(-1, Math.min(48, pin));
    }
    s["udpPeers"] = [];
    document.querySelectorAll('#udpPeerList .udpPeerRow').forEach(function(row) {
        var val = row.querySelector('.udpPeerIP').value || "0.0.0.0";
        s["udpPeers"].push({ "ip": val.split('.').map(Number), "legacy": row.querySelector('.udpPeerLegacyCell input').checked, "enabled": row.querySelector('.udpPeerEnabledCell input').checked });
    });
    sendWS(JSON.stringify({settings: s}));
    captureSettingsSnapshot();
    settingsDirty = false;

    // ── Set password (only if fields are filled) ──────────────────────────
    if (pw1 !== "") {
        console.log("[PW] Sende setPassword...");
        var hash = hashPassword(pw1);
        console.log("[PW] Hash berechnet:", hash.substring(0, 8) + "...");
        sendWS(JSON.stringify({ setPassword: hash }));
    } else {
        console.log("[PW] No password entered, no setPassword sent");
    }

    // Clear fields after saving
    document.getElementById("settingsWebPassword").value = "";
    document.getElementById("settingsWebPasswordConfirm").value = "";

    showToast(t('settings.saved'));
}

function showMessages(parseAll = false) {
    if (parseAll) {
        //Delete everything
        for (let i = 1; i <= 10; i++) { document.getElementById("channel" + i).innerHTML = ""; }
    }
    
    var sound = false;

    var myCall = document.getElementById("settingsMycall").value;

    messages.forEach(function(m) {
        //Abort if message was already displayed
        if ((m.parsed == true) && (parseAll == false)) {return;}
        m.parsed = true;
        var msg = "";

        //Process message
        if ((m.messageType == 0) || (m.messageType == 1)) { //only TEXT & TRACE messages
            var txClass = m.tx ? ' msg-tx' : '';
            const date = new Date(m.timestamp * 1000);
            var timeStr = date.toLocaleString("de-DE", {day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit"}).replace(",", "");
            var prefix = "";
            if (m.dstCall)  { prefix += esc(m.dstCall) + ": "; }
            if (m.dstGroup) { prefix += esc(m.dstGroup) + ": "; }
            if (m.messageType == 1) { prefix += "[TRACE] "; }
            msg += "<div class='msg-bubble" + txClass + "'>";
            msg += "<span class='msg-time'>" + timeStr + "</span>";
            msg += "<span class='msg-call' onclick='insertCallsign(\"" + esc(m.srcCall).replace(/"/g, '') + "\")'>" + esc(m.srcCall) + "</span>";
            msg += "<span class='msg-text'>" + prefix + esc(m.text || '') + "</span>";
            msg += "</div>";
        }
        
        //Split into different channels
        var found = false;
        //Message to all -> Channel 1
        if ((m.dstCall == "") && (m.dstGroup == "") && (found == false)) {
            found = true;
            document.getElementById("channel1").insertAdjacentHTML('beforeend', msg);
            if (!parseAll) {channels[1] = true;}
        }

        //Messages to me -> Channel 2
        if (myCall && (m.dstCall == myCall) && (found == false)) {
            found = true;
            document.getElementById("channel2").insertAdjacentHTML('beforeend', msg);
            if (!parseAll) {channels[2] = true;}
            sound = 1;
        }

        for (let i = 1; i <= 10; i++) {
            //Separator
            if (m.delimiter == true) {
                found = true;
                document.getElementById("channel" + i).insertAdjacentHTML('beforeend', "<span></span>");
            }

            //Message to group -> Channel 3...10
            if ((m.dstGroup == Cookie.get("channel" + i)) && (m.dstCall == "") && (found == false)) {
                found = true;
                document.getElementById("channel" + i).insertAdjacentHTML('beforeend', msg);
                if (!parseAll && !channelMuted[i]) { channels[i] = true; sound = 1; }
            }
        }

        // Collection groups: route messages from configured groups without own tab
        if (found == false && m.dstGroup) {
            for (let s = 3; s <= 10; s++) {
                if (channelSammel[s] && sammelGroups[s] && sammelGroups[s].includes(m.dstGroup)) {
                    found = true;
                    document.getElementById("channel" + s).insertAdjacentHTML('beforeend', msg);
                    break;
                }
            }
        }

        //Messages that I sent -> Channel 2
        if (myCall && (m.srcCall == myCall) && (m.dstGroup == "") && (found == false)) {
            found = true;
            document.getElementById("channel2").insertAdjacentHTML('beforeend', msg);
            if (!parseAll) {channels[2] = true;}
        }

        //Rest -> Channel 1
        if (found == false) {
            found = true;
            document.getElementById("channel1").insertAdjacentHTML('beforeend', msg);
            if (!parseAll) {channels[1] = true;}
        }

    });

    //Adjust UI for auto-scroll and unread messages
    setUI(ui);
     if ((parseAll == false) && (sound == true)) {okSound.play(); console.log("SOUND!!!");}

}



function initWebSocket() {
    //Debug
    if (!window.location.hostname.includes("127.0.0.1")) {
        gateway = `ws://${window.location.hostname}/socket`;
        baseURL = "";
    } else {
        gateway = "ws://192.168.33.60/socket";
        baseURL = "http://192.168.33.60/"
    }

    if (typeof setAntennaColor === 'function') setAntennaColor('#525252');

    //WebSocket init
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    init = false;
    authRequired = false;
    _wsReconnectDelay = 500; // Reset backoff on successful connect
    if (typeof setAntennaColor === 'function') setAntennaColor('#4ecca3');
    keepAlive();
}

var _wsReconnectDelay = 500;

function onClose(event) {
    init = false;
    if (typeof setAntennaColor === 'function') setAntennaColor('#525252');
    clearTimeout(timeout);
    // save cache before reconnect so messages survive reload
    if (_msgCacheDirty) saveCachedMessages();
    setTimeout(initWebSocket, _wsReconnectDelay);
    // Exponential backoff: 500ms → 1s → 2s → 4s → … → 30s max
    _wsReconnectDelay = Math.min(_wsReconnectDelay * 2, 30000);
}

function sendWS(text) {
    try {
        if (websocket.readyState !== WebSocket.OPEN) return;
        websocket.send(text);
        console.log("TX: " + text);
    } catch(e) { console.warn('WS send failed:', e); }
}

function keepAlive() {
    if (websocket.readyState == websocket.OPEN) {
        websocket.send(JSON.stringify({ping: new Date() }));
    }
    timeout = setTimeout(keepAlive, 20000);
}

// save message cache when leaving page
window.addEventListener("beforeunload", function() {
    if (_msgCacheDirty) saveCachedMessages();
});

// ── Pure JS SHA-256 / HMAC-SHA256 (no SubtleCrypto needed, works over HTTP) ──
function _sha256bytes(input) {
    function rr(v,n){return(v>>>n)|(v<<(32-n));}
    const K=new Uint32Array([0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2]);
    let H=new Uint32Array([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]);
    const m=Array.from(input);const bl=m.length*8;m.push(0x80);
    while((m.length%64)!==56)m.push(0);
    m.push(0,0,0,0,(bl>>>24)&0xff,(bl>>>16)&0xff,(bl>>>8)&0xff,bl&0xff);
    const W=new Uint32Array(64);
    for(let i=0;i<m.length;i+=64){
        for(let j=0;j<16;j++)W[j]=(m[i+j*4]<<24)|(m[i+j*4+1]<<16)|(m[i+j*4+2]<<8)|m[i+j*4+3];
        for(let j=16;j<64;j++){const s0=rr(W[j-15],7)^rr(W[j-15],18)^(W[j-15]>>>3);const s1=rr(W[j-2],17)^rr(W[j-2],19)^(W[j-2]>>>10);W[j]=(W[j-16]+s0+W[j-7]+s1)|0;}
        let a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
        for(let j=0;j<64;j++){const t1=(h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+K[j]+W[j])|0;const t2=((rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c)))|0;h=g;g=f;f=e;e=(d+t1)|0;d=c;c=b;b=a;a=(t1+t2)|0;}
        H[0]=(H[0]+a)|0;H[1]=(H[1]+b)|0;H[2]=(H[2]+c)|0;H[3]=(H[3]+d)|0;H[4]=(H[4]+e)|0;H[5]=(H[5]+f)|0;H[6]=(H[6]+g)|0;H[7]=(H[7]+h)|0;
    }
    return H;
}
function _sha256hex(str) {
    const enc = new TextEncoder();
    const H = _sha256bytes(Array.from(enc.encode(str)));
    return Array.from(H).map(v => v.toString(16).padStart(8,'0')).join('');
}
function _hmacSha256hex(keyHex, msg) {
    const enc = new TextEncoder();
    const key = new Uint8Array(64);
    for (let i = 0; i < 32; i++) key[i] = parseInt(keyHex.substr(i*2, 2), 16);
    const ipad = new Uint8Array(64), opad = new Uint8Array(64);
    for (let i = 0; i < 64; i++) { ipad[i] = key[i] ^ 0x36; opad[i] = key[i] ^ 0x5c; }
    const msgB = Array.from(enc.encode(msg));
    const innerH = _sha256bytes(Array.from(ipad).concat(msgB));
    const innerB = [];
    for (let i = 0; i < 8; i++) { innerB.push((innerH[i]>>>24)&0xff,(innerH[i]>>>16)&0xff,(innerH[i]>>>8)&0xff,innerH[i]&0xff); }
    const outerH = _sha256bytes(Array.from(opad).concat(innerB));
    return Array.from(outerH).map(v => v.toString(16).padStart(8,'0')).join('');
}

// ── Show / hide auth overlay ────────────────────────────────────────
function showAuthOverlay(mycall, chipId) {
    document.getElementById("auth-overlay").style.display = "flex";
    document.getElementById("auth-password").value = "";
    document.getElementById("auth-error").style.display = "none";
    var mc = document.getElementById("auth-mycall");
    var ci = document.getElementById("auth-chipid");
    if (mc) mc.textContent = mycall || "";
    if (ci) ci.textContent = chipId ? "Chip ID: " + chipId : "";
    setTimeout(() => document.getElementById("auth-password").focus(), 50);
}

function hideAuthOverlay() {
    document.getElementById("auth-overlay").style.display = "none";
}

function showAuthError(msg) {
    var el = document.getElementById("auth-error");
    el.textContent = msg;
    el.style.display = "block";
    document.getElementById("auth-password").value = "";
    document.getElementById("auth-password").focus();
}

// ── Send HMAC-SHA256 challenge-response ─────────────────────────────────────
function sendAuthResponse(password) {
    const pwHash = _sha256hex(password);
    const response = _hmacSha256hex(pwHash, authNonce);
    sendWS(JSON.stringify({ auth: { response: response } }));
}

// ── Compute SHA-256 of a password (for setPassword) ───────────────────────
function hashPassword(password) {
    if (password === "") return "";
    return _sha256hex(password);
}
