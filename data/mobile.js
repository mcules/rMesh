const menuItems = [
    { type: 'spacer' },
    { type: 'header', label: 'Groups' },
    { 
        label: 'all', 
        action: () => showContent('home') ,
        longPressAction: () => {
            alert('Gruppe bearbeiten oder löschen?');
        }     
    },
    { 
        label: '-- new group --', 
        action: async () => {
            const name = await showModal("Neue Gruppe", "Name:", "", true);
            if(name) console.log("neue Gruppe:", name);
        }    
    },    
    { type: 'spacer' },
    { type: 'header', label: 'Direct Messages' },
    { 
        label: 'all', 
        action: () => showContent('home') ,
        longPressAction: () => {
            alert('Gruppe bearbeiten oder löschen?');
        }     
    },
    { 
        label: '-- new contact --', 
        action: async () => {
            const name = await showModal("Neue Gruppe", "Name:", "", true);
            if(name) console.log("neue Gruppe:", name);
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
    },



    { type: 'spacer' },
    { type: 'spacer' },
    { type: 'spacer' },
    { type: 'spacer' },    
    { 
        label: 'Startseite', 
        action: () => showContent('home') 
    },
    { type: 'header', label: 'Gruppen' },
    { 
        label: 'Hilfe', 
        action: () => showContent('hilfe') 
    },
    { type: 'header', label: 'Direktnachrichten' },
    { 
        label: 'Profil bearbeiten', 
        action: async () => {
            const name = await showModal("Profil", "Name ändern:", "Max", true);
            if(name) console.log("Name geändert zu:", name);
        }
    },

    { 
        label: 'Info Box', 
        action: () => showModal("System", "Version 1.0.4", "", false) 
    },
    { 
        label: 'Einstellungen', 
        action: () => showContent('einstellungen') 
    },
    { type: 'header', label: 'Funktionen' },
    { type: 'spacer' }
];


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
// Zähler, um die nächste Farbe aus der Liste zu wählen
let colorIndex = 0;





window.addEventListener('DOMContentLoaded', function() {
	
    console.log("Hallo");
    buildMenu();

    addBubble(
        "right", 
        "Michael Multerer", 
        "10:25", 
        getColorForName("Ralf"), 
        "Ein Bekannter will seinen IC-9700 verkaufen. Gepflegter Zustand...", 
        "home"
    );


    addBubble(
        "right", 
        "Michael Multerer", 
        "10:25", 
        getColorForName("Ralf"), 
        "Ein Bekannter will seinen IC-9700 verkaufen. Gepflegter Zustand... sdfjkgh slkfjg slkfjgh slkfgh slkfjg hsldkfgj hsdlfkj gshldghk ", 
        "home"
    );

    addBubble(
        "left", 
        "Michael Multerer", 
        "10:25", 
        getColorForName("Jochen"), 
        "Ein Bekannter will seinen IC-9700 verkaufen. Gepflegter Zustand...", 
        "home"
    );

    addBubble(
        "system", 
        "DB0LUS > DB0REN", 
        "10:25", 
        "#5972ffff", 
        "sdfsd\nlksdjflskdjf", 
        "home"
    );    


	// Beispiel: Namen ändern (Input-Modus)
	async function changeName() {
		const name = await showModal("Namensänderung", "Wie lautet dein neuer Name?", "Max Mustermann", true);
		
		if (name !== null) {
			console.log("Der User hat eingegeben: " + name);
			alert("Hallo " + name);
		}
	}

	// Beispiel: Einfache Nachricht (Message-Modus)
	async function infoMessage() {
		await showModal("Hinweis", "Deine Einstellungen wurden gespeichert!", "", false);
		console.log("User hat die Nachricht bestätigt.");
	}


    setupInputBar('home', mySendMessageFunction);

        // // Aufruf der Auswahl-Funktion
        // showSelectionModal("Gruppen-Optionen", "Bitte wähle eine Aktion aus:",   ["Bearbeiten", "Stummschalten", "Löschen"]).then(function(choice) {
        //     // Diese Funktion wird erst ausgeführt, wenn der User geklickt hat
        //     if (choice === null) {
        //         console.log("Der User hat das Modal abgebrochen.");
        //     } else {
        //         console.log("Der User hat gewählt: " + choice);
                
        //         // Hier kannst du mit einer if-Abfrage oder switch weiterarbeiten
        //         if (choice === "Löschen") {
        //             // Weitere Logik hier...
        //         }
        //     }
        // });

        showContent('cMonitor');

        initWebSocket();

        //setAntennaColor("rgb(255, 217, 0)");

	
	
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
	document.getElementById(sectionId).classList.add('active');
    window.scrollTo(0, 0); 

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

    // FIX: Ganz kurz warten (0ms reicht oft), damit der Browser 
    // die neue Höhe des Containers berechnen kann.
    setTimeout(function() {
        container.scrollTo({
            top: container.scrollHeight,
            behavior: 'smooth'
        });
    }, 10); 
}



/**
 * Fügt eine Eingabeleiste in eine Sektion ein.
 * Verhindert Duplikate innerhalb derselben Sektion.
 * @param {string} sectionId - Die ID der Sektion (z.B. 'home')
 * @param {function} onSendCallback - Die Funktion, die beim Senden aufgerufen wird
 */
function setupInputBar(sectionId, onSendCallback) {
    const section = document.getElementById(sectionId);
    if (!section) return;

    section.classList.add('has-input');
    // 1. Bestehende Leiste in dieser Sektion entfernen (verhindert Duplikate)
    const existingBar = section.querySelector('.input-bar');
    if (existingBar) {
        existingBar.remove();
    }

    // 2. Neue Leiste erstellen
    const bar = document.createElement('div');
    bar.className = 'input-bar';

    const input = document.createElement('input');
    input.type = 'text';
    input.placeholder = 'type a message....';
    
    const button = document.createElement('button');
    button.className = 'send-btn';
    button.innerHTML = '➤'; // Ein einfacher Pfeil als Icon

    // 3. Sende-Logik
    const handleSend = () => {
        const text = input.value.trim();
        if (text !== "") {
            onSendCallback(sectionId, text); // Übergibt ID und Text
            input.value = ""; // Feld leeren
        }
    };

    button.onclick = handleSend;

    // Enter-Taste zum Senden unterstützen
    input.onkeydown = (e) => {
        if (e.key === 'Enter') handleSend();
    };

    // Zusammenbauen
    bar.appendChild(input);
    bar.appendChild(button);
    section.appendChild(bar);
}

// 4. Die Funktion, die beim Senden aufgerufen wird (Beispiel)
function mySendMessageFunction(id, text) {
    console.log("Gesendet von " + id + ": " + text);
    
    // Hier rufen wir deine addBubble Funktion auf:
    addBubble("right", "Ich", "jetzt", "#00f2ff", text, id);
}

/**
 * Gibt eine feste Farbe für einen Namen zurück.
 * @param {string} name - Der Name des Teilnehmers
 * @returns {string} - Die HEX-Farbe
 */
function getColorForName(name) {
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



/**
 * Zeigt ein Modal mit einer Liste von Auswahlmöglichkeiten.
 * @param {string} title - Überschrift
 * @param {string} desc - Beschreibungstext
 * @param {Array<string>} options - Liste der Texte für die Buttons
 * @returns {Promise<string|null>} - Der gewählte Text oder null bei Abbruch
 */
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


/**
 * Setzt die Farbe des Antennensymbols direkt via Style
 * @param {string} color - 'gray', 'green', 'red' oder Hex-Codes
 */
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