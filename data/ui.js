var settingsDirty = false;
var _settingsSnapshot = null;

const SETTINGS_PANELS = ['lora', 'setup', 'network'];

function captureSettingsSnapshot() {
    _settingsSnapshot = {};
    SETTINGS_PANELS.forEach(panelId => {
        const panel = document.getElementById(panelId);
        if (!panel) return;
        panel.querySelectorAll('input, select, textarea').forEach(el => {
            if (!el.id) return;
            _settingsSnapshot[el.id] = (el.type === 'checkbox') ? el.checked : el.value;
        });
    });
}

function restoreSettings() {
    settingsDirty = false;

    if (!settings) return;

    fillSettingsForm(settings);

    if (settings.udpPeers) {
        renderUdpPeers(settings.udpPeers);
    } else {
        renderUdpPeers([]);
    }

    var pw1 = document.getElementById("settingsWebPassword");
    var pw2 = document.getElementById("settingsWebPasswordConfirm");
    var pwMatchRow = document.getElementById("settingsWebPasswordMatchRow");
    var pwMatch = document.getElementById("settingsWebPasswordMatch");

    if (pw1) pw1.value = "";
    if (pw2) pw2.value = "";
    if (pwMatchRow) pwMatchRow.style.display = "none";
    if (pwMatch) pwMatch.textContent = "";

    captureSettingsSnapshot();
    settingsVisibility();
}

function setUI(value) {
    if (SETTINGS_PANELS.includes(ui) && value !== ui && settingsDirty) {
        if (!confirm(t('settings.unsaved_confirm'))) return;
        restoreSettings();
    }
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
    document.getElementById("loraButton").classList.remove('selected');
    document.getElementById("setupButton").classList.remove('selected');
    document.getElementById("networkButton").classList.remove('selected');
    document.getElementById("aboutButton").classList.remove('selected');
    document.getElementById("monitor").classList.remove('big');
    document.getElementById("monitor").style.display = "none";
    document.getElementById("peer").style.display = "none";
    document.getElementById("routing").style.display = "none";
    document.getElementById("lora").style.display = "none";
    document.getElementById("setup").style.display = "none";
    document.getElementById("network").style.display = "none";
    document.getElementById("about").style.display = "none";
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
        case "lora":
            document.getElementById("loraButton").classList.add('selected');
            document.getElementById("lora").style.display = "";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "setup":
            document.getElementById("setupButton").classList.add('selected');
            document.getElementById("setup").style.display = "";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "network":
            document.getElementById("networkButton").classList.add('selected');
            document.getElementById("network").style.display = "";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "about":
            document.getElementById("aboutButton").classList.add('selected');
            document.getElementById("about").style.display = "";
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
    }

    document.getElementById('monitor').scrollTop = document.getElementById("monitor").scrollHeight;

    // Update mobile UI if in mobile mode
    if (typeof updateMobUI === 'function') updateMobUI();

    var globalUnread = false;
    for (let i = 1; i <= 10; i++) {
        document.getElementById('channel' + i).scrollTop = document.getElementById("channel" + i).scrollHeight;
        //Ungelesene Nachrichten anzeigen (nur wenn nicht muted)
        if ((channels[i] != false) && (document.hidden) && !channelMuted[i]) {globalUnread = true;}
        if ((channels[i] != false) && (activeChannel != i) && !channelMuted[i]) {document.getElementById("channelButton" + i).classList.add('unread');}
        if (activeChannel == i) {channels[i] = false;}
        // Mute/Sammelgruppe-Indikatoren im Button-Label
        if (i > 2) {
            let label = i + ":" + (Cookie.get("channel" + i) || "........");
            if (i === channelSammel) label += " 📥";
            else if (channelMuted[i]) label += " 🔕";
            document.getElementById("channelButton" + i).innerHTML = label;
        }
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
        for (e of document.getElementsByClassName('DHCP_ACTIVE')) {
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
            for (e of document.getElementsByClassName('DHCP_ACTIVE')) {
                e.style.display = "";
            }
        } else {
            for (e of document.getElementsByClassName('DHCP_ENABLED')) {
                e.style.display = "";
            }
            for (e of document.getElementsByClassName('DHCP_ACTIVE')) {
                e.style.display = "none";
            }
        }
    }
}


