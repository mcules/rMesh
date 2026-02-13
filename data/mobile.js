const emojis = [
        // Gesichter & Smileys
        '😊','😂','🤣','😍','😎','🤔','😅','😉','🙄','🤨','😏','🥳','😭','😤','😱','🥱','😴',
        // Handzeichen & Menschen
        '👍','👎','👌','✌️','🤞','🤙','👏','🙌','🙏','💪','👋','🖐️','🤳','👈','👉',
        // Herzen & Symbole
        '❤️','✨','🔥','💥','💯','💢',
        // Technik & LoRa/Mesh
        '📡','📶','📻','💻','🔋','🔌','💡','📟','🛡️','🌍','🛰️','⚡','⚙️','🔧',
        // Status & Warnung
        '✅','❌','⚠️','🚫','🔔','🔕','🆘','🛑','🟢','🟡','🔴','💬','🗨️'
    ];

var guiSettings;
let wakeLock = null;

// Der Speicher im Hintergrund (während die Seite geladen ist)
const nameColorMap = {};
const distinctColors = [
    // --- Top 10 (Maximaler Kontrast) ---
    "#00f2ff", // Cyan (sehr hell)
    "#ffab40", // Orange
    "#b2ff59", // Hellgrün
    "#f06292", // Rosa/Pink
    "#ffd740", // Bernstein/Gelb
    "#9fa8da", // Indigo/Hellblau
    "#ff5252", // Hellrot
    "#e1bee7", // Helles Lila
    "#00e676", // Frühlingsgrün
    "#40c4ff", // Sky Blue
    "#ff1500ff", // Koralle
    "#15ff00ff", // Limette (pastell)
    "#00ffffff", // Aqua/Türkis
    "#0011ffff", // Magenta-Hell
    "#fbff00ff", // Tiefes Lila (hell)
];
let colorIndex = 0;

async function requestWakeLock() {
  try {
    wakeLock = await navigator.wakeLock.request('screen');
    wakeLock.addEventListener('release', () => {
    });
  } catch (err) {
    console.error(`${err.name}, ${err.message}`);
  }
}

document.addEventListener('click', requestWakeLock);

window.addEventListener('DOMContentLoaded', async function() {
    loadGuiSettings();
    buildMenu();

    initWebSocket();
    
	
});


document.getElementById("settingsScanWifi").addEventListener("click", function() {
    showModal("Note", "WiFi scan started. Results will be displayed in a few seconds.", "", false);
    document.getElementById('settingsSSIDList').innerHTML = "";
    sendWS(JSON.stringify({scanWifi: true }));
}); 

document.getElementById("settingsSSIDList").addEventListener("click", function() {
    document.getElementById("settingsSSID").value = document.getElementById("settingsSSIDList").value;
});


function enableNotifications() {
    Notification.requestPermission().then(function(result) {
        if (result === 'granted') {
            alert('Benachrichtigungen aktiviert!');
        }
    });
}




