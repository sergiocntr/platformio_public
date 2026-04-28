<?php
//Configurazione
require("config.php");

// Funzione per smussare la temperatura e l'umidità
function smoothTemperature($newReading, $prevValue, $alpha = 0.3)
{
    return $alpha * $newReading + (1 - $alpha) * $prevValue;
}

function insertData($data, $stmt, $myTime, $deviceId, $prevTemp, $prevHum, $prevPress, $fh)
{
    try {
        // Calcola i valori smussati
        $smoothedTemp = smoothTemperature($data['temperatureBMP'], $prevTemp);
        $smoothedHum = smoothTemperature($data['humidityBMP'], $prevHum);
        $smoothedPress = smoothTemperature($data['externalPressure'], $prevPress);

        // Calcolo del dew_point e humidex
        $dew_point = pow(($data['humidityBMP'] / 100), (1 / 8)) * (112 + (0.9 * $data['temperatureBMP'])) + (0.1 * $data['temperatureBMP']) - 112;
        $dewpointK = $dew_point + 273.15;
        $humidex = $data['temperatureBMP'] + 0.5555 * (6.11 * pow(exp(1), (5417.7530 * ((1 / 273.16) - (1 / $dewpointK)))) - 10);

        // Arrotonda i valori
        $dew_point = round($dew_point, 2);
        $humidex = round($humidex, 2);

        // Debug log
        fwrite($fh, "Inserting data: " . json_encode($data) . "\n");

        // Prepara e esegue la query
        $stmt->execute([
            'timestamp' => date('Y-m-d H:i:s', $myTime),
            'temp' => round($data['temperatureBMP'], 2),
            'press' => round($data['externalPressure'], 2),
            'hum' => round($data['humidityBMP'], 2),
            'dew_point' => $dew_point,
            'humidex' => $humidex,
            'voltage' => $data['voltage'],
            'deviceId' => $deviceId,
            'smoothedTemp' => $smoothedTemp,
            'smoothedHum' => $smoothedHum,
            'smoothedPress' => $smoothedPress
        ]);

        return [
            'success' => true,
            'message' => "Ok scritto record",
            'prevTemp' => $smoothedTemp,
            'prevHum' => $smoothedHum,
            'prevPress' => $smoothedPress
        ];
    } catch (PDOException $e) {
        $errorMessage = "Errore durante l'inserimento dei dati: " . $e->getMessage();
        telegram($errorMessage);
        fwrite($fh, $errorMessage . "\n");

        return [
            'success' => false,
            'message' => "Errore: " . $e->getMessage()
        ];
    }
}

$dummy = false;
if ($dummy === true) {

    $DataIn = json_decode('[{"deviceId":1,"humidityBMP":100,"temperatureBMP":10.84375,"externalPressure":1011.8125,"battery":4218,"counter":0,"checksum":112}]', true);
} else {
    // Abilita il log degli errori
    ini_set('display_errors', 1);
    error_reporting(E_ALL);

    // Log dell'input ricevuto
    $myFile = "../log/sqlLogMariner.log";
    $fh = fopen($myFile, 'a');
    fwrite($fh, "--- Nuova richiesta " . date('Y-m-d H:i:s') . " ---\n");
    fwrite($fh, "Metodo HTTP: " . $_SERVER['REQUEST_METHOD'] . "\n");

    // Leggi il contenuto raw della richiesta
    $rawInput = file_get_contents('php://input');
    fwrite($fh, "Raw input ricevuto: " . $rawInput . "\n");

    // Verifica che ci siano dati
    if (empty($rawInput)) {
        $error = "Nessun dato ricevuto";
        fwrite($fh, "Errore: " . $error . "\n");
        http_response_code(400);
        die(json_encode(['error' => $error]));
    }

    // Decodifica JSON
    $DataIn = json_decode($rawInput, true);
    $jsonError = json_last_error();
    if ($jsonError !== JSON_ERROR_NONE) {
        $error = "Errore parsing JSON: " . json_last_error_msg();
        fwrite($fh, "Errore: " . $error . "\n");
        http_response_code(400);
        die(json_encode(['error' => $error]));
    }

    // Log dei dati decodificati
    fwrite($fh, "Dati decodificati: " . print_r($DataIn, true) . "\n");
}

