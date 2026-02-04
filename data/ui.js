function setUI(value) {
    ui = value;
    Cookie.set("ui", ui);
    //Alles ausblenden
    for (let i = 1; i <= 10; i++) {
        document.getElementById("channel" + i).style.display = "none";
        document.getElementById("messageText" + i).style.display = "none";
        document.getElementById("channelButton" + i).classList.remove('selected');
        document.getElementById("channelButton" + i).classList.remove('unread');
        if (Cookie.get("channel" + i)) {
            document.getElementById("channelButton" + i).innerHTML = i + ":" + Cookie.get("channel" + i);
        } else {
            document.getElementById("channelButton" + i).innerHTML = i + ":........";
        }
        document.getElementById("channelButton1").innerHTML = "1: all";
        if (settings) document.getElementById("channelButton2").innerHTML = "2:" + settings.mycall; //document.getElementById("settingsMycall").value;
    }
    document.getElementById("messageText0").style.display = "none";
    document.getElementById("monitorButton").classList.remove('selected');
    document.getElementById("peerButton").classList.remove('selected');
    document.getElementById("routingButton").classList.remove('selected');
    document.getElementById("dosButton").classList.remove('selected');
    document.getElementById("tuneButton").classList.remove('selected');
    document.getElementById("announceButton").classList.remove('selected');
    document.getElementById("setupButton").classList.remove('selected');
    document.getElementById("networkButton").classList.remove('selected');
    document.getElementById("monitor").classList.remove('big');
    document.getElementById("monitor").style.display = "none";
    document.getElementById("peer").style.display = "none";
    document.getElementById("routing").style.display = "none";
    document.getElementById("setup").style.display = "none";
    document.getElementById("network").style.display = "none";
    document.getElementById("dstCall").innerHTML = "";
    activeChannel = 0;

    switch (ui) {
        case "channel1":
            document.getElementById("channel1").style.display = "flex";
            document.getElementById("channelButton1").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText1").style.display = "flex";
            document.getElementById("messageText1").focus();
            document.getElementById("dstCall").innerHTML = "all";
            activeChannel = 1;
            break;
        case "channel2":
            document.getElementById("channel2").style.display = "flex";
            document.getElementById("channelButton2").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText2").style.display = "flex";
            document.getElementById("messageText2").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel2", "");
            activeChannel = 2;
            break;
        case "channel3":
            document.getElementById("channel3").style.display = "flex";
            document.getElementById("channelButton3").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText3").style.display = "flex";
            document.getElementById("messageText3").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel3", "");
            activeChannel = 3;
            break;
        case "channel4":
            document.getElementById("channel4").style.display = "flex";
            document.getElementById("channelButton4").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText4").style.display = "flex";
            document.getElementById("messageText4").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel4", "");
            activeChannel = 4;
            break;
        case "channel5":
            document.getElementById("channel5").style.display = "flex";
            document.getElementById("channelButton5").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText5").style.display = "flex";
            document.getElementById("messageText5").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel5", "");
            activeChannel = 5;
            break;
        case "channel6":
            document.getElementById("channel6").style.display = "flex";
            document.getElementById("channelButton6").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText6").style.display = "flex";
            document.getElementById("messageText6").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel6", "");
            activeChannel = 6;
            break;
        case "channel7":
            document.getElementById("channel7").style.display = "flex";
            document.getElementById("channelButton7").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText7").style.display = "flex";
            document.getElementById("messageText7").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel7", "");
            activeChannel = 7;
            break;
        case "channel8":
            document.getElementById("channel8").style.display = "flex";
            document.getElementById("channelButton8").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText8").style.display = "flex";
            document.getElementById("messageText8").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel8", "");
            activeChannel = 8;
            break;
        case "channel9":
            document.getElementById("channel9").style.display = "flex";
            document.getElementById("channelButton9").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText9").style.display = "flex";
            document.getElementById("messageText9").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel9", "");
            activeChannel = 9;
            break;
        case "channel10":
            document.getElementById("channel10").style.display = "flex";
            document.getElementById("channelButton10").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText10").style.display = "flex";
            document.getElementById("messageText10").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel10", "");
            activeChannel = 10;
            break;
        case "monitor":
            document.getElementById("monitorButton").classList.add('selected');
            document.getElementById("monitor").classList.add('big');
            document.getElementById("monitor").style.display = "flex";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "peer":
            document.getElementById("peerButton").classList.add('selected');
            document.getElementById("peer").style.display = "flex";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "routing":
            document.getElementById("routingButton").classList.add('selected');
            document.getElementById("routing").style.display = "flex";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "setup":
            document.getElementById("setupButton").classList.add('selected');
            document.getElementById("setup").style.display = "flex";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "network":
            document.getElementById("networkButton").classList.add('selected');
            document.getElementById("network").style.display = "flex";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
    }

    document.getElementById('monitor').scrollTop = document.getElementById("monitor").scrollHeight;
    var globalUnread = false;
    for (let i = 1; i <= 10; i++) {
        document.getElementById('channel' + i).scrollTop = document.getElementById("channel" + i).scrollHeight
        //Ungelesene Nachrichten anzeigen
        if ((channels[i] != false) && (document.hidden)) {globalUnread = true;}
        if ((channels[i] != false) && (activeChannel != i)) {document.getElementById("channelButton" + i).classList.add('unread');}
        if (activeChannel == i) {channels[i] = false;}
    }      

    //Fenstertitel
    if (globalUnread) {
        if (settings) {document.title = settings.altTitel; }
    } else {
        if (settings) {document.title = settings.titel; }
    }
}



function settingsVisibility() {
    if (document.getElementById("settingsApMode").checked) {
        for (e of document.getElementsByClassName('DHCP_ENABLED')) {
            e.style.display = "none";
        }
        for (e of document.getElementsByClassName('AP_MODE_ENABLED')) {
            e.style.display = "none";
        }				
    } else {
        for (e of document.getElementsByClassName('AP_MODE_ENABLED')) {
            e.style.display = "";
        }				
        if (document.getElementById("settingsDHCP").checked) {
            for (e of document.getElementsByClassName('DHCP_ENABLED')) {
                e.style.display = "none";
            }
        } else {
            for (e of document.getElementsByClassName('DHCP_ENABLED')) {
                e.style.display = "";
            }
        }
    }
}


function initUI() {
    //Aktionen für Channel Buttons
    for (let i = 1; i <= 10; i++) {
        if (i > 2) {
            document.getElementById("channelButton" + i).addEventListener("dblclick", async function() {
                var value = await inputBox("Group Name?");
                Cookie.set("channel" + i, value);
                showMessages(true);
            });
        }

        document.getElementById('messageText' + i).addEventListener('keydown', function(e) {
            if (e.key === 'Enter') {
                const pos = this.selectionStart;
                const text = this.value;
                const startOfLine = text.lastIndexOf('\n', pos - 1) + 1;
                let endOfLine = text.indexOf('\n', pos);
                if (endOfLine === -1) endOfLine = text.length;
                const currentLineText = text.substring(startOfLine, endOfLine);
                sendMessage(currentLineText, i,);
                
                if (pos < text.length) {
                    e.preventDefault(); 
                    const nextLineIndex = text.indexOf('\n', pos);
                    if (nextLineIndex !== -1) {
                        this.setSelectionRange(nextLineIndex + 1, nextLineIndex + 1);
                    } else {
                        this.setSelectionRange(text.length, text.length);
                    }
                }
            }
        });
    }   


}