function buildMenu() {

    //Menüstruktur dynamisch bauen
    const menuItems = [
        { type: 'spacer' },
        { type: 'header', label: 'Groups' },
    ];

    //Gruppe alle
    menuItems.push(
        { 
            label: "all" , 
            action: () => {
                showContent("group_all");
                document.getElementById("group_all").scrollTo({top: document.getElementById("group_all").scrollHeight, behavior: 'smooth' });
            }
        }            
    );    
    const div = document.createElement("div"); 
    div.id = "group_all";
    div.classList.add("content-section"); 
    document.querySelector(".content-container").appendChild(div); 
    setupInputBar('group_all', mySendMessageFunction);   

    //Gruppen hinzufügen
    for (key in guiSettings.groups) { 
        const groupName  = guiSettings.groups[key].name; 
        //DIVs hinzu
        const container = document.querySelector(".content-container"); 
        if (!document.getElementById("group_" + groupName)) {
            const div = document.createElement("div"); 
            div.id = "group_" + groupName ;
            div.classList.add("content-section"); 
            container.appendChild(div);
            setupInputBar('group_' + groupName, mySendMessageFunction); 
        }
        //Menüeinträge hinzu
        menuItems.push(
            { 
                label: groupName , 
                action: () => {
                    showContent("group_" + groupName );
                    document.getElementById("group_" + groupName).scrollTo({top: document.getElementById("group_" + groupName).scrollHeight, behavior: 'smooth' });
                },
                longPressAction: () => {
                    //Gruppe löschen
                    showSelectionModal("Delete?", "Do you really want to delete " + groupName + "?",   ["yes"]).then(function(choice) {
                        if (choice === "yes") {
                            var newGroups = []; 
                            for (var i = 0; i < guiSettings.groups.length; i++) { 
                                if (guiSettings.groups[i].name !== groupName) { newGroups.push(guiSettings.groups[i]); } 
                            } 
                            guiSettings.groups = newGroups;
                            showMessages(true);
                            showContent("group_all");
                        } 
                    });
                }     
            }            
        );
    }
    menuItems.push(...[
        { 
            label: '-- new group --', 
            action: async () => {
                const name = await showModal("Add new group", "Name:", "", true);
                const newGroup = {
                    name: name, 
                    read: true};
                if (name) {
                    guiSettings.groups.push(newGroup);
                    showMessages(true);
                    showContent("group_" + name);
                }
            }    
        },    
        { type: 'spacer' },
        { type: 'header', label: 'Direct Messages' }]);

    //DM hinzufügen
    for (key in guiSettings.dm) { 
        const callsign  = guiSettings.dm[key].name; 
        //DIVs hinzu
        const container = document.querySelector(".content-container"); 
        if (!document.getElementById("dm_" + callsign)) {
            const div = document.createElement("div"); 
            div.id = "dm_" + callsign ;
            div.classList.add("content-section"); 
            container.appendChild(div);
            setupInputBar('dm_' + callsign, mySendMessageFunction); 
        }
        //Menüeinträge hinzu
        menuItems.push(
            { 
                label: callsign , 
                action: () => {
                    showContent("dm_" + callsign );
                    document.getElementById("dm_" + callsign).scrollTo({top: document.getElementById("dm_" + callsign).scrollHeight, behavior: 'smooth' });
                }
            }            
        );
    }
    menuItems.push(...[
        { 
            label: '-- new contact --', 
            action: async () => {
                var name = await showModal("Add new contact", "Callsign:", "", true);
                if (name) {
                    name = name.toUpperCase();
                    const newDM = {
                        name: name, 
                        read: true
                    };

                    var exists = false;
                    for (var i = 0; i < guiSettings.dm.length; i++) {
                        if (guiSettings.dm[i].name === name) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        guiSettings.dm.push(newDM);
                        showMessages(true);
                        showContent("dm_" + name);
                    }

                    showMessages(true);
                }
            }    
        },  


        { type: 'spacer' },
        { type: 'header', label: 'Info'},
        { 
            label: 'Monitor', 
            action: function() { 
                showContent('cMonitor'); 
                window.scrollTo({ 
                    top: document.body.scrollHeight, 
                    behavior: 'smooth' 
                });
            }
        },
        { 
            label: 'Peers', 
            action: () => showContent('cPeers') 
        },
        { 
            label: 'Routing', 
            action: () => showContent('cRouting') 
        },
        { type: 'spacer' },
        { type: 'header', label: 'Settings' },
        { 
            label: 'Network', 
            action: () => showContent('cNetwork') 
        },
        { 
            label: 'LoRa', 
            action: () => showContent('cLora') 
        },
        { 
            label: 'About', 
            action: () => showContent('cAbout') 
        }
    ]);


    const menuList = document.getElementById('menu-list');
    menuList.innerHTML = '';

    menuItems.forEach(item => {
        const li = document.createElement('li');

        if (item.type === 'spacer') {
            li.classList.add('separator');
        } 
        else if (item.type === 'header') {
            li.classList.add('menu-header');
            li.textContent = item.label;
        } 
        else {
            li.textContent = item.label;
            li.id = "mnu_" + item.label;
            
            let pressTimer;

            // Funktion für den Start des Drückens
            const startPress = (e) => {
                // Timer starten (z.B. 800ms für Long Press)
                pressTimer = setTimeout(() => {
                    if (typeof item.longPressAction === 'function') {
                        item.longPressAction();
                        // Optional: Vibration für haptisches Feedback auf dem Handy
                        toggleMenu();
                        if (navigator.vibrate) navigator.vibrate(50);
                    }
                }, 800);
            };

            // Funktion zum Abbrechen des Timers
            const cancelPress = () => {
                clearTimeout(pressTimer);
            };

            // Events für Touch (Mobile) und Maus (Desktop)
            li.addEventListener('mousedown', startPress);
            li.addEventListener('touchstart', startPress, { passive: true });
            li.addEventListener('mouseup', cancelPress);
            li.addEventListener('mouseleave', cancelPress);
            li.addEventListener('touchend', cancelPress);
            li.addEventListener('touchmove', cancelPress); // Abbrechen, wenn gescrollt wird

            // Normaler Klick
            li.onclick = () => {
                if (typeof item.action === 'function') {
                    item.action();
                    //document.getElementById("mnu_" + item.label).classList.remove('newMessages');
                    document.getElementById("title").innerHTML = settings.name + " - " + item.label;
                    document.title = settings.name + " - " + item.label;
                }
                toggleMenu();
            };
        }

        menuList.appendChild(li);

    });
    saveGuiSettings();
    
}