$num_record = count($DataIn);
$myTime = time();
$myFile = "../log/sqlLogMariner.log";
$ThisFileName = basename(__FILE__, '.php');
$fh = fopen($myFile, 'a');
$message = date('Y-m-d H:i:s') . " - Num Record: $num_record - Log from $ThisFileName";
fwrite($fh, $message . "\n");
$tableName = $dummy === true ? "METEO_DUMMY" : "METEO";
// Recupera i valori precedenti per smooth
$stmt = db()->prepare("SELECT TEMP_SMOOTH, HUM_SMOOTH, PRESS_SMOOTH FROM $tableName WHERE deviceId = :deviceId ORDER BY TIMESTAMP_LOCAL DESC LIMIT 1");
$stmt->execute(['deviceId' => $DataIn[0]['deviceId'] ?? 1]);
$previousData = $stmt->fetch(PDO::FETCH_ASSOC);

$prevTemp = $previousData['TEMP_SMOOTH'] ?? ($DataIn[0]['temperatureBMP'] ?? 0);
$prevHum = $previousData['HUM_SMOOTH'] ?? ($DataIn[0]['humidityBMP'] ?? 0);
$prevPress = $previousData['PRESS_SMOOTH'] ?? ($DataIn[0]['externalPressure'] ?? 0);

$stmt = db()->prepare("
INSERT INTO $tableName (TIMESTAMP_LOCAL, TEMP, PRESSURE, HUM, DEW_POINT, HDEX, VOLTAGE, deviceId, TEMP_SMOOTH, HUM_SMOOTH, PRESS_SMOOTH) 
VALUES (:timestamp, :temp, :press, :hum, :dew_point, :humidex, :voltage, :deviceId, :smoothedTemp, :smoothedHum, :smoothedPress)
");
$error = false;
for ($i = 0; $i < $num_record; $i++) {
    $deviceId = $DataIn[$i]['deviceId'] ?? null;

    if (!$deviceId) {
        $message = "Error: deviceId non trovato nel record $i";
        fwrite($fh, $message . "\n");
        telegram($message);
        continue; // Skip questo record e continua con il prossimo
    }

    $data = [
        'temperatureBMP' => round($DataIn[$i]['temperatureBMP'] ?? 0, 2),
        'externalPressure' => round($DataIn[$i]['externalPressure'] ?? 0, 2),  // CORRETTO
        'humidityBMP' => round($DataIn[$i]['humidityBMP'] ?? 0, 2),
        'voltage' => $DataIn[$i]['voltage'] ?? 0,  // CORRETTO
    ];

    $result = insertData($data, $stmt, $myTime, $deviceId, $prevTemp, $prevHum, $prevPress, $fh);

    if (!$result['success']) {
        fwrite($fh, "Error inserting record $i: " . $result['message'] . "\n");
        $error = true;
        continue;
    }

    fwrite($fh, "Successfully inserted record $i\n");

    // Aggiorna i valori precedenti
    $prevTemp = $result['prevTemp'];
    $prevHum = $result['prevHum'];
    $prevPress = $result['prevPress'];

    $myTime -= 877;
}
fwrite($fh, "Risposta: " . json_encode($response) . "\n");
fwrite($fh, "--- Fine richiesta ---\n\n");
fclose($fh);


if ($error === true) {
    http_response_code(400); // Client Error
    $response = [
        'status' => 'error',
        'message' => "Error inserting record"

    ];
} else {
    http_response_code(200); // Success
    $response = [
        'status' => 'success',
        'message' => 'Dati ricevuti e processati',
        'records_processed' => count($DataIn)
    ];
}

header('Content-Type: application/json');
echo json_encode($response);
