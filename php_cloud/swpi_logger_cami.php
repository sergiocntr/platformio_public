<?php
define('PROJECT_ROOT', '/membri/developteamgold/');
require_once(PROJECT_ROOT . 'config.php');
$DataIn = json_decode(file_get_contents('php://input'), true);

$t_ds = $DataIn['T_DS'];
$t_k = $DataIn['T_K'];
$t_pid = $DataIn['T_PID'];

try {
    // Parametri della query
    $sql = "INSERT INTO `CAMI`(`T_DS`, `T_K`, `PID`) VALUES (:t_ds, :t_k, :t_pid);";
    $params = [
        ':t_ds' => $t_ds,
        ':t_k' => $t_k,
        ':t_pid' => $t_pid
    ];

    // Esecuzione della query
    $response = makeSQL($sql, $params,1408);

    // Controllo del risultato
    if ($response !== [] || $response === true) {
        respondWithJson(["message" => "Dati inseriti con successo"], 200);
    } else {
        respondWithJson(["error" => "Errore durante l'inserimento dei dati"], 500);
    }
} catch (Exception $e) {
    respondWithJson(["error" => $e->getMessage()], 500);
}
