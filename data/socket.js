		

function onMessage(event) {
    var d = JSON.parse(event.data);
    if (d.status === undefined) {console.log("RX: " + event.data);}

    //RAW-RX
    if (d.monitor) {

        f = d.monitor;
        var msg = "<span ";
        if (d.monitor.tx == true) {
            msg += "class='monitor-tx' >";
        } else {
            msg += ">";
        }
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

                if (f.messageType == 0) {msg += " TEXT: ";}
                if (f.messageType == 1) {msg += " TRACE: ";}
                if (f.messageType == 15) {msg += " COMMAND: ";}
                if (f.text) {
                    msg +=  f.text;
                }
                break;
            case 0x04: 
                msg += " Message ACK "; 
                if (f.id > 0) { msg += " ID: " + f.id + " "; }
                break;
        }
        document.getElementById("monitor").innerHTML = document.getElementById("monitor").innerHTML + "</span>" + msg;
        document.getElementById('monitor').scrollTop = document.getElementById("monitor").scrollHeight;
    }

    //Message empfangen
    if (d.message) {
        messages.push(d.message);
        showMessages(d.message);
        okSound.play();
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
        document.title = d.settings.name + " " + d.settings.version + " " + d.settings.mycall;
        document.getElementById("version").innerHTML = d.settings.name + " " + d.settings.version;
        document.getElementById("statusMyCall").innerHTML = "MyCall: " + d.settings.mycall;
        document.getElementById("settingsLoraRepeat").checked = d.settings.loraRepeat; 
        document.getElementById("settingsLoraMaxMessageLength").innerHTML = d.settings.loraMaxMessageLength + " characters"; 
        setUI(ui);
        settingsVisibility();
    }

    //Status
    if (d.status) {
        drawClock(new Date(d.status.time * 1000));
       if (d.status.tx) {
            document.getElementById("statusTRX").innerHTML = "TRX:&nbsp;<span style='color: #d9ff00;'> << TX >> </span>"; 
        } else if (d.status.rx) {
            document.getElementById("statusTRX").innerHTML = "TRX:&nbsp;<span style='color: #00ff00;'> >> RX << </span>"; 
        } else {
            document.getElementById("statusTRX").innerHTML = "TRX:&nbsp;<span>stby</span>"; 
        }
        document.getElementById("statusTxBufferCount").innerHTML = "TX-Buffer: " + d.status.txBufferCount; 
        document.getElementById("statusRetry").innerHTML = "Retry: " + d.status.retry; 

        
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
    var dstCall = document.getElementById('dstCall').innerHTML;
    if (dstCall == "..........") return;
    if (dstCall == "") {
        dstCall = (await inputBox("Destination Call?")).toUpperCase();
    }
    if (dstCall == "all") dstCall = "";
    var sendMessage = {};
    sendMessage["text"] = text;
    sendMessage["dstCall"] = dstCall;
    console.log(sendMessage);
    sendWS(JSON.stringify({sendMessage: sendMessage}));                    
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

function showMessages() {
    //Alles löschen
    for (let i = 1; i <= 10; i++) {
        document.getElementById("channel" + i).innerHTML = "";
    }
    messages.forEach(function(m) {
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
            if (m.messageType == 1) {msg += " [TRACE] ";}
            msg += ": " + m.text;
            msg += "</span>"
        }
        
        //Auf verschiedene Kanäle aufteilen
        var found = false;
        //Nachricht an alle
        if ((m.dstCall == "") && (found == false)) {
            found = true;
            document.getElementById("channel1").innerHTML += msg;
            channels[1].unread ++;
        }

        //Nachrichten an mich
        if ((m.dstCall == document.getElementById("settingsMycall").value) && (found == false)) {
            found = true;
            document.getElementById("channel2").innerHTML += msg;
            channels[2].unread ++;
        }

        for (let i = 1; i <= 10; i++) {
            //Trennung
            if (m.delimiter == true) {
                found = true;
                document.getElementById("channel" + i).innerHTML += "<span>^</span>";
            }
            //Nachricht an Gruppe 
            if ((m.dstCall == channels[i].dstCall) && (found == false)) {
                found = true;
                document.getElementById("channel" + i).innerHTML += msg;
                //channels[i].unread ++;
            }
        }        

        //Nachrichten, die ich gesendet habe
        if ((m.srcCall == document.getElementById("settingsMycall").value) && (found == false)) {
            found = true;
            document.getElementById("channel2").innerHTML += msg;
            //if (activeChannel != 2) {channels[2].unread ++;}
        }

        //Rest 
        if (found == false) {
            found = true;
            document.getElementById("channel1").innerHTML += msg;
            //if (activeChannel != 1) {channels[1].unread ++;}
        }

    });

    console.log( channels);

    //Nach unten scrollen
    for (let i = 1; i <= 10; i++) {
        document.getElementById('channel' + i).scrollTop = document.getElementById("channel" + i).scrollHeight;
        if (activeChannel == i) {channels[i].unread = 0;}
        if (channels[i].unread > 0) {document.getElementById("channelButton" + i).classList.add('unread');}
    }     

}



function initWebSocket() {
    var baseURL = "";
    var gateway = "";

    //Debug
    if (!window.location.hostname.includes("127.0.0.1")) {
        gateway = `ws://${window.location.hostname}/socket`;
        baseURL = "";
    } else {
        gateway = "ws://192.168.33.60/socket";
        baseURL = "http://192.168.33.60/"
    }

    //Nachrichten laden
    fetch(baseURL + "messages.json", )
        .then(response => response.text())
        .then(text => {
            const lines = text.split(/\r?\n/);
            lines.forEach(line => {
                if (line.trim().length === 0) return;
                const m = JSON.parse(line);
                messages.push(m.message);
        });

        const result = {"delimiter": true};
        messages.push(result);
        showMessages();

        for (let i = 1; i <= 10; i++) {
            channels[i].unread = 0;
            document.getElementById("channelButton" + i).classList.remove('unread');
        } 

    });				

    //Websocket init
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    //sendWS(JSON.stringify({scanWifi: true }));
    keepAlive();
}

function onClose(event) {
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