function saveGuiSettings() {
    const json = JSON.stringify(guiSettings);
    document.cookie = "guiSettings=" + encodeURIComponent(json) + "; expires=Fri, 31 Dec 9999 23:59:59 GMT; path=/";
}


function loadGuiSettings() {
    const cookies = document.cookie.split("; ");
    for (const c of cookies) {
        const [name, value] = c.split("=");
        if (name === "guiSettings") {
            guiSettings =  JSON.parse(decodeURIComponent(value));
            guiSettings.dm = [];
            return;
        }
    }
    guiSettings = { 
        groups: [],
        dm: [],
        menu: "cMonitor",
        update: 0
    };
}


function toggleMenu() {
	const menu = document.getElementById('side-menu');
	menu.classList.toggle('open');
}

function showContent(sectionId) {
	// 1. Alle Sektionen verstecken
	const sections = document.querySelectorAll('.content-section');
	sections.forEach(section => {
		section.classList.remove('active');
	});

	// 2. Gewählte Sektion anzeigen
    if (document.getElementById(sectionId)) {
    	document.getElementById(sectionId).classList.add('active');
    } else {
        document.getElementById("cMonitor").classList.add('active');           
    }
    showMessages(true);
    window.scrollTo(0, 0); 
    guiSettings.menu = sectionId;
    saveGuiSettings();

}



/**
 * Zeigt ein Modal an.
 * @param {string} title - Überschrift
 * @param {string} desc - Beschreibungstext
 * @param {string} defaultValue - Startwert für den Input
 * @param {boolean} isInput - true = mit Eingabefeld, false = reine Nachricht
 * @returns {Promise<string|boolean>} - Der Text oder true/false bei Nachricht
 */
function showModal(title, desc, defaultValue = "", isInput = true) {
    return new Promise((resolve) => {
        const modal = document.getElementById('custom-modal');
        const inputField = document.getElementById('modal-input');
        const submitBtn = document.getElementById('modal-submit');

        // Inhalte setzen
        document.getElementById('modal-title').textContent = title;
        document.getElementById('modal-description').textContent = desc;
        
        // Input-Box zeigen oder verstecken
        inputField.style.display = isInput ? "block" : "none";
        inputField.value = defaultValue;

        // Modal anzeigen
        modal.style.display = "flex";

        // Event-Handler für den Submit-Button
        submitBtn.onclick = () => {
            const result = isInput ? inputField.value : true;
            closeModal();
            resolve(result);
        };

        // Abbrechen-Logik
        document.getElementById('modal-cancel').onclick = () => {
            closeModal();
            resolve(null); // Gibt null zurück, wenn abgebrochen wurde
        };
    });
}

