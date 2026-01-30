<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <title>rMesh Installer</title>

    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>

    <style>
        body {
            font-family: Arial, sans-serif;
            background: #f4f6f8;
            margin: 0;
            padding: 20px;
        }

        h2 {
            text-align: center;
            margin-bottom: 30px;
        }

        .device-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 20px;
            padding: 10px;
        }

        .device-card {
            background: white;
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.1);
            text-align: center;
        }

        .device-card img {
            width: 250px;
            height: 180px;
            object-fit: contain;
            border-radius: 8px;
            background: #fafafa;
            border: 1px solid #ddd;
            margin-bottom: 15px;
        }

        .device-name {
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 12px;
        }

        button[slot="activate"] {
            background: #0078ff;
            color: white;
            border: none;
            padding: 10px 18px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 15px;
        }

        button[slot="activate"]:hover {
            background: #005fcc;
        }
    </style>
</head>
<body>

<h2>rMesh Installer V1.0.2-a</h2>

<div class="device-grid">

<?php
$baseDir = __DIR__;
$devices = array_filter(glob($baseDir . '/*'), 'is_dir');

foreach ($devices as $devicePath) {
    $device = basename($devicePath);

    if (!file_exists("$devicePath/firmware.bin") && !file_exists("$devicePath/littlefs.bin")) {
        continue;
    }

    $imagePath = "$devicePath/image.jpg";
    $imageUrl = file_exists($imagePath) ? "$device/image.jpg" : "https://via.placeholder.com/250x180?text=No+Image";

    echo "<div class='device-card'>";
    echo "<img src='$imageUrl' alt='Board Image'>";
    echo "<div class='device-name'>$device</div>";

    echo "
        <esp-web-install-button manifest=\"$device/manifest.php\">
            <button slot='activate'>Firmware installieren</button>
            <span slot='unsupported'>Browser nicht unterstützt.</span>
        </esp-web-install-button>
    ";

    echo "</div>";
}
?>

</div>

</body>
</html>
