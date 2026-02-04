var websocket;
var settings;
var messages = [];
var baseURL = "";
var gateway = "";
var init = false;



function onMessage(event) {
    var d = JSON.parse(event.data);
    if (d.status === undefined) {console.log("RX: " + event.data);}

    //RAW-RX
    if (d.monitor) {
        var f = d.monitor;
        var msg = ""; 
        //TX-Frame gelb
        if (d.monitor.tx == true) { msg += "<span class='monitor-tx' >"; } else { msg += "<span>"; }
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
        if (d.message.tx != true) okSound.play();
    }

    //Peers
    if (d.peerlist) {
        var peers = "";
        peers += "<table>";
        peers += "<tr> <td>Port</td> <td>Call</td> <td>Last RX</td> <td>RSSI</td> <td>SNR</td> <td>Frq. Error</td> </tr>";
        if (d.peerlist.peers) {
            d.peerlist.peers.forEach(function(p, index) {
				if (p.port == 0) {port = "LoRa";} else {port = "Wifi";}
                const lastRX = new Date(p.timestamp * 1000);
                peers += "<tr>";
                peers += "<td>" + port + "</td>";
                peers += "<td";
                if (p.available == true) { peers += " class='green' "} else { peers += " class='red' "}
                peers += ">" + p.call + "</td>";
                peers += "<td>" + lastRX.toLocaleTimeString('de-DE') + "</td>";
                peers += "<td>" + p.rssi + "</td>";
                peers += "<td>" + p.snr + "</td>";
                peers += "<td>" + parseInt(p.frqError) + "</td>";
                peers += "</tr>";
            });
        }
        peers += "</table>";
        document.getElementById("peer").innerHTML = peers;
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
        document.getElementById("settingsWifiBrodcast").value = d.settings.wifiBrodcast[0] + "." + d.settings.wifiBrodcast[1] + "." + d.settings.wifiBrodcast[2] + "." + d.settings.wifiBrodcast[3];
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
        document.getElementById("myCall").innerHTML = d.settings.mycall;
        document.getElementById("settingsLoraRepeat").checked = d.settings.loraRepeat; 
        document.getElementById("settingsLoraMaxMessageLength").innerHTML = d.settings.loraMaxMessageLength + " characters"; 
        settings.titel = settings.name + " - " + settings.mycall;
        settings.altTitel = "🚨 " + settings.name + " - " + settings.mycall + " 🚨"

        if (init == false) {
            init = true;
            //for (let i = 0; i <= 10; i++) {channels[i] = false;} 
            //setUI(ui);
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
    settings["wifiBrodcast"] = document.getElementById("settingsWifiBrodcast").value.split('.').map(Number);
    settings["loraFrequency"] = parseFloat(document.getElementById("settingsLoraFrequency").value);
    settings["loraOutputPower"] = parseInt(document.getElementById("settingsLoraOutputPower").value);
    settings["loraBandwidth"] = parseFloat(document.getElementById("settingsLoraBandwidth").value);
    settings["loraSyncWord"] = parseInt(document.getElementById("settingsLoraSyncWord").value, 16);
    settings["loraCodingRate"] = parseInt(document.getElementById("settingsLoraCodingRate").value);
    settings["loraSpreadingFactor"] = parseInt(document.getElementById("settingsLoraSpreadingFactor").value);
    settings["loraPreambleLength"] = parseInt(document.getElementById("settingsLoraPreambleLength").value);
    settings["loraRepeat"] = document.getElementById("settingsLoraRepeat").checked;
    sendWS(JSON.stringify({settings: settings}));
}

function showMessages(parseAll = false) {
    if (parseAll) {
        //Alles löschen
        for (let i = 1; i <= 10; i++) { document.getElementById("channel" + i).innerHTML = ""; }
    }

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
                if (!parseAll) {channels[i] = true;}
            }
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

    //Websocket init
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    //sendWS(JSON.stringify({scanWifi: true }));
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
