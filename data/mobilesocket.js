var websocket;
var settings;
var messages = [];
var baseURL = "";
var gateway = "";
var init = false;
let heartBeatTimer;
let okSound = new Audio("ok.wav");
var timeout;
var authRequired = false;
var authNonce    = "";


function initWebSocket() {
    //Debug
    if (!window.location.hostname.includes("127.0.0.1")) {
        gateway = `ws://${window.location.hostname}/socket`;
        baseURL = "";
    } else {
        gateway = "ws://192.168.33.66/socket";
        baseURL = "http://192.168.33.66/"
        //gateway = "ws://10.10.253.161/socket";
        //baseURL = "http://10.10.253.161/"
    }

    //Websocket init
    setAntennaColor("#525252");
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    init = false;
    keepAlive();
}

function onClose(event) {
    init = false;
    authRequired = false;
    authNonce = "";
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

function showMessages(parseAll) {
    if (!guiSettings || !messages) return;

    // Alle Container löschen
    if (parseAll === true) {
        buildMenu();
        // Gruppen aufräumen
        for (let key in guiSettings.groups) { 
            const groupName = guiSettings.groups[key].name; 
            const div = document.getElementById("group_" + groupName);
            if (div) { // WICHTIG: Nur wenn das Element existiert
                div.innerHTML = "";
            }
            guiSettings.groups[key].read = true;  
        }
        // "All" Gruppe aufräumen
        const divAll = document.getElementById("group_all");
        if (divAll) {
            divAll.innerHTML = "";
        }
        // DMs aufräumen
        for (let key in guiSettings.dm) { 
            const callsign = guiSettings.dm[key].name; 
            const div = document.getElementById("dm_" + callsign);
            if (div) {
                div.innerHTML = "";
            }
            guiSettings.dm[key].read = true;
        }
        guiSettings.readAll = true;
    }

    var sound = false;
    var globalUnRead = false;

    //Alle Nachrichten durchlaufen
    messages.forEach(function(m) {
        //Abbruch, wenn Nachricht schon angezeigt wurde
        if ((m.parsed == true) && (parseAll == false)) {return;}

        //Nachricht zusammenbauen
        var found = false;
        var css = "left";
        if (m.tx) css = "right";
        var titel = m.srcCall + " > " + m.dstCall + m.dstGroup;
        var msg = m.text;
        var date = new Date(m.timestamp * 1000);
        var dateString = date.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit" }) + " " + date.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" });
        m.parsed = true;
        var hopCount = "";
        if (m.hopCount) {
            hopCount = `${m.hopCount} ${m.hopCount === 1 ? 'Hop ' : 'Hops '}`;
        }


        //Nachrichten zuordnen (Gruppen)
        for (var key in guiSettings.groups) {
            const groupName  = guiSettings.groups[key].name;
            if ((groupName == m.dstGroup) && (m.dstCall == "")) {
                found = true;
                const inSammel  = guiSettings.groups[key].inSammel === true;
                const sammelName = guiSettings.sammelName || "";
                if (inSammel && sammelName) {
                    // Sammelgruppe: Nachricht umleiten, keine Notification
                    addBubble(css, titel, hopCount + " " + dateString, getColorForName(m.srcCall), msg, "group_" + sammelName);
                    m.read = true;
                } else {
                    addBubble(css, titel, hopCount + " " + dateString, getColorForName(m.srcCall), msg, "group_" + groupName);
                    //Wenn Browser Focus hat und Gruppe angezeigt wird -> als gelesen merkieren
                    if ((document.getElementById("group_" + groupName).classList.contains("active")) && focus) {m.read = true;}
                    //Wenn Mute, dann als gelesen markieren
                    if (guiSettings.groups[key].mute == true) {m.read = true;};
                    //Wenn nicht gelesen, dann Gruppe als ungelesen kennzeichen
                    if (m.read == false) { guiSettings.groups[key].read = false; sound = true;}
                }
            }
        }

        //Direkte Nachrichten empfangen
        if (m.dstCall == settings.mycall) {
            found = true;
            var callsign = m.srcCall;
            //Prüfen, ob bereits vorhanden
            var exists = false; 
            for (var i = 0; i < guiSettings.dm.length; i++) { 
                if (guiSettings.dm[i].name === callsign) { exists = true; break; } 
            } 
            if (!exists) { guiSettings.dm.push({ name: callsign, read: true }); buildMenu(); }
            //Anzeigen
            addBubble(
                css, 
                titel, 
                hopCount + " " + dateString,
                getColorForName(callsign), 
                msg, 
                "dm_" + callsign
            );  
            //Wenn Browser Focus hat und Gruppe angezeigt wird -> als gelesen merkieren
            if ((document.getElementById("dm_" + callsign).classList.contains("active")) && focus) {m.read = true;}
            //Wenn nicht gelesen, dann Gruppe als ungelesen kennzeichen
            if (m.read == false) { 
                for (var i = 0; i < guiSettings.dm.length; i++) { 
                    if (guiSettings.dm[i].name === callsign) { guiSettings.dm[i].read = false; break;} 
                } 
            }
            sound = true;
        }

        //Direkte Nachrichten gesendet
        if ((m.srcCall == settings.mycall) && (m.dstCall != "")) {
            found = true;
            var callsign = m.dstCall;
            //Prüfen, ob bereits vorhanden
            var exists = false; 
            for (var i = 0; i < guiSettings.dm.length; i++) { 
                if (guiSettings.dm[i].name === callsign) { exists = true; break; } 
            } 
            if (!exists) { guiSettings.dm.push({ name: callsign, read: true }); buildMenu(); }
            //Anzeigen
            addBubble(
                css, 
                titel, 
                dateString,
                getColorForName(m.srcCall), 
                msg, 
                "dm_" + callsign
            );   
        }

        //Keine Gruppe gesetzt + Rest
        if (((m.dstCall == "") && (m.dstGroup == "")) || (found == false)) {
            addBubble(
                css, 
                titel, 
                hopCount + " " + dateString,
                getColorForName(m.srcCall), 
                msg, 
                "group_all"
            );   
            //Wenn Browser Focus hat und Gruppe angezeigt wird -> als gelesen merkieren
            if ((document.getElementById("group_all").classList.contains("active")) && focus) {m.read = true;}
            //Wenn Mute, dann als gelesen markieren
            if (guiSettings.muteAll == true) {m.read = true;};
            //Wenn nicht gelesen, dann Gruppe als ungelesen kennzeichen
            if (m.read == false) { guiSettings.readAll = false; sound = true;}
        }
        
    });

    //Ungelesen anzeigen
    //All
    if (guiSettings.readAll == false) {globalUnRead = true; document.getElementById("mnu_all").classList.add('newMessages'); }
    //Gruppen
    for (var key in guiSettings.groups) { 
        if (guiSettings.groups[key].read == false) {globalUnRead = true; document.getElementById("mnu_" + guiSettings.groups[key].name).classList.add('newMessages'); }
    }
    //DM
    for (var key in guiSettings.dm) { 
        if (guiSettings.dm[key].read == false) {globalUnRead = true; document.getElementById("mnu_" + guiSettings.dm[key].name).classList.add('newMessages'); }
    }
    //Global
    if (globalUnRead == true)  {
        document.getElementById("burger-icon").classList.add('newMessages');
        if ((parseAll == false) && (sound == true)) {okSound.play(); console.log("SOUND!!!");}
        document.title = settings.name + " ✉️";
    } else {
        document.getElementById("burger-icon").classList.remove('newMessages');
        document.title = settings.name;
    }

}

