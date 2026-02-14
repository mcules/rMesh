var websocket;
var settings;
var messages = [];
var baseURL = "";
var gateway = "";
var init = false;
let heartBeatTimer;
let okSound = new Audio("ok.wav");

function initWebSocket() {
    //Debug
    if (!window.location.hostname.includes("127.0.0.1")) {
        gateway = `ws://${window.location.hostname}/socket`;
        baseURL = "";
    } else {
        gateway = "ws://192.168.33.60/socket";
        baseURL = "http://192.168.33.60/"
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
    //Alle Container löschen
    if (parseAll == true) {
        buildMenu();
        for (key in guiSettings.groups) { 
            const groupName = guiSettings.groups[key].name; 
            const div = document.getElementById("group_" + groupName);
            div.innerHTML = "";
            setupInputBar('group_' + groupName, mySendMessageFunction); 
            //Alles als gelesen martieren
            guiSettings.groups[key].read = true;  
        }
        document.getElementById("group_all").innerHTML = "";
        setupInputBar('group_all', mySendMessageFunction); 
        for (key in guiSettings.dm) { 
            const callsign = guiSettings.dm[key].name; 
            const div = document.getElementById("dm_" + callsign);
            div.innerHTML = "";
            setupInputBar('dm_' + callsign, mySendMessageFunction); 
            guiSettings.dm[key].read = true;
        }
    }

    var sound = false;

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
        m.parsed = true;

        //Nachrichten zuordnen (Gruppen)
        for (key in guiSettings.groups) { 
            const groupName  = guiSettings.groups[key].name;

            if ((groupName == m.dstGroup) && (m.dstCall == "")) {
                addBubble(
                    css, 
                    titel, 
                    date.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit" }) + " " + date.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" }),
                    getColorForName(m.srcCall), 
                    msg, 
                    "group_" + groupName
                );   
                if (document.getElementById("group_" + groupName).classList.contains("active") != false) {m.read = true;}
                if (m.read == false) { guiSettings.groups[key].read = false; }
                found = true;
                sound = true;
            }

        }

        //Direkte Nachrichten empfangen
        if (m.dstCall == settings.mycall) {
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
                date.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit" }) + " " + date.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" }),
                getColorForName(callsign), 
                msg, 
                "dm_" + callsign
            );  
            if (document.getElementById("dm_" + callsign).classList.contains("active") != false) {m.read = true;} 
            if (m.read == false) { 
                for (var i = 0; i < guiSettings.dm.length; i++) { 
                    if (guiSettings.dm[i].name === callsign) { guiSettings.dm[i].read = false; break;} 
                } 
            }
            found = true;
            sound = true;
        }

        //Direkte Nachrichten gesendet
        if ((m.srcCall == settings.mycall) && (m.dstCall != "")) {
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
                date.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit" }) + " " + date.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" }),
                getColorForName(m.srcCall), 
                msg, 
                "dm_" + callsign
            );   
            if (document.getElementById("dm_" + callsign).classList.contains("active") != false) {m.read = true;} 
            if (m.read == false) { 
                for (var i = 0; i < guiSettings.dm.length; i++) { 
                    if (guiSettings.dm[i].name === callsign) { guiSettings.dm[i].read = false; break;} 
                } 
            }
            found = true;
            sound = true;
        }

        //Keine Gruppe gesetzt + Rest
        if (((m.dstCall == "") && (m.dstGroup == "")) || (found == false)) {
            addBubble(
                css, 
                titel, 
                date.toLocaleDateString("de-DE", { day: "2-digit", month: "2-digit" }) + " " + date.toLocaleTimeString("de-DE", { hour: "2-digit", minute: "2-digit" }),
                getColorForName(m.srcCall), 
                msg, 
                "group_all"
            );   
        }
        
    });

    //Ungelesen anzeigen
    var globalUnRead = false;
    for (key in guiSettings.groups) { 
        if (guiSettings.groups[key].read == false) {globalUnRead = true; document.getElementById("mnu_" + guiSettings.groups[key].name).classList.add('newMessages'); }
    }
    for (key in guiSettings.dm) { 
        if (guiSettings.dm[key].read == false) {globalUnRead = true; document.getElementById("mnu_" + guiSettings.dm[key].name).classList.add('newMessages'); }
    }
    if (globalUnRead == true)  {
        document.getElementById("burger-icon").classList.add('newMessages');
        if ((parseAll == false) && (sound == true)) {okSound.play(); console.log("SOUND!!!");}
    } else {
        document.getElementById("burger-icon").classList.remove('newMessages');
    }

}

function onMessage(event) {
    var d = JSON.parse(event.data);
    //if (d.status === undefined) {console.log("RX: " + event.data);}

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
        document.getElementById("version").innerHTML = d.settings.name + " " + d.settings.version;
        document.getElementById("hardware").innerHTML = d.settings.hardware;
        document.getElementById("settingsLoraRepeat").checked = d.settings.loraRepeat; 
        document.getElementById("settingsLoraMaxMessageLength").innerHTML = d.settings.loraMaxMessageLength + " characters"; 

        //UDP Peers
        if (d.settings.udpPeers) {
            d.settings.udpPeers.forEach(function(p, index) {
                document.getElementById("settingsUDPPeer" + index).value = p.ip[0] + "." + p.ip[1] + "." + p.ip[2] + "." + p.ip[3];                
            });
        }

        if (init == false) {
            settingsVisibility();
            messages = [];
            //messages.json laden (geht erst jetzt, weil sonst mycall nicht bekannt)
            fetch(baseURL + "messages.json")
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
                    });
                    showMessages(true);
                    showContent(guiSettings.menu, guiSettings.title);
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

function saveSettings() {
    var settings = {};
    settings["mycall"] = document.getElementById("settingsMycall").value;
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
    settings["udpPeers"] = [];
    for (var i = 0; i < 5; i++) {
        var val = document.getElementById("settingsUDPPeer" + i).value;
        if (!val) val = "0.0.0.0";
        var ipParts = val.split('.').map(Number);
        settings["udpPeers"].push({
            "ip": ipParts
        });
    }
    sendWS(JSON.stringify({settings: settings}));
    showModal("Note", "Settings saved.", "", false);
}

function reboot() {
    sendWS(JSON.stringify({reboot: true }));
    showModal("Note", "System rebooting...", "", false);
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
}

function sendTuning() {
    sendWS(JSON.stringify({tune: true }));
    okSound.play();
}



