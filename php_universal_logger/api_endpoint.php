<?php
/**
 * Universal Data Logger API
 * Single endpoint for all sensor data (caminetto, meteo, energy, etc.)
 */
define('PROJECT_ROOT', '/membri/developteamgold/');
require_once(PROJECT_ROOT . 'config.php');
// 1. Configuration & Dependencies
require_once(__DIR__ . '/lib/ProtocolParser.php');
//require_once(__DIR__ . '/../php_cloud/config.php'); // Orchestrator

// Settings
$configPath = __DIR__ . '/config/protocol.json';
$logFile    = PROJECT_ROOT . '/log/buffer_access.log';

// Ensure log directory exists
if (!is_dir(PROJECT_ROOT . '/log')) respondWithJson(["error" => "Log Dir not found"], 404);

// 2. Initialize Parser
try {
    $parser = new ProtocolParser($configPath);
} catch (Exception $e) {
    respondWithJson(["error" => "Configuration Error: " . $e->getMessage()], 500);
}

// 3. Receive Data
$method = $_SERVER['REQUEST_METHOD'];
if ($method !== 'POST' && $method !== 'PUT') {
    respondWithJson(["error" => "Method Not Allowed"], 405);
}

$rawInput = file_get_contents('php://input');

// Log Request
file_put_contents($logFile, date('[Y-m-d H:i:s] ') . "Request from {$_SERVER['REMOTE_ADDR']} | Method: $method | Len: " . strlen($rawInput) . "\n", FILE_APPEND);

if (empty($rawInput)) {
    respondWithJson(["error" => "Empty Body"], 400);
}

// 4. Decode Data
try {
    $decoded = $parser->decode($rawInput);
} catch (Exception $e) {
    // If decoding fails, maybe it's raw JSON from an old device?
    $decoded = json_decode($rawInput, true);
    if (json_last_error() !== JSON_ERROR_NONE) {
        respondWithJson(["error" => "Decoding Failed: " . $e->getMessage()], 400);
    }
}

// 5. Logical Switch Case
$type = $decoded['type'] ?? 'UNKNOWN';
$data = $decoded['data'] ?? $decoded; // Fallback for JSON
$deviceId = $data['deviceId'] ?? 0;
$success = false;

switch ($type) {
    case 'CAMINETTO':
        // NOTE: Use only columns verified in your DB (T_DS, T_K, PID)
        $sql = "INSERT INTO `CAMI`(`T_DS`, `T_K`, `PID`) VALUES (:t_ds, :t_k, :pid)";
        $params = [
            ':t_ds'    => $data['tempAmb'] ?? 0,
            ':t_k'     => $data['tempK'] ?? 0,
            ':pid'     => $data['fanPid'] ?? 0
            // Add extra columns here if you update your DB later:
            // ':t_acqua' => $data['tempAcqua'] ?? 0,
            // ':press'   => $data['pressione'] ?? 0,
            // ':curr'    => $data['corrente'] ?? 0
        ];
        $success = makeSQL($sql, $params, 10);
        break;

    case 'METEO':
    case 'BME280':
        // Le sonde esterne sono gestite altrove (Node-RED/altri script)
        $success = true;
        break;

    case 'DHT':
        // Filtriamo solo i sensori interni che ti interessano
        // 16 (0x10) = Salotto, 32 (0x20) = Camera Grande, 48 (0x30) = Bagno
        $allowedDevices = [16, 32, 48];
        
        if (in_array($deviceId, $allowedDevices)) {
            // NOTA: Crea la tabella SENSORI_INTERNI_TEMP nel DB per usare questa logica
            $sql = "INSERT INTO `SENSORI_INTERNI_TEMP` (`deviceId`, `temp`, `hum`, `comfort`, `timestamp`) 
                    VALUES (:devid, :temp, :hum, :comfort, :ts)";
            $params = [
                ':devid'   => $deviceId,
                ':temp'    => round($data['temperature'] ?? 0, 2),
                ':hum'     => round($data['humidity'] ?? 0, 2),
                ':comfort' => $data['comfort'] ?? 0,
                ':ts'      => date('Y-m-d H:i:s')
            ];
            $success = makeSQL($sql, $params, 4); // Topic 4 per DHT
        } else {
            $success = true;
        }
        break;

    case 'PZEM':
        $sql = "INSERT INTO `ENERGIA` (`VOLT`, `AMP`, `COSPI`, `POWER`, `TIMESTAMP_LOCAL`) 
                VALUES (:volt, :amp, :cospi, :pow, :ts)";
        $params = [
            ':volt'  => round($data['voltage'] ?? 0, 2),
            ':amp'   => round($data['current'] ?? 0, 2),
            ':cospi' => round($data['cosphi'] ?? 0, 2),
            ':pow'   => round($data['power'] ?? 0, 2),
            ':ts'    => date('Y-m-d H:i:s')
        ];
        $success = makeSQL($sql, $params, 6);
        break;

    case 'BOILER':
        $sql = "INSERT INTO `CALDAIA`(`TEMP`, `POWER`) VALUES (:temp, :power)";
        $params = [
            ':temp'  => $data['temperature'] ?? 0,
            ':power' => $data['valvePos'] ?? 0
        ];
        $success = makeSQL($sql, $params, 9);
        break;

    case 'TIME':
    case 'ACK':
    case 'ANNOUNCE':
        // Pacchetti di sistema: validi ma non richiedono inserimento su DB qui
        $success = true;
        break;

    case 'TENDE':
        // Placeholder per le tende
        $success = true;
        break;

    default:
        // Pacchetto decodificato ma non gestito nello switch
        error_log("Packet type received but not mapped to DB: " . $type);
        $success = true; // Rispondiamo comunque successo se la decodifica è andata a buon fine
        break;
}

// 6. Final Response
respondWithJson([
    "status"      => $success ? "success" : "error",
    "type"        => $type,
    "deviceId"    => $deviceId,
    "received_at" => date('Y-m-d H:i:s')
], $success ? 200 : 500);

