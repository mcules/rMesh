<?php
$call    = isset($_GET['call'])    ? strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($_GET['call'])), 0, 16)) : '';
$device  = isset($_GET['device'])  ? substr($_GET['device'],  0, 64) : '';
$version = isset($_GET['version']) ? substr($_GET['version'], 0, 32) : '';

$ctx = stream_context_create(['http' => [
    'header'  => "User-Agent: rMesh-Website\r\n",
    'timeout' => 5,
]]);

$json = @file_get_contents('https://api.github.com/repos/DN9KGB/rMesh/releases/latest', false, $ctx);

if ($json) {
    $latest   = json_decode($json)->tag_name;
    $body     = json_encode(['version' => $latest]);
    $logEvent = 'version_check';
    $logError = '';
} else {
    $latest   = '';
    $body     = json_encode(['version' => 'unknown']);
    $logEvent = 'version_check_failed';
    $logError = 'GitHub API nicht erreichbar';
    http_response_code(503);
}

// Antwort sofort und vollständig senden – DB-Arbeit darf die Firmware nicht blockieren
header('Content-Type: application/json');
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Content-Length: ' . strlen($body));
header('Connection: close');
echo $body;

if (function_exists('fastcgi_finish_request')) {
    fastcgi_finish_request();
} else {
    if (ob_get_level()) ob_end_flush();
    flush();
}

// DB-Logging nach Response
require_once __DIR__ . '/ota_log_helper.php';
logOtaEvent($call, $device, $logEvent, $version, $latest, $logError);
if ($logEvent === 'version_check' && $version) {
    if ($version !== $latest) {
        logOtaEvent($call, $device, 'update_found', $version, $latest, '');
    } else {
        logOtaEvent($call, $device, 'no_update', $version, $latest, '');
    }
}
