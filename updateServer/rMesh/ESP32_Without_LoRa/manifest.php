<?php

$folderName = basename(__DIR__);

$json = <<<JSON
{
  "name": "$folderName",
  "new_install_immediately_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "bootloader.bin", "offset": 4096 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin", "offset": 65536 },
        { "path": "littlefs.bin", "offset": 2686976 }
      ]
    }
  ],
  "baudrate": 921600
}
JSON;

header('Content-Type: application/json');
echo $json;

?>

