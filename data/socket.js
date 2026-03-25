var websocket;
var settings;
var messages = [];
var baseURL = "";
var gateway = "";
var init = false;

// Mute und Sammelgruppe pro Channel (gespeichert als Cookies)
// channelMuted[i] = true → kein Sound/Badge für Channel i
// channelSammel   = Channel-Index 3-10 der als Sammelgruppe dient (0 = keine)
// sammelGroups    = Array von Gruppen-Namen-Strings, die in die Sammelgruppe geleitet werden
var channelMuted  = new Array(11).fill(false);
var channelSammel = 0;
var sammelGroups  = [];

function loadChannelFlags() {
    channelSammel = parseInt(Cookie.get("chSamCol") || "0");
    for (let i = 1; i <= 10; i++) {
        channelMuted[i] = Cookie.get("chMute" + i) === "1";
    }
    try { sammelGroups = JSON.parse(Cookie.get("chSamGrps") || "[]"); } catch(e) { sammelGroups = []; }
}
function saveChannelFlags() {
    Cookie.set("chSamCol", String(channelSammel));
    for (let i = 1; i <= 10; i++) {
        Cookie.set("chMute" + i, channelMuted[i] ? "1" : "0");
    }
    Cookie.set("chSamGrps", JSON.stringify(sammelGroups));
}

// ── Auth-State ────────────────────────────────────────────────────────────────
var authRequired = false;
var authNonce    = "";



