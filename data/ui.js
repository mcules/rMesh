var settingsDirty = false;
var _settingsSnapshot = null;

const SETTINGS_PANELS = ['lora', 'setup', 'network'];

// On mobile, use "block" so content flows top-to-bottom; on desktop, use "flex" for column layout
function showPanel(el) {
    if (typeof el === 'string') el = document.getElementById(el);
    el.style.display = (window.innerWidth < 768) ? 'block' : 'flex';
}

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
            showPanel("channel1");
            document.getElementById("channelButton1").classList.add('selected');
            showPanel("monitor");
            document.getElementById("messageText1").style.display = "flex";
            document.getElementById("messageText1").focus();
            document.getElementById("dstCall").innerHTML = "all";
            activeChannel = 1;
            break;
        case "channel2":
            showPanel("channel2");
            document.getElementById("channelButton2").classList.add('selected');
            showPanel("monitor");
            document.getElementById("messageText2").style.display = "flex";
            document.getElementById("messageText2").focus();
            document.getElementById("dstCall").innerHTML = Cookie.get("channel2", "");
            activeChannel = 2;
            break;
        case "channel3": case "channel4": case "channel5": case "channel6":
        case "channel7": case "channel8": case "channel9": case "channel10":
            var chNum = parseInt(ui.replace("channel", ""));
            showPanel("channel" + chNum);
            document.getElementById("channelButton" + chNum).classList.add('selected');
            showPanel("monitor");
            document.getElementById("messageText" + chNum).style.display = "flex";
            if (!channelSammel[chNum]) {
                document.getElementById("messageText" + chNum).focus();
                document.getElementById("dstCall").innerHTML = Cookie.get("channel" + chNum, "");
            } else {
                document.getElementById("dstCall").innerHTML = "📥 " + (sammelNames[chNum] || "Sammelgruppe");
            }
            activeChannel = chNum;
            break;
        case "monitor":
            document.getElementById("monitorButton").classList.add('selected');
            document.getElementById("monitor").classList.add('big');
            showPanel("monitor");
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "peer":
            document.getElementById("peerButton").classList.add('selected');
            showPanel("peer");
            document.getElementById("messageText0").style.display = "flex";
            document.getElementById("dstCall").innerHTML = "";
            break;
        case "routing":
            document.getElementById("routingButton").classList.add('selected');
            showPanel("routing");
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
        // Mute/collection group indicators in button label
        if (i <= 2) {
            let label = i === 1 ? "1: all" : "2:" + (settings ? settings.mycall : "");
            if (channelMuted[i]) label += " 🔕";
            document.getElementById("channelButton" + i).innerHTML = label;
        } else {
            let label;
            if (channelSammel[i]) {
                label = i + ":" + (sammelNames[i] || "📥");
                label += " 📥";
            } else {
                label = i + ":" + (Cookie.get("channel" + i) || "........");
                if (channelMuted[i]) label += " 🔕";
            }
            document.getElementById("channelButton" + i).innerHTML = label;
        }
        // Disable text input for collection group channels
        if (i > 2) {
            var ta = document.getElementById("messageText" + i);
            if (channelSammel[i]) {
                ta.disabled = true;
                ta.placeholder = "📥 " + (sammelNames[i] || "Collection group") + " (receive only)";
            } else {
                ta.disabled = false;
                ta.placeholder = "";
            }
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
        for (e of document.getElementsByClassName('AP_ONLY')) {
            e.style.display = "";
        }
    } else {
        for (e of document.getElementsByClassName('AP_ONLY')) {
            e.style.display = "none";
        }
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
    let localIsSammel     = channelSammel[channelIdx];
    let localSammelGroups = (sammelGroups[channelIdx] || []).slice();
    let localSammelName   = sammelNames[channelIdx] || "";

    const overlay = document.createElement('div');
    overlay.className = 'ch-settings-overlay';

    const dialog = document.createElement('div');
    dialog.className = 'ch-settings-dialog';
    var chTitle = channelIdx === 1 ? 'all' : channelIdx === 2 ? 'direct' : 'Gruppe ' + channelIdx;
    dialog.innerHTML =
        '<h3>' + chTitle + '</h3>' +
        '<div id="chsNameRow"' + (channelIdx <= 2 ? ' style="display:none"' : '') + ' class="ch-section">' +
            '<label>Gruppenname (Ziel)</label>' +
            '<input id="chsName" type="text" value="' + currentName.replace(/"/g,'&quot;') + '">' +
        '</div>' +
        '<div id="chsMiddle"></div>' +
        '<div class="ch-footer">' +
            '<button id="chsCancel" class="ch-btn">Abbrechen</button>' +
            '<button id="chsOk" class="ch-btn ch-btn-active">OK</button>' +
        '</div>';
    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    function mkBtn(label, cls, onclick, title) {
        const b = document.createElement('button');
        b.textContent = label;
        b.className = 'ch-btn' + (cls ? ' ' + cls : '');
        if (title) b.title = title;
        b.onclick = onclick;
        return b;
    }

    function renderSamList(listEl) {
        listEl.innerHTML = '';
        if (localSammelGroups.length === 0) {
            listEl.innerHTML = '<div style="color:#888;font-size:12px;margin-bottom:4px;">(keine)</div>';
            return;
        }
        localSammelGroups.forEach(function(grp, idx) {
            const row = document.createElement('div');
            row.className = 'ch-sam-item';
            const lbl = document.createElement('span');
            lbl.textContent = grp;
            const del = mkBtn('\u00d7', 'ch-btn-danger', function() { localSammelGroups.splice(idx, 1); renderSamList(listEl); });
            del.style.padding = '2px 8px';
            del.style.height = '22px';
            row.appendChild(lbl);
            row.appendChild(del);
            listEl.appendChild(row);
        });
    }

    function renderMiddle() {
        const mid = dialog.querySelector('#chsMiddle');
        mid.innerHTML = '';
        dialog.querySelector('#chsNameRow').style.display = (channelIdx <= 2 || localIsSammel) ? 'none' : '';

        if (localIsSammel) {
            // Sammelgruppe: Name
            var sec = document.createElement('div');
            sec.className = 'ch-section';
            sec.innerHTML = '<label>Name der Sammelgruppe</label>';
            const nameInput = document.createElement('input');
            nameInput.id = 'chsSamName';
            nameInput.type = 'text';
            nameInput.value = localSammelName;
            nameInput.placeholder = 'z.B. Notfall, Info, ...';
            nameInput.addEventListener('input', function() { localSammelName = nameInput.value; });
            sec.appendChild(nameInput);
            mid.appendChild(sec);

            // Sammelgruppe: Quell-Gruppen
            var sec2 = document.createElement('div');
            sec2.className = 'ch-section';
            sec2.innerHTML = '<label>Gruppen, die hier gesammelt werden</label>';
            const listEl = document.createElement('div');
            listEl.id = 'chsSamList';
            listEl.style.marginBottom = '6px';
            renderSamList(listEl);
            sec2.appendChild(listEl);

            const addRow = document.createElement('div');
            addRow.className = 'ch-add-row';
            const addInput = document.createElement('input');
            addInput.type = 'text';
            addInput.placeholder = 'Gruppenname...';
            addInput.addEventListener('keydown', function(e) { if (e.key === 'Enter') addGroup(addInput); });
            const addBtn = mkBtn('+', '', function() { addGroup(addInput); });
            addRow.appendChild(addInput);
            addRow.appendChild(addBtn);
            sec2.appendChild(addRow);
            mid.appendChild(sec2);

            mid.appendChild(mkBtn('📥', 'ch-btn-danger ch-btn-icon', function() {
                localIsSammel = false;
                renderMiddle();
            }, 'Sammelgruppe aufheben'));
        } else {
            // Icon-Buttons nebeneinander
            var row = document.createElement('div');
            row.className = 'ch-icon-row';
            row.appendChild(mkBtn(localMuted ? '🔔' : '🔕', 'ch-btn-icon' + (localMuted ? ' ch-btn-active' : ''), function() {
                localMuted = !localMuted;
                renderMiddle();
            }, localMuted ? 'Laut schalten' : 'Stummschalten'));
            if (channelIdx > 2) {
                row.appendChild(mkBtn('📥', 'ch-btn-icon', function() {
                    localIsSammel = true;
                    renderMiddle();
                }, 'Als Sammelgruppe'));
            }
            mid.appendChild(row);
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
    if (channelIdx > 2) setTimeout(function() { dialog.querySelector('#chsName').focus(); }, 10);

    function confirm() {
        var newName = "";
        if (!localIsSammel) {
            newName = dialog.querySelector('#chsName').value;
            Cookie.set("channel" + channelIdx, newName);
        } else {
            Cookie.set("channel" + channelIdx, "");
        }
        channelMuted[channelIdx] = localMuted;
        channelSammel[channelIdx] = localIsSammel;
        sammelGroups[channelIdx] = localSammelGroups;
        sammelNames[channelIdx] = localSammelName;
        if (!localIsSammel) {
            delete sammelGroups[channelIdx];
            delete sammelNames[channelIdx];
        }
        saveChannelFlags();
        // Persist group name to device
        if (channelIdx >= 3) {
            var gn = {};
            gn[String(channelIdx)] = newName;
            sendWS(JSON.stringify({settings: {groupNames: gn}}));
        }
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
        document.getElementById("channelButton" + i).addEventListener("dblclick", function() {
            showChannelSettings(i);
        });

        document.getElementById('messageText' + i).addEventListener('input', function() {
            checkMsgLength(this);
        });

        document.getElementById('messageText' + i).addEventListener('keydown', function(e) {
            if (e.key === 'Enter') {
                const pos = this.selectionStart;
                const text = this.value;
                const startOfLine = text.lastIndexOf('\n', pos - 1) + 1;
                let endOfLine = text.indexOf('\n', pos);
                if (endOfLine === -1) endOfLine = text.length;
                const currentLineText = text.substring(startOfLine, endOfLine);
                sendMessage(currentLineText, i,);
                e.preventDefault();
                this.value = '';
            }
        });
    }   


}



