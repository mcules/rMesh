// --- DER ULTIMATIVE VISUAL VIEWPORT FIX ---
function applyVisualViewport() {
    if (window.visualViewport) {
        // Zwingt den Body auf die exakt sichtbaren Pixel (abzüglich Tastatur)
        document.body.style.height = window.visualViewport.height + 'px';
        
        // Blockiert das heimliche Hochschieben von Android
        window.scrollTo(0, 0); 
    }
}

if (window.visualViewport) {
    // Reagiert in Echtzeit auf das Öffnen/Schließen der Tastatur
    window.visualViewport.addEventListener('resize', applyVisualViewport);
    
    // Falls Firefox trotzdem versucht zu scrollen, nageln wir es fest
    window.visualViewport.addEventListener('scroll', () => window.scrollTo(0,0));
    
    applyVisualViewport();
} else {
    // Fallback für sehr alte Browser
    document.body.style.height = '100dvh';
}
// ------------------------------------------

const emojis = [
        // Gesichter & Smileys
        '😊','😂','🤣','😍','😎','🤔','😅','😉','🙄','🤨','😏','🥳', '🤮', '😡', '😭','😤','😱','🥱','😴', '😜', '☠️', '🙈', '🤷‍♂️', '💩',
        // Handzeichen & Menschen
        '👍','👎','👌','✌️','🤞','🤙','👏','🙌','🙏','🤦', '💪','👋','🖐️','🤳','👈','👉',
        // Herzen & Symbole
        '❤️','✨','🔥','💥','💯','💢', '☀️', 
        // Technik & LoRa/Mesh
        '📡','📶','📻','💻','🔋','🔌','💡','📟','🛡️','🌍','🛰️','⚡','⚙️','🔧',
        // Status & Warnung
        '✅','❌','⚠️','🚫','🔔','🔕','🆘','🛑','🟢','🟡','🔴','💬','🗨️', '❤️', '✉️'
    ];

var guiSettings;
var wakeLock = null;
var focus = true;
var settings = settings || { name: "rMesh", mycall: "" };

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



document.addEventListener("visibilitychange", function () {
    if (document.visibilityState === "visible") {
        focus = true;
        showMessages(true);
    } else {
        focus = false;
    }
});


document.getElementById("settingsScanWifi").addEventListener("click", function() {
    showModal("Note", "WiFi scan started. Results will be displayed in a few seconds.", "", false);
    document.getElementById('settingsSSIDList').innerHTML = "";
    sendWS(JSON.stringify({scanWifi: true }));
}); 

document.getElementById("settingsSSIDList").addEventListener("click", function() {
    document.getElementById("settingsSSID").value = document.getElementById("settingsSSIDList").value;
});