function closeModal() {
    document.getElementById('custom-modal').style.display = "none";
}



/**
 * Erzeugt eine Sprechblase mit flexibler CSS-Klasse
 * @param {string} bubbleClass - Die CSS-Klasse (z.B. 'right', 'left', 'system', 'admin')
 * @param {string} title - Name des Absenders
 * @param {string} subtitle - Zusatz (z.B. Uhrzeit)
 * @param {string} titleColor - Farbe des Namens
 * @param {string} text - Inhalt
 * @param {string} containerId - Ziel-Container
 */
function addBubble(bubbleClass, title, subtitle, titleColor, text, containerId) {
    var container = document.getElementById(containerId);
    if (!container) return;

    var row = document.createElement('div');
    // Klassische String-Verknüpfung
    row.className = 'bubble-row ' + bubbleClass;

    var bubble = document.createElement('div');
    bubble.className = 'bubble';

    var header = document.createElement('div');
    header.className = 'bubble-header';
    
    var titleSpan = document.createElement('span');
    titleSpan.textContent = title;
    titleSpan.style.color = titleColor;

    var timeSpan = document.createElement('span');
    timeSpan.className = 'bubble-time';
    timeSpan.textContent = subtitle;

    header.appendChild(titleSpan);
    header.appendChild(timeSpan);

    var content = document.createElement('div');
    content.className = 'bubble-text';
    content.textContent = text;

    bubble.appendChild(header);
    bubble.appendChild(content);
    row.appendChild(bubble);

    container.appendChild(row);

    container.scrollTo({
        top: container.scrollHeight,
        behavior: 'smooth'
    });

}




function setupInputBar(sectionId, onSendCallback) {
    const section = document.getElementById(sectionId);
    if (!section) return;

    section.classList.add('has-input');
    const existingBar = section.querySelector('.input-bar');
    if (existingBar) existingBar.remove();

    const bar = document.createElement('div');
    bar.className = 'input-bar';

    const emojiBtn = document.createElement('button');
    emojiBtn.className = 'emoji-btn';
    emojiBtn.innerHTML = '😊';
    emojiBtn.type = 'button';

    const picker = document.createElement('div');
    picker.className = 'emoji-picker';
    picker.style.display = 'none';
    
    // Maximale Länge aus den Einstellungen holen (mit Fallback auf 200, falls d.settings fehlt)
    const maxLength = (window.d && d.settings && d.settings.loraMaxMessageLength) ? d.settings.loraMaxMessageLength : 200;

    emojis.forEach(emoji => {
        const span = document.createElement('span');
        span.innerHTML = emoji;
        span.onclick = () => {
            // Prüfen, ob durch das Emoji das Limit überschritten würde
            if (input.value.length + emoji.length <= maxLength) {
                input.value += emoji;
                input.focus();
            } else {
                alert("Nachricht zu lang!"); // Optionaler Hinweis
            }
            picker.style.display = 'none';
        };
        picker.appendChild(span);
    });

    const input = document.createElement('input');
    input.type = 'text';
    input.placeholder = 'type a message....';
    
    // --- WICHTIG: Die Begrenzung direkt am Input-Feld setzen ---
    input.maxLength = maxLength; 
    
    const button = document.createElement('button');
    button.className = 'send-btn';
    button.innerHTML = '➤';

    emojiBtn.onclick = (e) => {
        e.stopPropagation();
        const isVisible = picker.style.display === 'grid';
        picker.style.display = isVisible ? 'none' : 'grid';
    };

    document.addEventListener('click', () => { picker.style.display = 'none'; });
    picker.onclick = (e) => e.stopPropagation();

    const handleSend = () => {
        // Sicherheitshalber auch hier nochmal kürzen (Slice)
        const text = input.value.trim().substring(0, maxLength);
        if (text !== "") {
            onSendCallback(sectionId, text);
            input.value = "";
            picker.style.display = 'none';
        }
    };

    button.onclick = handleSend;
    input.onkeydown = (e) => { if (e.key === 'Enter') handleSend(); };

    bar.appendChild(emojiBtn);
    bar.appendChild(picker);
    bar.appendChild(input);
    bar.appendChild(button);
    section.appendChild(bar);
}


