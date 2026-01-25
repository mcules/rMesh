function setUI(value) {
    ui = value;
    //Alles ausblenden
    for (let i = 1; i <= 10; i++) {
        document.getElementById("channel" + i).style.display = "none";
        document.getElementById("channelButton" + i).classList.remove('selected');
    }
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

    switch (ui) {
        case "channel1":
            document.getElementById("channel1").style.display = "flex";
            document.getElementById("channelButton1").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel2":
            document.getElementById("channel2").style.display = "flex";
            document.getElementById("channelButton2").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel3":
            document.getElementById("channel3").style.display = "flex";
            document.getElementById("channelButton3").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel4":
            document.getElementById("channel4").style.display = "flex";
            document.getElementById("channelButton4").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel5":
            document.getElementById("channel5").style.display = "flex";
            document.getElementById("channelButton5").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel6":
            document.getElementById("channel6").style.display = "flex";
            document.getElementById("channelButton6").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel7":
            document.getElementById("channel7").style.display = "flex";
            document.getElementById("channelButton7").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel8":
            document.getElementById("channel8").style.display = "flex";
            document.getElementById("channelButton8").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel9":
            document.getElementById("channel9").style.display = "flex";
            document.getElementById("channelButton9").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "channel10":
            document.getElementById("channel10").style.display = "flex";
            document.getElementById("channelButton10").classList.add('selected');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "monitor":
            document.getElementById("monitorButton").classList.add('selected');
            document.getElementById("monitor").classList.add('big');
            document.getElementById("monitor").style.display = "flex";
            break;
        case "peer":
            document.getElementById("peerButton").classList.add('selected');
            document.getElementById("peer").style.display = "flex";
            break;
        case "routing":
            document.getElementById("routingButton").classList.add('selected');
            document.getElementById("routing").style.display = "flex";
            break;
        case "setup":
            document.getElementById("setupButton").classList.add('selected');
            document.getElementById("setup").style.display = "flex";
            break;
        case "network":
            document.getElementById("networkButton").classList.add('selected');
            document.getElementById("network").style.display = "flex";
            break;
    }

    document.getElementById('monitor').scrollTop = document.getElementById("monitor").scrollHeight;
    for (let i = 1; i <= 10; i++) {
        document.getElementById('channel' + 1).scrollTop = document.getElementById("channel1").scrollHeight;
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