// Gruppen-Einstellungsdialog (Doppelklick auf Channel-Button)
function showChannelSettings(channelIdx) {
    document.querySelectorAll('.ch-settings-overlay').forEach(e => e.remove());

    const currentName = Cookie.get("channel" + channelIdx) || "";
    let localMuted        = channelMuted[channelIdx];
    let localSammel       = channelSammel;
    let localSammelGroups = sammelGroups.slice();

    const overlay = document.createElement('div');
    overlay.className = 'ch-settings-overlay';
    overlay.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,0.6);z-index:9998;display:flex;align-items:center;justify-content:center;';

    const dialog = document.createElement('div');
    dialog.style.cssText = 'background:#222;border:1px solid #555;border-radius:6px;padding:16px 18px;min-width:280px;max-width:340px;font-size:14px;color:#ddd;';
    dialog.innerHTML = `
        <div style="font-weight:bold;margin-bottom:12px;">Gruppe ${channelIdx}</div>
        <div id="chsNameRow">
            <label style="display:block;margin-bottom:4px;font-size:12px;color:#aaa;">Gruppenname (Ziel)</label>
            <input id="chsName" type="text" value="${currentName.replace(/"/g,'&quot;')}"
                   style="width:100%;box-sizing:border-box;padding:5px;background:#333;border:1px solid #666;border-radius:3px;color:#ddd;margin-bottom:12px;">
        </div>
        <div id="chsMiddle"></div>
        <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:16px;">
            <button id="chsCancel" style="padding:6px 14px;background:#444;border:none;border-radius:4px;color:#ddd;cursor:pointer;">Abbrechen</button>
            <button id="chsOk" style="padding:6px 14px;background:#4ecca3;border:none;border-radius:4px;color:#111;font-weight:bold;cursor:pointer;">OK</button>
        </div>`;
    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    function mkBtn(label, onclick) {
        const b = document.createElement('button');
        b.textContent = label;
        b.style.cssText = 'padding:5px 10px;background:#444;border:1px solid #666;border-radius:4px;color:#ddd;cursor:pointer;';
        b.onclick = onclick;
        return b;
    }

    function renderSamList(listEl) {
        listEl.innerHTML = '';
        if (localSammelGroups.length === 0) {
            listEl.innerHTML = '<div style="color:#888;font-size:12px;margin-bottom:4px;">(keine)</div>';
            return;
        }
        localSammelGroups.forEach((grp, idx) => {
            const row = document.createElement('div');
            row.style.cssText = 'display:flex;align-items:center;gap:6px;margin-bottom:4px;';
            const lbl = document.createElement('span');
            lbl.textContent = grp;
            lbl.style.flex = '1';
            const del = mkBtn('×', () => { localSammelGroups.splice(idx, 1); renderSamList(listEl); });
            del.style.padding = '2px 7px';
            row.appendChild(lbl);
            row.appendChild(del);
            listEl.appendChild(row);
        });
    }

    function renderMiddle() {
        const mid = dialog.querySelector('#chsMiddle');
        mid.innerHTML = '';
        // Gruppenname-Feld nur anzeigen wenn kein Sammelgruppe-Channel
        dialog.querySelector('#chsNameRow').style.display = (localSammel === channelIdx) ? 'none' : '';

        if (localSammel === channelIdx) {
            // Diese Gruppe ist die Sammelgruppe → Filterliste verwalten
            const hdr = document.createElement('div');
            hdr.style.cssText = 'font-size:12px;color:#aaa;margin-bottom:6px;';
            hdr.textContent = 'Gruppen, die hier gesammelt werden:';
            mid.appendChild(hdr);

            const listEl = document.createElement('div');
            listEl.id = 'chsSamList';
            listEl.style.marginBottom = '8px';
            renderSamList(listEl);
            mid.appendChild(listEl);

            const addRow = document.createElement('div');
            addRow.style.cssText = 'display:flex;gap:6px;margin-bottom:10px;';
            const addInput = document.createElement('input');
            addInput.type = 'text';
            addInput.placeholder = 'Gruppenname...';
            addInput.style.cssText = 'flex:1;padding:4px 6px;background:#333;border:1px solid #666;border-radius:3px;color:#ddd;';
            addInput.addEventListener('keydown', e => { if (e.key === 'Enter') addGroup(addInput); });
            const addBtn = mkBtn('+ Hinzufügen', () => addGroup(addInput));
            addRow.appendChild(addInput);
            addRow.appendChild(addBtn);
            mid.appendChild(addRow);

            mid.appendChild(mkBtn('📥 Sammelgruppe aufheben', () => {
                localSammel = 0;
                renderMiddle();
            }));
        } else {
            // Normaler Channel → Mute + Als Sammelgruppe
            const muteRow = document.createElement('div');
            muteRow.style.marginBottom = '8px';
            muteRow.appendChild(mkBtn(localMuted ? '🔔 Laut schalten' : '🔕 Stummschalten', () => {
                localMuted = !localMuted;
                renderMiddle();
            }));
            mid.appendChild(muteRow);

            mid.appendChild(mkBtn('📥 Als Sammelgruppe', () => {
                localSammel = channelIdx;
                renderMiddle();
            }));
        }
    }

    function addGroup(input) {
        const val = input.value.trim();
        if (val && !localSammelGroups.includes(val)) {
            localSammelGroups.push(val);
            const listEl = dialog.querySelector('#chsSamList');
            if (listEl) renderSamList(listEl);
        }
        input.value = '';
        input.focus();
    }

    renderMiddle();
    setTimeout(() => dialog.querySelector('#chsName').focus(), 10);

    function confirm() {
        if (localSammel !== channelIdx) {
            Cookie.set("channel" + channelIdx, dialog.querySelector('#chsName').value);
        } else {
            Cookie.set("channel" + channelIdx, "");
        }
        channelMuted[channelIdx] = localMuted;
        channelSammel = localSammel;
        sammelGroups = localSammelGroups;
        saveChannelFlags();
        overlay.remove();
        showMessages(true);
        setUI(ui);
    }

    dialog.querySelector('#chsOk').onclick = confirm;
    dialog.querySelector('#chsCancel').onclick = () => overlay.remove();
    overlay.addEventListener('click', e => { if (e.target === overlay) overlay.remove(); });
    dialog.querySelector('#chsName').addEventListener('keydown', e => {
        if (e.key === 'Enter') confirm();
        if (e.key === 'Escape') overlay.remove();
    });
}

function initSettingsDirtyTracking() {
    SETTINGS_PANELS.forEach(panelId => {
        const panel = document.getElementById(panelId);
        if (!panel) return;
        panel.addEventListener('input', () => { settingsDirty = true; });
        panel.addEventListener('change', () => { settingsDirty = true; });
    });
}

function initUI() {
    initSettingsDirtyTracking();
    //Aktionen für Channel Buttons
    for (let i = 1; i <= 10; i++) {
        if (i > 2) {
            document.getElementById("channelButton" + i).addEventListener("dblclick", function() {
                showChannelSettings(i);
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



