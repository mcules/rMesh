// i18n - DE/EN translation module
const I18N = {
    en: {
        // Navigation
        'nav.monitor': 'Monitor', 'nav.peers': 'Peers', 'nav.routing': 'Routing',
        'nav.dos': 'DOS', 'nav.tune': 'Tune', 'nav.announce': 'Announce',
        'nav.lora': 'LoRa', 'nav.setup': 'Setup', 'nav.network': 'Network', 'nav.about': 'About',
        'nav.section_nav': 'Navigation', 'nav.section_settings': 'Settings',
        'about.version': 'Version:', 'about.changelog': 'Changelog',
        'lora.section_freq': 'Frequency & Power', 'lora.section_mod': 'Modulation', 'lora.section_msg': 'Messages',
        // Status
        'status.dstcall': 'Dst:', 'status.mycall': 'My Call:', 'status.heap': 'Heap:',
        'status.battery': 'Bat:', 'status.trx': 'TRX:', 'status.txbuf': 'TX-Buf:', 'status.retry': 'Retry:',
        // LoRa
        'lora.preset': 'Preset:', 'lora.preset_choose': 'Choose Preset', 'lora.section_preset': 'Preset',
        'lora.frequency': 'Frequency:', 'lora.power': 'Output Power:',
        'lora.bandwidth': 'Bandwidth:', 'lora.coding_rate': 'Coding Rate:',
        'lora.sf': 'Spreading Factor:', 'lora.sync_word': 'Sync Word:',
        'lora.preamble': 'Preamble Length:', 'lora.repeat': 'Repeat Messages:',
        'lora.enabled': 'HF active:', 'lora.max_len': 'Max Message Length:',
        'lora.cr5': '5 - Standard', 'lora.cr6': '6 - Improved robustness',
        'lora.cr7': '7 - High reliability', 'lora.cr8': '8 - Maximum redundancy',
        'lora.sf6': '6 - Short range (LOS)', 'lora.sf7': '7 - Good (Standard)',
        'lora.sf8': '8 - Better', 'lora.sf9': '9 - Very good',
        'lora.sf10': '10 - Excellent', 'lora.sf11': '11 - Maximum',
        'lora.sf12': '12 - Maximum (Deep Indoor)',
        // Setup sections
        'setup.general': 'General', 'setup.callsign': 'Callsign:',
        'setup.position': 'Position:', 'setup.chip_id': 'Chip ID:',
        'setup.hardware': 'Hardware:',
        'setup.section_update': 'Online Update', 'setup.update_channel': 'Update Channel:',
        'setup.section_firmware': 'Firmware Upload', 'setup.firmware': 'Firmware:',
        'setup.filesystem': 'Filesystem:',
        'setup.section_security': 'Security', 'setup.pw_status': 'Status:',
        'setup.pw_new': 'New Password:', 'setup.pw_confirm': 'Confirm:',
        'setup.section_battery': 'Battery', 'setup.bat_show': 'Show battery level:',
        'setup.bat_full': 'Voltage at 100%:',
        'setup.section_system': 'System',
        'setup.language': 'Language:',
        // Network
        'net.ap': 'Access Point:', 'net.networks': 'WiFi Networks:',
        'net.ssid': 'SSID:', 'net.password': 'Password:',
        'net.dhcp': 'DHCP:', 'net.ip': 'IP Address:',
        'net.subnet': 'Subnet:', 'net.gateway': 'Gateway:',
        'net.dns': 'DNS:', 'net.udp_peers': 'UDP Peers:',
        'net.ntp': 'NTP Server:',
        // Buttons
        'btn.save': 'save', 'btn.reboot': 'reboot', 'btn.shutdown': 'shutdown',
        'btn.delete_messages': 'delete messages', 'btn.sync_time': 'sync browser time',
        'btn.scan': 'scan', 'btn.check_install': 'check & install',
        'btn.force_install': 'force install', 'btn.upload_flash': 'upload & flash',
        'btn.remove_password': 'delete password',
        'btn.send_announce': 'send announce', 'btn.send_tuning': 'send tuning',
        'btn.add_peer': '+ Peer', 'btn.remove_peer': 'Remove',
        'udp.legacy': 'Legacy', 'udp.active': 'Active',
        // Auth
        'auth.password_required': 'Password required', 'auth.login': 'Login',
        // Mobile
        'mob.message_placeholder': 'Message...',
        // Setup password status
        'pw.set': 'Password is set', 'pw.not_set': 'No password set',
        // Peer / Routing table headers
        'peer.port': 'Port', 'peer.call': 'Call', 'peer.last_rx': 'Last RX',
        'peer.rssi': 'RSSI', 'peer.snr': 'SNR', 'peer.frq_err': 'Frq. Error',
        'route.call': 'Call', 'route.node': 'Node', 'route.hops': 'HopCount', 'route.last_rx': 'Last RX',
        'settings.unsaved_confirm': 'There are unsaved changes. Discard and continue?',
    },
    de: {
        'nav.monitor': 'Monitor', 'nav.peers': 'Peers', 'nav.routing': 'Routing',
        'nav.dos': 'DOS', 'nav.tune': 'Tune', 'nav.announce': 'Announce',
        'nav.lora': 'LoRa', 'nav.setup': 'Setup', 'nav.network': 'Netzwerk', 'nav.about': 'Über',
        'nav.section_nav': 'Navigation', 'nav.section_settings': 'Einstellungen',
        'about.version': 'Version:', 'about.changelog': 'Changelog',
        'lora.section_freq': 'Frequenz & Leistung', 'lora.section_mod': 'Modulation', 'lora.section_msg': 'Nachrichten',
        'status.dstcall': 'Ziel:', 'status.mycall': 'Mein Rufzeichen:', 'status.heap': 'Heap:',
        'status.battery': 'Akku:', 'status.trx': 'TRX:', 'status.txbuf': 'TX-Puf:', 'status.retry': 'Retry:',
        'lora.preset': 'Preset:', 'lora.preset_choose': 'Preset wählen', 'lora.section_preset': 'Preset',
        'lora.frequency': 'Frequenz:', 'lora.power': 'Sendeleistung:',
        'lora.bandwidth': 'Bandbreite:', 'lora.coding_rate': 'Coding Rate:',
        'lora.sf': 'Spreading Factor:', 'lora.sync_word': 'Sync Word:',
        'lora.preamble': 'Preamble Länge:', 'lora.repeat': 'Nachrichten wiederholen:',
        'lora.enabled': 'HF aktiv:', 'lora.max_len': 'Max. Nachrichtenlänge:',
        'lora.cr5': '5 - Standard', 'lora.cr6': '6 - Erhöhter Schutz gegen Störungen',
        'lora.cr7': '7 - Hoher Schutz, längere Sendezeit', 'lora.cr8': '8 - Maximaler Schutz',
        'lora.sf6': '6 - Gering (Sichtverbindung)', 'lora.sf7': '7 - Gut (Standard)',
        'lora.sf8': '8 - Besser', 'lora.sf9': '9 - Sehr gut',
        'lora.sf10': '10 - Exzellent', 'lora.sf11': '11 - Maximum',
        'lora.sf12': '12 - Maximum (Deep Indoor)',
        'setup.general': 'Allgemein', 'setup.callsign': 'Rufzeichen:',
        'setup.position': 'Position:', 'setup.chip_id': 'Chip ID:',
        'setup.hardware': 'Hardware:',
        'setup.section_update': 'Online Update', 'setup.update_channel': 'Update-Kanal:',
        'setup.section_firmware': 'Firmware Upload', 'setup.firmware': 'Firmware:',
        'setup.filesystem': 'Dateisystem:',
        'setup.section_security': 'Sicherheit', 'setup.pw_status': 'Status:',
        'setup.pw_new': 'Neues Passwort:', 'setup.pw_confirm': 'Bestätigung:',
        'setup.section_battery': 'Akku', 'setup.bat_show': 'Akkustand anzeigen:',
        'setup.bat_full': 'Spannung bei 100%:',
        'setup.section_system': 'System',
        'setup.language': 'Sprache:',
        'net.ap': 'Accesspoint:', 'net.networks': 'WLAN-Netzwerke:',
        'net.ssid': 'SSID:', 'net.password': 'Passwort:',
        'net.dhcp': 'DHCP:', 'net.ip': 'IP-Adresse:',
        'net.subnet': 'Subnetz:', 'net.gateway': 'Gateway:',
        'net.dns': 'DNS:', 'net.udp_peers': 'UDP-Peers:',
        'net.ntp': 'NTP-Server:',
        'btn.save': 'Speichern', 'btn.reboot': 'Neustart', 'btn.shutdown': 'Herunterfahren',
        'btn.delete_messages': 'Nachrichten löschen', 'btn.sync_time': 'Browserzeit sync.',
        'btn.scan': 'Suchen', 'btn.check_install': 'Prüfen & installieren',
        'btn.force_install': 'Update erzwingen', 'btn.upload_flash': 'Hochladen & flashen',
        'btn.remove_password': 'Passwort löschen',
        'btn.send_announce': 'Announce senden', 'btn.send_tuning': 'Tuning senden',
        'btn.add_peer': '+ Peer', 'btn.remove_peer': 'Entfernen',
        'udp.legacy': 'Legacy', 'udp.active': 'Aktiv',
        'auth.password_required': 'Passwort erforderlich', 'auth.login': 'Anmelden',
        'mob.message_placeholder': 'Nachricht...',
        'pw.set': 'Passwort ist gesetzt', 'pw.not_set': 'Kein Passwort gesetzt',
        'peer.port': 'Port', 'peer.call': 'Call', 'peer.last_rx': 'Last RX',
        'peer.rssi': 'RSSI', 'peer.snr': 'SNR', 'peer.frq_err': 'Frq. Error',
        'route.call': 'Call', 'route.node': 'Node', 'route.hops': 'HopCount', 'route.last_rx': 'Last RX',
        'settings.unsaved_confirm': 'Es gibt ungespeicherte Änderungen. Verwerfen und fortfahren?',
    }
};

var currentLang = localStorage.getItem('rmesh_lang') || 'de';

function t(key) {
    return (I18N[currentLang] && I18N[currentLang][key])
        || (I18N['en'] && I18N['en'][key])
        || key;
}

function setLanguage(lang) {
    currentLang = lang;
    localStorage.setItem('rmesh_lang', lang);
    applyI18n();
}

function applyI18n() {
    document.querySelectorAll('[data-i18n]').forEach(function (el) {
        el.textContent = t(el.getAttribute('data-i18n'));
    });
    document.querySelectorAll('[data-i18n-ph]').forEach(function (el) {
        el.placeholder = t(el.getAttribute('data-i18n-ph'));
    });
    document.querySelectorAll('[data-i18n-title]').forEach(function (el) {
        el.title = t(el.getAttribute('data-i18n-title'));
    });
    // Update lang attribute on html element
    document.documentElement.lang = currentLang;
}