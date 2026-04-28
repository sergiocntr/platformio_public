<?php
require("./config.php");
//$ThisFileName = basename(__FILE__, '.php');
$message = "";
$DataIn = json_decode(file_get_contents('php://input'), true);
$salH = $DataIn['salH'];
$salT = $DataIn['salT'];
if (($salH > 120) || ($salT > 120)) {
    $message = "Temp/Press fuori limite: ";
}
$sql = "INSERT INTO METEOCASA ( `TEMP`, `HUM`, `humSal`, `tempSal`) 
        VALUES ('" . $DataIn['camT'] . "','" . $DataIn['camH'] . "','" . $salH . "','" . $salT . "');";
$reponse = faiSQL($sql);
echo $reponse;