function getColorForName(name) {
    if (name == settings.mycall) return "#009eaf";

    // Falls der Name schon eine Farbe hat, diese zurückgeben
    if (nameColorMap[name]) {
        return nameColorMap[name];
    }

    // Wenn der Name neu ist: Farbe aus der Liste zuordnen
    const assignedColor = distinctColors[colorIndex];

    // Im Speicher hinterlegen
    nameColorMap[name] = assignedColor;

    // Index für den nächsten Namen erhöhen (und bei Ende der Liste vorne anfangen)
    colorIndex = (colorIndex + 1) % distinctColors.length;

    return assignedColor;
}




function showSelectionModal(title, desc, options = []) {
    return new Promise((resolve) => {
        const modal = document.getElementById('custom-modal');
        const inputField = document.getElementById('modal-input');
        const submitBtn = document.getElementById('modal-submit');
        const selectionList = document.getElementById('modal-selection-list');

        // Standard-Inhalte setzen
        document.getElementById('modal-title').textContent = title;
        document.getElementById('modal-description').textContent = desc;
        
        // Input und Standard-Submit ausblenden
        inputField.style.display = "none";
        submitBtn.style.display = "none";

        // Liste leeren und anzeigen
        selectionList.innerHTML = '';
        selectionList.style.display = "block";

        // Optionen dynamisch erstellen
        options.forEach(optText => {
            const btn = document.createElement('button');
            btn.className = 'modal-option';
            btn.textContent = optText;
            btn.onclick = () => {
                cleanup();
                resolve(optText);
            };
            selectionList.appendChild(btn);
        });

        // Modal anzeigen
        modal.style.display = "flex";

        // Hilfsfunktion zum Schließen und Aufräumen
        const cleanup = () => {
            selectionList.style.display = "none";
            submitBtn.style.display = "block"; // Für das nächste normale Modal wieder zeigen
            closeModal();
        };

        // Abbrechen-Logik
        document.getElementById('modal-cancel').onclick = () => {
            cleanup();
            resolve(null);
        };
    });
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


function setAntennaColor(hexColor) {
    const antenna = document.getElementById('antenna-icon');
    if (!antenna) return;

    // Direktes Setzen der Styles auf das SVG Element
    antenna.style.transition = 'none';
    antenna.style.stroke = hexColor;
    antenna.style.fill = hexColor;

    // Falls die Bögen im SVG "fill: none" haben müssen (damit sie nicht ausgefüllt werden):
    const paths = antenna.querySelectorAll('path');
    paths.forEach(p => p.style.fill = 'none');
}


// 4. Die Funktion, die beim Senden aufgerufen wird (Beispiel)
function mySendMessageFunction(id, text) {
    if (id) {
        //DM
        if (id.substring(0, 3) == "dm_") {
            var dst = id.substring(3).toUpperCase();
            //Nachricht vorbereiten und über Websocket senden
            var message = {};
            message["text"] = text;
            message["dst"] = dst;
            sendWS(JSON.stringify({sendMessage: message}));                    

        }
        //Group
        if (id.substring(0, 6) == "group_") {
            var dst = id.substring(6);
            if (dst == "all") dst = "";
            var message = {};
            message["text"] = text;
            message["dst"] = dst;
            sendWS(JSON.stringify({sendGroup: message}));                    
        }
    }
}