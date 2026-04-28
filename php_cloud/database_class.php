<?php
if (!defined('CONFIG_LOADED')) {
    die('Direct access not allowed');
}
/**
 * Connect to the database and returns an instance of PDO class
 * or false if the connection fails
 *
 * @return PDO
 */
if (!defined('CONFIG_LOADED')) {
    die('Direct access not allowed');
  }
  const DB_HOST = 'localhost';
  const DB_NAME = 'my_developteamgold';
  const DB_USER = 'developteamgold';
  const DB_PASSWORD = '';
function db(): PDO
{
  static $pdo;
  if (!$pdo) {
    $pdo = new PDO(
      sprintf("mysql:host=%s;dbname=%s;charset=UTF8", DB_HOST, DB_NAME),
      DB_USER,
      DB_PASSWORD,
      [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
    );
  }

  return $pdo;
}

function makeSQL($sql, array $params = null,  $topic = 2)
{
    try {
        $stmt = db()->prepare($sql, [PDO::ATTR_CURSOR => PDO::CURSOR_FWDONLY]);
        if (is_null($params)) {
            $stmt->execute();
        } else {
            $stmt->execute($params);
        }
        // Verifica se la query è un SELECT
        if (stripos($sql, "SELECT") === 0) {
          // Se è un SELECT, restituisci i dati
          return $stmt->fetchAll(PDO::FETCH_ASSOC);
      } else {
          // Se non è un SELECT (per esempio un INSERT o UPDATE), restituisci true/false
          return $stmt->rowCount() > 0;  // rowCount() restituisce il numero di righe affette dalla query
      }
    } catch (exception $e) {
        $par = print_r($params, 1);
        $sqlerror = $e->getMessage();
        $msg = "ERROR makeSQL $sqlerror on SQL $sql with params $par on line " . __LINE__ . "\n";
        telegramMsg($msg, $topic);
        return [];
    } finally {
        $stmt = null;
    }
}

function respondWithJson($data, int $statusCode = 200): void
{
    http_response_code($statusCode);
    header("Content-Type: application/json; charset=UTF-8");
    echo json_encode($data);
    exit;
}

