<?php
require("../log/utilLog.php");
$ThisFileName = basename(__FILE__, '.php');
$message = "";
$swpipwd1 = $_GET['pwd'];
$check = 0;
if ($swpipwd1 != $swpipwd) {
    $message .= "Pass Sbagliata" . "\n";

    $check = 1;
}
$voltage = $_GET['bat'];
if (($voltage < 3000) || ($voltage > 5000)) {
    $message .= "Voltaggio non corretto: " . $voltage . "\n";
    $check = 1;
}
if ($check == 0) {


    $Kconst = 273.15;
    $temp = $_GET['temp'];
    $temp = round($temp, 2);
    $press = round($_GET['press'], 2);
    $hum = round($_GET['hum'], 2);
    //$voltage = $_GET['bat'];
    $dew_point = pow(($hum / 100), (1 / 8)) * (112 + (0.9 * $temp)) + (0.1 * $temp) - 112;
    $dewpointK = $dewpoint + $Kconst;
    $humidex = $temp + 0.5555 * (6.11 * pow(exp(1), (5417.7530 * ((1 / 273.16) - (1 / $dewpointK)))) - 10);
    $dew_point = round($dew_point, 2);
    $humidex = round($humidex, 2);
    $sql = "INSERT INTO METEO (TEMP, PRESSURE, HUM, DEW_POINT,HDEX,VOLTAGE) 
                VALUES ('" . $temp . "','" . $press . "','" . $hum . "','" . $dew_point . "','" . $humidex . "','" . $voltage . "');";

    $message = faiSQL($sql);
    
}

if ($message !== "") {
    faiLog($message, $ThisFileName);
}