function onMessage(event) {
    var d = JSON.parse(event.data);
    if (d.status === undefined) {console.log("RX: " + event.data);}

    // ── Auth-Flow ─────────────────────────────────────────────────────────────
    if (d.auth) {
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
            authNonce = d.auth.nonce;  // neue Nonce für nächsten Versuch
            showAuthError(d.auth.error);
        }
        return;
    }

    // ── Passwort gespeichert ──────────────────────────────────────────────────
    if (d.passwordSaved !== undefined) {
        if (d.passwordSaved) {
            msgBox("Password saved. Reloading page...");
            setTimeout(() => location.reload(), 2000);
        } else {
            msgBox("Password removed.");
        }
        return;
    }

    // Nachrichten ignorieren solange Auth aussteht
    if (authRequired) return;

    //RAW-RX
    if (d.monitor) {
        var f = d.monitor;
        var msg = ""; 
        //TX-Frame gelb
        if (d.monitor.tx == true) { msg += "<span class='monitor-tx'>→ "; } else { msg += "<span>← "; }
        //Port
        if (f.port == 0) {msg += "LoRa";} else {msg += "Wifi";}
        //Zeit
        const date = new Date(d.monitor.timestamp * 1000);
        msg += " " + date.toLocaleString("de-DE", {hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", "");		
        //Lesbar anzeigen
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
                if (f.id) { msg += " ID: " + f.id ; }
                if (f.srcCall) { msg += " SRC: " + f.srcCall; }
                if (f.dstCall) { msg += " DST: " + f.dstCall; }
                if (f.dstGroup) { msg += " GRP: " + f.dstGroup; }
                if (f.messageType == 0) {msg += " TEXT: ";}
                if (f.messageType == 1) {msg += " TRACE: ";}
                if (f.messageType == 15) {msg += " COMMAND: ";}
                if (f.text) { msg += f.text; }
                break;
            case 0x04: 
                msg += " Message ACK "; 
                if (f.id > 0) { msg += " ID: " + f.id + " "; }
                break;
        }
        msg += "</span>";
        document.getElementById("monitor").innerHTML += msg;
        document.getElementById('monitor').scrollTop = document.getElementById("monitor").scrollHeight;
    }

    //Message empfangen
    if (d.message) {
        d.message.parsed = false;
        messages.push(d.message);
        showMessages();
    }

    //Peers
    if (d.peerlist) {
        var peers = "";
        peers += "<table>";
        peers += "<tr> <td>" + t('peer.port') + "</td> <td>" + t('peer.call') + "</td> <td>" + t('peer.last_rx') + "</td> <td>" + t('peer.rssi') + "</td> <td>" + t('peer.snr') + "</td> <td>" + t('peer.frq_err') + "</td> </tr>";
        if (d.peerlist.peers) {
            d.peerlist.peers.forEach(function(p, index) {
				if (p.port == 0) {port = "LoRa";} else {port = "Wifi";}
                const lastRX = new Date(p.timestamp * 1000);
                peers += "<tr>";
                peers += "<td>" + port + "</td>";
                peers += "<td";
                if (p.available == true) { peers += " class='green' "} else { peers += " class='red' "}
                peers += ">" + p.call + "</td>";
                peers += "<td>" + lastRX.toLocaleString("de-DE", {day: "2-digit",  month: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", "")  + "</td>";
                peers += "<td>" + p.rssi + "</td>";
                peers += "<td>" + p.snr + "</td>";
                peers += "<td>" + parseInt(p.frqError) + "</td>";
                peers += "</tr>";
            });
        }
        peers += "</table>";
        document.getElementById("peer").innerHTML = peers;
    }

    //Routing Liste
    if (d.routingList) {
        var routing = "";
        routing += "<table>";
        routing += "<tr> <td>" + t('route.call') + "</td> <td>" + t('route.node') + "</td> <td>" + t('route.hops') + "</td> <td>" + t('route.last_rx') + "</td> </tr>";
        if (d.routingList.routes) {
            d.routingList.routes.forEach(function(r, index) {
                const lastRX = new Date(r.timestamp * 1000);
                routing += "<tr>";
                routing += "<td>" + r.srcCall + "</td>";
                routing += "<td>" + r.viaCall + "</td>";
                routing += "<td>" + r.hopCount + "</td>";
                routing += "<td>" + lastRX.toLocaleString("de-DE", {day: "2-digit",  month: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", "")  + "</td>";
                routing += "</tr>";
            });
        }
        routing += "</table>";
        document.getElementById("routing").innerHTML = routing;
    }    

    //Einstellungen
    if (d.settings) {
        settings = d.settings;
        function fmtIP(a) { return (a && a[0] !== undefined) ? a[0]+'.'+a[1]+'.'+a[2]+'.'+a[3] : '-'; }
        fillSettingsForm(settings);
        document.getElementById("version").innerHTML = d.settings.name + " " + d.settings.version;
        document.getElementById("myCall").innerHTML = d.settings.mycall;
        document.getElementById("settingsLoraMaxMessageLength").innerHTML = d.settings.loraMaxMessageLength + " characters";
        settings.titel = settings.name + " - " + settings.mycall;
        settings.altTitel = "🚨 " + settings.name + " - " + settings.mycall + " 🚨"
        document.getElementById("currentWiFiIP").textContent      = fmtIP(d.settings.currentIP);
        document.getElementById("currentWifiNetMask").textContent  = fmtIP(d.settings.currentNetMask);
        document.getElementById("currentWifiGateway").textContent  = fmtIP(d.settings.currentGateway);
        document.getElementById("currentWifiDNS").textContent      = fmtIP(d.settings.currentDNS);
        //UDP Peers
        if (d.settings.udpPeers) {
            renderUdpPeers(d.settings.udpPeers);
        }

        captureSettingsSnapshot();

        // Chip ID + Hardware in About panel
        var chipIdEl = document.getElementById("aboutChipId");
        if (chipIdEl) chipIdEl.innerHTML = d.settings.chipId || "";
        var hwEl = document.getElementById("aboutHardware");
        if (hwEl) hwEl.innerHTML = d.settings.hardware || "";
        var aboutVersionEl = document.getElementById("aboutVersion");
        if (aboutVersionEl) aboutVersionEl.innerHTML = (d.settings.name || "") + " " + (d.settings.version || "");
        var aboutChangelogEl = document.getElementById("aboutChangelog");
        if (aboutChangelogEl) aboutChangelogEl.textContent = d.settings.changelog || "";

        // Battery status row visibility
        var hasBat = d.settings.hasBattery === true;
        var batEnabled = d.settings.batteryEnabled !== false;
        var batGpRow = document.getElementById("batteryGpRow");
        if (batGpRow) batGpRow.style.display = (hasBat && batEnabled) ? "" : "none";
        var batSettingsSection = document.getElementById("batterySettingsSection");
        if (batSettingsSection) batSettingsSection.style.display = hasBat ? "" : "none";

        // Password status display
        var pwStatus = document.getElementById("settingsWebPasswordStatus");
        var pwRemoveRow = document.getElementById("settingsWebPasswordRemoveRow");
        if (pwStatus) {
            if (d.settings.webPasswordSet) {
                pwStatus.textContent = typeof t === 'function' ? t('pw.set') : 'Password is set';
                pwStatus.style.color = "#4ecca3";
                if (pwRemoveRow) pwRemoveRow.style.display = "";
            } else {
                pwStatus.textContent = typeof t === 'function' ? t('pw.not_set') : 'No password set';
                pwStatus.style.color = "";
                if (pwRemoveRow) pwRemoveRow.style.display = "none";
            }
        }

        if (init == false) {
            init = true;
            //for (let i = 0; i <= 10; i++) {channels[i] = false;}
            //setUI(ui);
            messages = [];
            //messages.json laden (geht erst jetzt, weil sonst mycall nicht bekannt)
            fetch(baseURL + "messages.json?" + Math.random())
                .then(function(response) {
                    return response.text();
                })
                .then(function(text) {
                    var lines = text.split(/\r?\n/);
                    lines.forEach(function(line) {
                        if (line.trim().length === 0) return;
                        var m = JSON.parse(line);
                        m.message.parsed = false;
                        messages.push(m.message);
                    });

                    //"Trennzeichen" zwischen gespeicherten und neuen Nachrichten
                    const result = {"delimiter": true};
                    messages.push(result);
                    showMessages(true);

                    //Alles als gelesen markieren
                    for (let i = 0; i <= 10; i++) {channels[i] = false;} 
                    setUI(ui);
                });
        }
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
        document.getElementById("heap").innerHTML = d.status.heap;
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

    //Update verfügbar
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

//Nachricht senden
async function sendMessage(text, channel) {
    //Zielrufzeichen zusammenbasteln
    var dstCall = document.getElementById('dstCall').innerHTML;
    if (channel == 2) {
        dstCall = (await inputBox("Destination Call?", Cookie.get("channel2"), document.getElementById('messageText' + channel) )).toUpperCase();
        Cookie.set("channel2", dstCall);
    }
    if (dstCall == "") {return;}
    if (dstCall == "all") dstCall = "";
    //Nachricht vorbereiten und über Websocket senden
    var message = {};
    message["text"] = text;
    message["dst"] = dstCall;
    if (channel == 2) {
        //Private Nachricht
        sendWS(JSON.stringify({sendMessage: message}));                    
    } else {
        //Gruppe
        sendWS(JSON.stringify({sendGroup: message}));                    
    }
}

// ── LoRa-Frequenz-Presets ─────────────────────────────────────────────────────
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
        // Eingetippte Frequenz beibehalten, nur Restparameter aus Preset laden
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
            + '<td class="udpPeerLegacyCell">' + legacySwitch + '</td>'
            + '<td class="udpPeerEnabledCell">' + enabledSwitch + '</td>'
            + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()">' + t('btn.remove_peer') + '</button></td>';
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
        + '<td class="udpPeerLegacyCell">' + udpPeerToggle(false) + '</td>'
        + '<td class="udpPeerEnabledCell">' + udpPeerToggle(true) + '</td>'
        + '<td><button class="button button-danger" onclick="this.closest(\'tr\').remove()">' + t('btn.remove_peer') + '</button></td>';
    tbody.appendChild(tr);
}

function fillSettingsForm(s) {
    document.getElementById("settingsMycall").value = s.mycall;
    document.getElementById("settingsPosition").value = s.position || "";
    document.getElementById("settingsNTP").value = s.ntp;
    document.getElementById("settingsSSID").value = s.wifiSSID;
    document.getElementById("settingsPassword").value = s.wifiPassword;
    document.getElementById("settingsWiFiIP").value = s.wifiIP[0] + "." + s.wifiIP[1] + "." + s.wifiIP[2] + "." + s.wifiIP[3];
    document.getElementById("settingsWifiNetMask").value = s.wifiNetMask[0] + "." + s.wifiNetMask[1] + "." + s.wifiNetMask[2] + "." + s.wifiNetMask[3];
    document.getElementById("settingsWifiGateway").value = s.wifiGateway[0] + "." + s.wifiGateway[1] + "." + s.wifiGateway[2] + "." + s.wifiGateway[3];
    document.getElementById("settingsWifiDNS").value = s.wifiDNS[0] + "." + s.wifiDNS[1] + "." + s.wifiDNS[2] + "." + s.wifiDNS[3];
    document.getElementById("settingsDHCP").checked = s.dhcpActive;
    document.getElementById("settingsApMode").checked = s.apMode;
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
    document.getElementById("settingsUpdateChannel").value = s.updateChannel || 0;
    const batEnabledEl = document.getElementById("settingsBatteryEnabled");
    if (batEnabledEl) batEnabledEl.checked = s.batteryEnabled !== false;
    const batVoltEl = document.getElementById("settingsBatteryFullVoltage");
    if (batVoltEl) batVoltEl.value = s.batteryFullVoltage || 4.2;
    settingsVisibility();
}

function saveSettings() {
    // ── Passwort-Felder prüfen ────────────────────────────────────────────────
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
    s["wifiSSID"] = document.getElementById("settingsSSID").value;
    s["wifiPassword"] = document.getElementById("settingsPassword").value;
    s["apMode"] = document.getElementById("settingsApMode").checked;
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
    s["updateChannel"] = parseInt(document.getElementById("settingsUpdateChannel").value);
    var batEnabledEl = document.getElementById("settingsBatteryEnabled");
    if (batEnabledEl) s["batteryEnabled"] = batEnabledEl.checked;
    var batVoltEl = document.getElementById("settingsBatteryFullVoltage");
    if (batVoltEl) s["batteryFullVoltage"] = parseFloat(batVoltEl.value);
    s["udpPeers"] = [];
    document.querySelectorAll('#udpPeerList .udpPeerRow').forEach(function(row) {
        var val = row.querySelector('.udpPeerIP').value || "0.0.0.0";
        s["udpPeers"].push({ "ip": val.split('.').map(Number), "legacy": row.querySelector('.udpPeerLegacyCell input').checked, "enabled": row.querySelector('.udpPeerEnabledCell input').checked });
    });
    sendWS(JSON.stringify({settings: s}));
    captureSettingsSnapshot();
    settingsDirty = false;

    // ── Passwort setzen (nur wenn Felder ausgefüllt) ──────────────────────────
    if (pw1 !== "") {
        console.log("[PW] Sende setPassword...");
        var hash = hashPassword(pw1);
        console.log("[PW] Hash berechnet:", hash.substring(0, 8) + "...");
        sendWS(JSON.stringify({ setPassword: hash }));
    } else {
        console.log("[PW] Kein Passwort eingegeben, kein setPassword gesendet");
    }

    // Felder nach dem Speichern leeren
    document.getElementById("settingsWebPassword").value = "";
    document.getElementById("settingsWebPasswordConfirm").value = "";
}

function showMessages(parseAll = false) {
    if (parseAll) {
        //Alles löschen
        for (let i = 1; i <= 10; i++) { document.getElementById("channel" + i).innerHTML = ""; }
    }
    
    var sound = false;

    messages.forEach(function(m) {
        //Abbruch, wenn Nachricht schon angezeigt wurde
        if ((m.parsed == true) && (parseAll == false)) {return;}
        m.parsed = true;
        var msg = "";

        //Nachricht aufbereiten
        if ((m.messageType == 0) || (m.messageType == 1)) { //nur TEXT & TRACE Nachrichten
            //if (m.dstCall.length == 0) {m.dstCall = "all";}
            msg += "<span";
            if (m.tx == true) {
                msg += " class='middle-tx'> ";
            } else {
                msg += ">";
            }
            const date = new Date(m.timestamp * 1000);
            msg += date.toLocaleString("de-DE", {day: "2-digit",  month: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", "") + " ";		
            msg += m.srcCall;
            if (m.dstCall)  {msg += " " + m.dstCall; }
            if (m.dstGroup)  {msg += " " + m.dstGroup; }
            if (m.messageType == 1) {msg += " [TRACE] ";}
            if (m.text) {msg += ": " + m.text;}
            msg += "</span>"
        }
        
        //Auf verschiedene Kanäle aufteilen
        var found = false;
        //Nachricht an alle -> Channel 1
        if ((m.dstCall == "") && (m.dstGroup == "") && (found == false)) {
            found = true;
            document.getElementById("channel1").innerHTML += msg;
            if (!parseAll) {channels[1] = true;}
        }

        //Nachrichten an mich -> Channel 2
        if ((m.dstCall == document.getElementById("settingsMycall").value) && (found == false)) {
            found = true;
            document.getElementById("channel2").innerHTML += msg;
            if (!parseAll) {channels[2] = true;}
            sound = 1;
        }

        for (let i = 1; i <= 10; i++) {
            //Trennung
            if (m.delimiter == true) {
                found = true;
                document.getElementById("channel" + i).innerHTML += "<span>^</span>";
            }

            //Nachricht an Gruppe -> Channel 3...10
            if ((m.dstGroup == Cookie.get("channel" + i)) && (m.dstCall == "") && (found == false)) {
                found = true;
                document.getElementById("channel" + i).innerHTML += msg;
                if (!parseAll && !channelMuted[i]) { channels[i] = true; sound = 1; }
            }
        }

        //Sammelgruppe: Nachrichten von definierten Gruppen ohne eigenen Tab
        if (found == false && m.dstGroup && channelSammel > 0 && sammelGroups.includes(m.dstGroup)) {
            found = true;
            document.getElementById("channel" + channelSammel).innerHTML += msg;
            // keine Notification
        }

        //Nachrichten, die ich gesendet habe -> Channel 2
        if ((m.srcCall == document.getElementById("settingsMycall").value) && (m.dstGroup == "") && (found == false)) {
            found = true;
            document.getElementById("channel2").innerHTML += msg;
            if (!parseAll) {channels[2] = true;}
        }

        //Rest -> Channel 1
        if (found == false) {
            found = true;
            document.getElementById("channel1").innerHTML += msg;
            if (!parseAll) {channels[1] = true;}
        }

    });

    //UI Anpassen wegen nach unten scrollen und ungelesenen Nachrichten
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

    //Websocket init
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    init = false;
    authRequired = false;
    if (typeof setAntennaColor === 'function') setAntennaColor('#4ecca3');
    keepAlive();
}

function onClose(event) {
    init = false;
    if (typeof setAntennaColor === 'function') setAntennaColor('#525252');
    clearTimeout(timeout);
    setTimeout(initWebSocket, 500);
}

function sendWS(text) {
    websocket.send(text);
    console.log("TX: " + text);
}

function keepAlive() {
    if (websocket.readyState == websocket.OPEN) {
        websocket.send(JSON.stringify({ping: new Date() }));
    }
    timeout = setTimeout(keepAlive, 1000);
}

// ── Pure JS SHA-256 / HMAC-SHA256 (kein SubtleCrypto nötig, funktioniert über HTTP) ──
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

// ── Auth-Overlay anzeigen / verstecken ────────────────────────────────────────
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

// ── HMAC-SHA256 Challenge-Response senden ─────────────────────────────────────
function sendAuthResponse(password) {
    const pwHash = _sha256hex(password);
    const response = _hmacSha256hex(pwHash, authNonce);
    sendWS(JSON.stringify({ auth: { response: response } }));
}

// ── SHA-256 eines Passworts berechnen (für setPassword) ───────────────────────
function hashPassword(password) {
    if (password === "") return "";
    return _sha256hex(password);
}
