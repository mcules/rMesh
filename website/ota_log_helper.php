<?php
/**
 * rMesh OTA Log Helper
 */

function _ensureOtaLogTable(PDO $db): void {
    $db->exec("CREATE TABLE IF NOT EXISTS rmesh_ota_log (
        `id`           INT UNSIGNED  NOT NULL AUTO_INCREMENT,
        `call`         VARCHAR(16)   NOT NULL DEFAULT '',
        `device`       VARCHAR(64)   NOT NULL DEFAULT '',
        `event`        VARCHAR(32)   NOT NULL,
        `version_from` VARCHAR(32)   NOT NULL DEFAULT '',
        `version_to`   VARCHAR(32)   NOT NULL DEFAULT '',
        `error`        VARCHAR(512)  NOT NULL DEFAULT '',
        `timestamp`    INT UNSIGNED  NOT NULL,
        PRIMARY KEY (`id`),
        KEY `idx_call_ts` (`call`, `timestamp`)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    // device-Spalte nachträglich hinzufügen falls Tabelle schon ohne sie existiert
    try {
        $db->exec("ALTER TABLE rmesh_ota_log ADD COLUMN `device` VARCHAR(64) NOT NULL DEFAULT '' AFTER `call`");
    } catch (Exception $e) {
        // Spalte existiert bereits – ignorieren
    }
}

function logOtaEvent(string $call, string $device, string $event, string $versionFrom, string $versionTo, string $error): void {
    require_once __DIR__ . '/db_config.php';
    try {
        $dsn = 'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=' . DB_CHARSET . ';connect_timeout=2';
        $db  = new PDO($dsn, DB_USER, DB_PASS, array(PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION));
        _ensureOtaLogTable($db);
        $stmt = $db->prepare("INSERT INTO rmesh_ota_log
            (`call`, `device`, `event`, `version_from`, `version_to`, `error`, `timestamp`)
            VALUES (:call, :device, :event, :vfrom, :vto, :error, :ts)");
        $stmt->execute(array(
            ':call'   => $call,
            ':device' => $device,
            ':event'  => $event,
            ':vfrom'  => $versionFrom,
            ':vto'    => $versionTo,
            ':error'  => $error,
            ':ts'     => time(),
        ));
    } catch (Exception $e) {
        error_log('rMesh ota_log: ' . $e->getMessage());
    }
}
