<?php
define('PROJECT_ROOT', '/membri/developteamgold/');
require_once(PROJECT_ROOT . 'config.php');
$DataIn = json_decode(file_get_contents('php://input'), true);
$temp = $DataIn['temp'];
$gaspower = $DataIn['gaspower'];

try {
  // Parametri della query
  $sql = "INSERT INTO `CALDAIA`( `TEMP`, `POWER`) VALUES (:temp, :power);";
  $params = [
      ':temp' => $temp,
      ':power' => $gaspower
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