function onMessage(event) {
    var d = JSON.parse(event.data);
    //if (d.status === undefined) {console.log("RX: " + event.data);}

    // Auth
    if (d.auth) {
        if (d.auth.required) {
            authRequired = true;
            authNonce = d.auth.nonce || "";
            showAuthOverlay(d.auth.mycall, d.auth.chipId);
        } else if (d.auth.ok) {
            authRequired = false;
            hideAuthOverlay();
        } else if (d.auth.error) {
            showAuthError(d.auth.error);
        }
        return;
    }

    // ── Passwort gespeichert ──────────────────────────────────────────────────
    if (d.passwordSaved !== undefined) {
        if (d.passwordSaved) {
            showModal("Note", "Password saved. Reloading...", "", false);
            setTimeout(() => location.reload(), 2000);
        } else {
            showModal("Note", "Password removed.", "", false);
        }
        return;
    }

    if (authRequired) return;

    //RAW-RX
    if (d.monitor) {
        var f = d.monitor;
        var msg = ""; 
        const date = new Date(d.monitor.timestamp * 1000);
        var titel = "";

        //Lesbar anzeigen
        if (typeof f.nodeCall !== "undefined") { titel += " " + f.nodeCall ; }
        if (typeof f.viaCall !== "undefined") { titel += " > " + f.viaCall ; }
        if (typeof f.hopCount !== "undefined") { 
            if (f.hopCount > 0) { 
                titel += " H" + f.hopCount; 
            }
        }
        switch (f.frameType) {
            case 0x00: titel += ": Announce"; break;
            case 0x01: titel += ": Announce ACK"; break;
            case 0x02: titel += ": Tuning"; break;
            case 0x03: 
            case 0x05: 
                if (f.messageType == 0) {titel += ": TEXT Message";}
                if (f.messageType == 1) {titel += ": TRACE Message";}
                if (f.messageType == 15) {titel += ": COMMAND Messahe";}
                if (f.id) { msg += "ID: " + f.id ; }
                if (f.srcCall) { msg += " SRC: " + f.srcCall; }
                if (f.dstCall) { msg += " DST: " + f.dstCall; }
                if (f.dstGroup) { msg += " GRP: " + f.dstGroup ; }
                if (f.text) { msg += "\nText: " + f.text; }
                break;
            case 0x04: 
                titel += ": Message ACK"; 
                if (f.id) { msg += "ID: " + f.id + "\n"; }
                break;
        }
        addBubble("system", titel, date.toLocaleString("de-DE", {hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", ""), "#009eaf", msg, "cMonitor");   

    }

    //Message empfangen
    if (d.message) {
        d.message.parsed = false;
        d.message.read = false;
        messages.push(d.message);
        showMessages(false);
    }

    //Peers
    if (d.peerlist) {
        var peers = "";
        peers += "<table class='list'>";
        peers += "<tr> <th>Port</th> <th>Call</th> <th>Last RX</th> <th>RSSI</th> <th>SNR</th>";
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
                //peers += "<td>" + parseInt(p.frqError) + "</td>";
                peers += "</tr>";
            });
        }
        peers += "</table>";
        document.getElementById("peer").innerHTML = peers;
    }

    //Routing Liste
    if (d.routingList) {
        var routing = "";
        routing += "<table class='list'>";
        routing += "<tr> <th>Call</th> <th>Node</th> <th>Hops</th> <th>Last RX</th> </tr>";
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
        document.getElementById("settingsMycall").value = d.settings.mycall;
        document.getElementById("settingsPosition").value = d.settings.position || "";
        document.getElementById("settingsNTP").value = d.settings.ntp;
        document.getElementById("settingsSSID").value = d.settings.wifiSSID;
        document.getElementById("settingsPassword").value = d.settings.wifiPassword;
        document.getElementById("settingsWiFiIP").value = d.settings.wifiIP[0] + "." + d.settings.wifiIP[1] + "." + d.settings.wifiIP[2] + "." + d.settings.wifiIP[3];
        document.getElementById("settingsWifiNetMask").value = d.settings.wifiNetMask[0] + "." + d.settings.wifiNetMask[1] + "." + d.settings.wifiNetMask[2] + "." + d.settings.wifiNetMask[3];
        document.getElementById("settingsWifiGateway").value = d.settings.wifiGateway[0] + "." + d.settings.wifiGateway[1] + "." + d.settings.wifiGateway[2] + "." + d.settings.wifiGateway[3];
        document.getElementById("settingsWifiDNS").value = d.settings.wifiDNS[0] + "." + d.settings.wifiDNS[1] + "." + d.settings.wifiDNS[2] + "." + d.settings.wifiDNS[3];
        document.getElementById("settingsDHCP").checked = d.settings.dhcpActive; 
        document.getElementById("settingsApMode").checked = d.settings.apMode; 
        document.getElementById("settingsLoraFrequency").value = d.settings.loraFrequency;
        document.getElementById("settingsLoraOutputPower").value = d.settings.loraOutputPower;
        document.getElementById("settingsLoraBandwidth").value = d.settings.loraBandwidth;
        document.getElementById("settingsLoraSyncWord").value = d.settings.loraSyncWord.toString(16).padStart(2, '0').toUpperCase();
        document.getElementById("settingsLoraCodingRate").value = d.settings.loraCodingRate;
        document.getElementById("settingsLoraSpreadingFactor").value = d.settings.loraSpreadingFactor;
        document.getElementById("settingsLoraPreambleLength").value = d.settings.loraPreambleLength;
        // Preset-Dropdown aus aktueller Frequenz ableiten
        const freq = d.settings.loraFrequency;
        const presetEl = document.getElementById("loraPreset");
        if (presetEl) {
            if (freq >= 430 && freq <= 440)       presetEl.value = "433";
            else if (freq >= 869.4 && freq <= 869.65) presetEl.value = "868";
            else                                   presetEl.value = "";
        }
        document.getElementById("version").innerHTML = d.settings.name + " " + d.settings.version;
        document.getElementById("hardware").innerHTML = d.settings.hardware;
        var chipIdEl = document.getElementById("setupChipId");
        if (chipIdEl) chipIdEl.innerHTML = d.settings.chipId || "";
        document.getElementById("settingsLoraRepeat").checked = d.settings.loraRepeat;
        document.getElementById("settingsLoraEnabled").checked = d.settings.loraEnabled !== false;
        document.getElementById("settingsUpdateChannel").value = d.settings.updateChannel || 0;
        document.getElementById("settingsLoraMaxMessageLength").innerHTML = d.settings.loraMaxMessageLength + " characters";
        const pwStatus = document.getElementById("settingsWebPasswordStatus");
        if (pwStatus) {
            pwStatus.textContent = d.settings.webPasswordSet ? "set ✓" : "not set";
            pwStatus.style.color = d.settings.webPasswordSet ? "#00d1b2" : "#888";
        }
        const removeRow = document.getElementById("settingsWebPasswordRemoveRow");
        if (removeRow) removeRow.style.display = d.settings.webPasswordSet ? "" : "none";

        //UDP Peers
        if (d.settings.udpPeers) {
            renderUdpPeers(d.settings.udpPeers);
        }

        if (init == false) {
            settingsVisibility(); 
            messages = [];
            showMessages(true);
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
                        if (m.message.timestamp > guiSettings.update) {m.message.read = false;} else {m.message.read = true;}
                        m.message.update = guiSettings.update;
                        messages.push(m.message);
                        showMessages(false);
                    });

                    //Letzten Ansicht laden
                    if (document.getElementById(guiSettings.content.content)) {
                        showContent(guiSettings.content.content, guiSettings.content.title, guiSettings.content.dst, guiSettings.content.group);
                    } else {
                        showContent("group_all", "all", "", true);
                    }


                    init = true;
                });
        }
    }

    //Status
    if (d.status) {
        if (d.status.tx) {
            setAntennaColor("#FF0000");
        } else if (d.status.rx) {
            setAntennaColor("#00FF00");
        } else {
            setAntennaColor("#afaf00");
        }
        document.getElementById("txBuffer").innerHTML = d.status.txBufferCount; 
        document.getElementById("retry").innerHTML = d.status.retry; 
        document.getElementById("heap").innerHTML = d.status.heap; 
        const time = new Date(d.status.time * 1000);
        document.getElementById("time").innerHTML = time.toLocaleString("de-DE", {day: "2-digit",  month: "2-digit", year: "numeric", hour: "2-digit", minute: "2-digit", second: "2-digit" }).replace(",", "");

        //Antennensymbol anpassen
        clearTimeout(heartBeatTimer);
        heartBeatTimer = setTimeout(function() { setAntennaColor("#525252"); }, 2000);

        //Letzte online Zeit speichern
        if (init == true) {
            guiSettings.update = d.status.time;
            saveGuiSettings();
        }
    }

    //Update-Status
    if (d.updateStatus !== undefined) {
        showModal("Update", d.updateStatus, "", false);
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

// ── LoRa-Frequenz-Presets ─────────────────────────────────────────────────────
const LORA_PRESETS = {
    '433': {
        frequency:       434.850,  // rMesh-Standardfrequenz im 70-cm-AFU-Band
        bandwidth:       62.5,     // kHz – bewährter rMesh-Standard
        spreadingFactor: 7,
        codingRate:      6,        // CR 4/6
        outputPower:     20,       // dBm – typisch für Amateurfunk
        preambleLength:  10,
        syncWord:        '2B',     // AMATEUR_SYNCWORD (Info, wird von Firmware gesetzt)
    },
    '868': {
        frequency:       869.525,  // MHz – Mitte von Sub-Band P (869,4–869,65 MHz)
        bandwidth:       125,      // kHz – passt in 250-kHz-Sub-Band, kürzere ToA
        spreadingFactor: 7,
        codingRate:      5,        // CR 4/5 – effizienter bei 10%-Duty-Cycle
        outputPower:     27,       // dBm – Default 868 MHz Public (500 mW, max. erlaubt)
        preambleLength:  10,
        syncWord:        '12',     // PUBLIC_SYNCWORD (Info, wird von Firmware gesetzt)
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

function renderUdpPeers(peers) {
    var list = document.getElementById('udpPeerList');
    if (!list) return;
    list.innerHTML = '<table class="udpPeerTable">'
        + '<thead><tr><th>Call</th><th>IP</th><th>legacy</th><th>aktiv</th><th></th></tr></thead>'
        + '<tbody id="udpPeerBody"></tbody></table>';
    peers.forEach(function(p) {
        var tbody = document.getElementById('udpPeerBody');
        var tr = document.createElement('tr');
        tr.className = 'udpPeerRow';
        tr.innerHTML = '<td><span class="udpPeerCall">' + (p.call || '–') + '</span></td>'
            + '<td><input class="udpPeerIP" value="' + p.ip.join('.') + '"></td>'
            + '<td><input type="checkbox" class="udpPeerLegacy"' + (p.legacy ? ' checked' : '') + '></td>'
            + '<td><input type="checkbox" class="udpPeerEnabled"' + (p.enabled !== false ? ' checked' : '') + '></td>'
            + '<td><button onclick="this.closest(\'tr\').remove()">✕</button></td>';
        tbody.appendChild(tr);
    });
}

function addUdpPeer() {
    var tbody = document.getElementById('udpPeerBody');
    if (!tbody) { renderUdpPeers([]); tbody = document.getElementById('udpPeerBody'); }
    var tr = document.createElement('tr');
    tr.className = 'udpPeerRow';
    tr.innerHTML = '<td><input class="udpPeerIP" value=""></td>'
        + '<td><input type="checkbox" class="udpPeerLegacy"></td>'
        + '<td><input type="checkbox" class="udpPeerEnabled" checked></td>'
        + '<td><button onclick="this.closest(\'tr\').remove()">✕</button></td>';
    tbody.appendChild(tr);
}

function saveSettings() {
    // Web password handling
    const pw1 = document.getElementById("settingsWebPassword").value;
    const pw2 = document.getElementById("settingsWebPasswordConfirm").value;
    if (pw1 || pw2) {
        if (pw1 !== pw2) {
            showModal("Error", "Passwords do not match.", "", false);
            return;
        }
        const hash = hashPassword(pw1);
        sendWS(JSON.stringify({ setPassword: hash }));
        document.getElementById("settingsWebPassword").value = "";
        document.getElementById("settingsWebPasswordConfirm").value = "";
    }

    var settings = {};
    settings["mycall"] = document.getElementById("settingsMycall").value;
    settings["position"] = document.getElementById("settingsPosition").value;
    settings["ntp"] = document.getElementById("settingsNTP").value;
    settings["dhcpActive"] = document.getElementById("settingsDHCP").checked;
    settings["wifiSSID"] = document.getElementById("settingsSSID").value;
    settings["wifiPassword"] = document.getElementById("settingsPassword").value;
    settings["apMode"] = document.getElementById("settingsApMode").checked;
    settings["wifiIP"] = document.getElementById("settingsWiFiIP").value.split('.').map(Number);
    settings["wifiNetMask"] = document.getElementById("settingsWifiNetMask").value.split('.').map(Number);
    settings["wifiGateway"] = document.getElementById("settingsWifiGateway").value.split('.').map(Number);
    settings["wifiDNS"] = document.getElementById("settingsWifiDNS").value.split('.').map(Number);
    settings["loraFrequency"] = parseFloat(document.getElementById("settingsLoraFrequency").value);
    settings["loraOutputPower"] = parseInt(document.getElementById("settingsLoraOutputPower").value);
    settings["loraBandwidth"] = parseFloat(document.getElementById("settingsLoraBandwidth").value);
    settings["loraSyncWord"] = parseInt(document.getElementById("settingsLoraSyncWord").value, 16);
    settings["loraCodingRate"] = parseInt(document.getElementById("settingsLoraCodingRate").value);
    settings["loraSpreadingFactor"] = parseInt(document.getElementById("settingsLoraSpreadingFactor").value);
    settings["loraPreambleLength"] = parseInt(document.getElementById("settingsLoraPreambleLength").value);
    settings["loraRepeat"] = document.getElementById("settingsLoraRepeat").checked;
    settings["loraEnabled"] = document.getElementById("settingsLoraEnabled").checked;
    settings["updateChannel"] = parseInt(document.getElementById("settingsUpdateChannel").value);
    settings["udpPeers"] = [];
    document.querySelectorAll('#udpPeerList .udpPeerRow').forEach(function(row) {
        var val = row.querySelector('.udpPeerIP').value || "0.0.0.0";
        settings["udpPeers"].push({ "ip": val.split('.').map(Number), "legacy": row.querySelector('.udpPeerLegacy').checked, "enabled": row.querySelector('.udpPeerEnabled').checked });
    });
    sendWS(JSON.stringify({settings: settings}));
    showModal("Note", "Settings saved.", "", false); 
}

function reboot() {
    showModal("Note", "Neustart wird durchgeführt...", "", false);
    sendWS(JSON.stringify({reboot: true }));
}

function shutdown() {
    if (confirm("Gerät wirklich herunterfahren?\n\nNur durch Hardware-Reset (Reset-Button oder Stromtrennung) wieder startbar.")) {
        showModal("Note", "Gerät wird heruntergefahren...", "", false);
        sendWS(JSON.stringify({shutdown: true}));
    }
}

function triggerUpdate() {
    sendWS(JSON.stringify({update: true }));
}

function forceInstall() {
    var ch = parseInt(document.getElementById("settingsUpdateChannel").value);
    sendWS(JSON.stringify({ forceUpdate: ch }));
    showModal("Note", "Update wird gesucht und installiert...", "", false);
}

function uploadFile(type, file, noreboot) {
    return new Promise(function(resolve, reject) {
        var formData = new FormData();
        formData.append('firmware', file, file.name);
        var url = '/ota?type=' + type + (noreboot ? '&noreboot=1' : '');
        var xhr = new XMLHttpRequest();
        xhr.open('POST', url, true);
        xhr.onload = function() {
            if (xhr.status === 200 && xhr.responseText === 'OK') { resolve(); }
            else { reject(xhr.responseText); }
        };
        xhr.onerror = function() { reject('Verbindungsfehler'); };
        xhr.send(formData);
    });
}

async function uploadAll() {
    var fwFile = document.getElementById('otaFwFile').files[0];
    var fsFile = document.getElementById('otaFsFile').files[0];
    if (!fwFile && !fsFile) { showModal("Note", "Firmware und LittleFS Datei fehlen.", "", false); return; }
    if (!fwFile) { showModal("Note", "Firmware Datei fehlt.", "", false); return; }
    if (!fsFile) { showModal("Note", "LittleFS Datei fehlt.", "", false); return; }
    try {
        if (fwFile) {
            showModal("Note", "Firmware wird hochgeladen (" + Math.round(fwFile.size / 1024) + " KB)...", "", false);
            await uploadFile('firmware', fwFile, !!fsFile);
        }
        if (fsFile) {
            showModal("Note", "LittleFS wird hochgeladen (" + Math.round(fsFile.size / 1024) + " KB)...", "", false);
            await uploadFile('spiffs', fsFile, false);
        }
        showModal("Note", "Upload abgeschlossen! Node startet neu...", "", false);
    } catch(e) {
        showModal("Error", "Upload fehlgeschlagen: " + e, "", false);
    }
}

function syncTime() {
    sendWS(JSON.stringify({time: Math.floor(Date.now() / 1000) }));
    showModal("Note", "System time updated from browser.", "", false);
}

function deleteMessages() {
    sendWS(JSON.stringify({deleteMessages: true }));
    showModal("Note", "Clearing buffer and rebooting...", "", false);
}

function sendAnnounce() {
    sendWS(JSON.stringify({announce: true }));
    okSound.play();
    showModal("Note", "Announcement gesendet.", "", false);
}

function sendTuning() {
    sendWS(JSON.stringify({tune: true }));
    okSound.play();
    showModal("Note", "Tune gesendet.", "", false);
}

function hashPassword(password) {
    if (password === "") return "";
    return _sha256hex(password);
}

function sendAuthResponse(password) {
    const pwHash = _sha256hex(password);
    const response = _hmacSha256hex(pwHash, authNonce);
    sendWS(JSON.stringify({ auth: { response: response } }));
}

function doLogin() {
    const pw = document.getElementById("auth-password").value;
    if (!pw) { showAuthError("Please enter your password."); return; }
    sendAuthResponse(pw);
}

function checkPwMatch() {
    var pw1 = document.getElementById("settingsWebPassword").value;
    var pw2 = document.getElementById("settingsWebPasswordConfirm").value;
    var row = document.getElementById("settingsWebPasswordMatchRow");
    var span = document.getElementById("settingsWebPasswordMatch");
    if (!pw2) { row.style.display = "none"; return; }
    row.style.display = "";
    if (pw1 === pw2) {
        span.textContent = "✓ match";
        span.style.color = "#00d1b2";
    } else {
        span.textContent = "✗ no match";
        span.style.color = "#ff4d4d";
    }
}

function removeWebPassword() {
    sendWS(JSON.stringify({ setPassword: "" }));
    showModal("Note", "Password removed.", "", false);
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

function showAuthOverlay(mycall, chipId) {
    document.getElementById("auth-overlay").style.display = "flex";
    document.getElementById("auth-error").style.display = "none";
    document.getElementById("auth-password").value = "";
    var mc = document.getElementById("auth-mycall");
    var ci = document.getElementById("auth-chipid");
    if (mc) mc.textContent = mycall || "";
    if (ci) ci.textContent = chipId ? "Chip ID: " + chipId : "";
    setTimeout(() => document.getElementById("auth-password").focus(), 100);
}

function hideAuthOverlay() {
    document.getElementById("auth-overlay").style.display = "none";
}

function showAuthError(msg) {
    const el = document.getElementById("auth-error");
    el.textContent = msg || "Wrong password";
    el.style.display = "block";
}


