<?php
$file   = $_GET['file']   ?? '';
$call   = isset($_GET['call'])   ? strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($_GET['call'])), 0, 16)) : '';
$device = isset($_GET['device']) ? substr($_GET['device'], 0, 64) : '';

if (!preg_match('/^[a-zA-Z0-9_\-\.]+$/', $file)) {
    http_response_code(400);
    exit;
}

// Redirect sofort senden – kein Blocking durch DB
header('Location: https://github.com/DN9KGB/rMesh/releases/latest/download/' . $file, true, 302);
header('Content-Length: 0');
header('Connection: close');

if (function_exists('fastcgi_finish_request')) {
    fastcgi_finish_request();
} else {
    if (ob_get_level()) ob_end_flush();
    flush();
}

// DB-Logging nach Response
require_once __DIR__ . '/ota_log_helper.php';
logOtaEvent($call, $device, 'update_start', '', '', $file);