function buildMenu() {

    //Menüstruktur dynamisch bauen
    const menuItems = [
        { type: 'spacer' },
        { type: 'header', label: 'Groups' },
    ];

    //Gruppe alle
    menuItems.push(
        { 
            label: "all",
            mute: guiSettings.muteAll,
            action: () => {
                showContent("group_all", "all");
                setupSendMessage('', true);   
                document.getElementById("group_all").scrollTo({top: document.getElementById("group_all").scrollHeight });
            },
            longPressAction: () => {
                //Gruppe löschen
                    showSelectionModal("Group Actions", "",   ["mute", "unmute"]).then(function(choice) {
                    if (choice === "mute") {
                        guiSettings.muteAll = true;
                        buildMenu();                         
                    }                       
                    if (choice === "unmute") {
                        guiSettings.muteAll = false;
                        buildMenu();                         
                    }                       
                });
            }
        }            
    );    
    createMainSection("group_all");

    //Gruppen hinzufügen
    for (var key in guiSettings.groups) { 
        const groupName  = guiSettings.groups[key].name; 
        //Main Section erzeugen
        createMainSection("group_" + groupName);
        //Menüeinträge hinzu
        menuItems.push(
            { 
                label: groupName,
                mute:  guiSettings.groups[key].mute,
                action: () => {
                    showContent("group_" + groupName, groupName);
                    setupSendMessage(groupName, true);   
                    document.getElementById("group_" + groupName).scrollTo({top: document.getElementById("group_" + groupName).scrollHeight });
                },
                longPressAction: () => {
                    //Gruppe löschen
                     showSelectionModal("Group Actions", "",   ["delete", "mute", "unmute"]).then(function(choice) {
                        if (choice === "delete") {
                            var newGroups = []; 
                            for (var i = 0; i < guiSettings.groups.length; i++) { 
                                if (guiSettings.groups[i].name !== groupName) { newGroups.push(guiSettings.groups[i]); } 
                            } 
                            guiSettings.groups = newGroups;
                            showMessages(true);
                            showContent("group_all", "all");
                        }  
                        if (choice === "mute") {
                            for (var i = 0; i < guiSettings.groups.length; i++) { 
                                if (guiSettings.groups[i].name == groupName) { guiSettings.groups[i].mute = true; } 
                            }                             
                            buildMenu();                         
                        }                       
                        if (choice === "unmute") {
                            for (var i = 0; i < guiSettings.groups.length; i++) { 
                                if (guiSettings.groups[i].name == groupName) { guiSettings.groups[i].mute = false; } 
                            }    
                            buildMenu();                         
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
                var name = await showModal("Add new group", "Name:", "", true);
                if (name) {
                    name = name.trim();
                    const newGroup = {
                        name: name, 
                        read: true};
                    guiSettings.groups.push(newGroup);
                    showMessages(true);
                    showContent("group_" + name, name);
                    setupSendMessage(name, true);  

                }
            }    
        },    
        { type: 'spacer' },
        { type: 'header', label: 'Direct Messages' }]); 

    //DM hinzufügen
    for (var key in guiSettings.dm) { 
        const callsign = guiSettings.dm[key].name; 
        //Main Section erzeugen
        createMainSection("dm_" + callsign);        
        //Menüeinträge hinzu
        menuItems.push(
            { 
                label: callsign , 
                action: () => {
                    showContent("dm_" + callsign, callsign );
                    setupSendMessage(callsign, false);  
                    document.getElementById("dm_" + callsign).scrollTo({top: document.getElementById("dm_" + callsign).scrollHeight });
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
                    name = name.trim();
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
                        showContent("dm_" + name, name);
                        setupSendMessage(name, false); 
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
                showContent('cMonitor', "Monitor"); 
                window.scrollTo({ 
                    top: document.body.scrollHeight, 
                    //behavior: 'smooth' 
                });
            }
        },
        { 
            label: 'Peers', 
            action: () => showContent('cPeers', "Peers") 
        },
        { 
            label: 'Routing', 
            action: () => showContent('cRouting', "Routing") 
        },
        { type: 'spacer' },
        { type: 'header', label: 'Settings' },
        { 
            label: 'Network', 
            action: () => showContent('cNetwork', "Network") 
        },
        { 
            label: 'LoRa', 
            action: () => showContent('cLora', "LoRa") 
        },
        { 
            label: 'About', 
            action: () => showContent('cAbout', "About") 
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
            if (item.mute == true) { li.textContent += " 🔕";}
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
                }
                toggleMenu();
            };
        }

        menuList.appendChild(li);

    });
    saveGuiSettings();
    
}


function createMainSection(sectionId) {
    // 1. Sicherheits-Check: Gibt es die Sektion vielleicht schon?
    if (document.getElementById(sectionId)) {
         return; // Abbruch, wir müssen nichts doppelt bauen!
    }
    const newMain = document.createElement('main');
    newMain.id = sectionId;
    newMain.innerHTML = "****" + sectionId + "****";
    newMain.className = 'content-section'; // WICHTIG: Damit unser CSS und showContent() es finden!
    document.body.appendChild(newMain);
}

function saveGuiSettings() {
    localStorage.setItem("guiSettings", JSON.stringify(guiSettings));
}


function loadGuiSettings() {
    const savedData = localStorage.getItem("guiSettings");

    if (savedData) {
        try {
            guiSettings = JSON.parse(savedData);
            guiSettings.dm = [];
            return;
        } catch (e) {
            console.error("Fehler beim Laden der Settings aus dem localStorage", e);
        }
    }
    // Fallback: Deine gewünschten Standardwerte, falls noch nie etwas gespeichert wurde
    guiSettings = { 
        groups: [ 
            { name: "Herzog", read: false }, 
            { name: "Wetter", read: false }, 
            { name: "Verkehr", read: false }
        ], 
        dm: [], 
        menu: "cMonitor", 
        update: 0, 
        title: "Monitor",
        sendBar: {active: false, dst: null, group: null},
        content: {content: "cLora", title: "LoRa"}
    };
}




function toggleMenu() {
	const menu = document.getElementById('side-menu'); 
	menu.classList.toggle('open');
}

function showContent(sectionId, title = "") {
    //Alle content-sections suchen und unsichtbar machen
    const alleSektionen = document.querySelectorAll('.content-section');
    alleSektionen.forEach(sektion => {
        sektion.classList.remove('active'); // Nimmt die Klasse weg -> display: none greift wieder
    });    

    //Footer weg
    document.getElementById("dynamic-footer").style.display = "none";
    guiSettings.sendBar = {active: false, dst: null, group: null};
    saveGuiSettings();


    //Titel
    const currentName = (settings && settings.name) ? settings.name : "rMesh";
    const titleElem = document.getElementById("title");
    if (titleElem) {
        titleElem.innerHTML = title ? currentName + " - " + title : currentName;
    }    

    //Nur den gewünschten Block suchen und sichtbar machen
    const zielSektion = document.getElementById(sectionId);
    if (zielSektion) {
        zielSektion.classList.add('active'); // Fügt die Klasse hinzu -> display: block greift
    } 

    showMessages(true);
    window.scrollTo(0, 0); 
    
    //UI-Zustand speichern (Safari-sicher)
    if (guiSettings) {
        guiSettings.content = {content: sectionId, title: title};
        saveGuiSettings();
    }
}


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
        //behavior: 'smooth'
    });

}



