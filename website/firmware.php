<?php
// Validates params, then streams the requested release asset from GitHub.
// All binary traffic goes through this server so the browser gets proper CORS headers.

$tag  = $_GET['tag']  ?? '';
$file = $_GET['file'] ?? '';

// Strict allowlist: tag = vX.Y.Z or VX.Y.Z, file = DEVICE_something.bin
if (!preg_match('/^[Vv]\d+\.\d+\.\d+$/', $tag) ||
    !preg_match('/^[a-zA-Z0-9_\-]+\.bin$/', $file)) {
    http_response_code(400);
    exit('Invalid parameters');
}

$url = "https://github.com/DN9KGB/rMesh/releases/download/$tag/$file";

$ctx = stream_context_create(['http' => [
    'header'          => "User-Agent: rMesh-Website\r\n",
    'timeout'         => 60,
    'follow_location' => 1,
]]);

$fh = @fopen($url, 'rb', false, $ctx);
if (!$fh) {
    http_response_code(502);
    exit('Could not fetch firmware from GitHub');
}

// Check HTTP status from the response metadata
$meta   = stream_get_meta_data($fh);
$status = 200;
foreach ($meta['wrapper_data'] as $line) {
    if (preg_match('#^HTTP/\S+ (\d+)#', $line, $m)) {
        $status = (int)$m[1];
    }
}
if ($status >= 400) {
    fclose($fh);
    http_response_code($status);
    exit("GitHub returned HTTP $status");
}

header('Content-Type: application/octet-stream');
header('Cache-Control: public, max-age=86400'); // cache for 1 day (releases are immutable)

fpassthru($fh);
fclose($fh);