function setupSendMessage(dst, group = true) {
    const bar = document.getElementById("dynamic-footer");
    bar.innerHTML = "";
    bar.style.display = 'flex';
 
    const emojiBtn = document.createElement('button');
    emojiBtn.className = 'emoji-btn';
    emojiBtn.innerHTML = '😊';
    emojiBtn.type = 'button';

    const picker = document.createElement('div');
    picker.className = 'emoji-picker';
    picker.style.display = 'none';
    
    // Maximale Länge aus den Einstellungen holen (mit Fallback auf 200, falls d.settings fehlt)
    const maxLength = (settings && settings.loraMaxMessageLength) ? settings.loraMaxMessageLength : 200;

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

            //Nachricht vorbereiten und über Websocket senden
            var message = {};
            message["text"] = text;
            message["dst"] = dst;
            if (group == true) {
                sendWS(JSON.stringify({sendGroup: message}));  
            } else {
                sendWS(JSON.stringify({sendMessage: message}));  
            }

            input.value = "";
            picker.style.display = 'none';
        }
    };

    const triggerSend = (e) => {
        e.preventDefault(); // Verhindert doppeltes Auslösen
        handleSend();
    };    

    const handleSafariSend = (e) => {
    e.preventDefault();
    handleSend();
    };

    button.addEventListener('pointerdown', handleSafariSend);
    button.addEventListener('touchend', triggerSend);
    button.addEventListener('click', handleSend);    

    button.onclick = handleSend;
    input.onkeydown = (e) => { if (e.key === 'Enter') handleSend(); };

    bar.appendChild(emojiBtn);
    bar.appendChild(picker);
    bar.appendChild(input);
    bar.appendChild(button);

    //Speichern
    guiSettings.sendBar = {active: true, dst: dst, group: group};
    saveGuiSettings();

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
    // WICHTIG: Nutze "let", um Safari-Crashes zu vermeiden!
    const isApMode = document.getElementById("settingsApMode")?.checked;
    const isDhcp = document.getElementById("settingsDHCP")?.checked;

    for (let e of document.getElementsByClassName('AP_MODE_ENABLED')) {
        e.style.display = isApMode ? "none" : "";
    }
    
    for (let e of document.getElementsByClassName('DHCP_ENABLED')) {
        e.style.display = (isApMode || isDhcp) ? "none" : "";
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


