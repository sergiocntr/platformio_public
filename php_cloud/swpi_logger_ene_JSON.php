<?php
require("config.php");
$_POST = json_decode(file_get_contents('php://input'), true);
$link = mysqli_connect($server, $user, $pwd, $db);
if (mysqli_connect_errno()) {
    printf("Connect failed: %s\n", mysqli_connect_error());
    exit();
}
$num_record = count($_POST['v']);
$num_record--;
//echo var_dump($_POST);
//echo $_POST['temp'];
//echo $num_record;
//echo $num_record;
//exit(); 
$myTime = time();
$myFile = "../log/sqleneLog.log";
$fh = fopen($myFile, 'a');
$message = date('Y-m-d H:i:s') . " - Num Record: " . $num_record;
fwrite($fh, $message . "\n");
for ($i = $num_record; $i >= 0; $i--) {
    $volt = $_POST['v'][$i];
    $volt = round($volt, 2);
    $curr = round($_POST['i'][$i], 2);
    $power = round($_POST['p'][$i], 2);
    $cosFi = round($_POST['c'][$i], 2);


    $sql = "INSERT INTO ENERGIA (`TIMESTAMP_LOCAL`, `VOLT`, `AMP`, `COSPI`, `POWER`) 
        VALUES ('" . date('Y-m-d H:i:s', $myTime) . "','" . $volt . "','" . $curr . "','" . $cosFi . "','" . $power . "');";
    //$sql = "INSERT INTO `ENERGIA`( `TIMESTAMP_LOCAL`,`VOLT`, `AMP`, `POWER`, `COSPI`) 
    //   VALUES  ('". date('Y-m-d H:i:s',$myTime)."','".$volt."','".$ampere."','".$power."','".$phase."');";
    if (mysqli_query($link, $sql)) {
        $message = "Ok scritto record " . $i;
        fwrite($fh, $message . "\n");
    } else {
        $message = "Error: " . $sql . "\n" . mysqli_error($link);
        fwrite($fh, $message . "\n");

    }
    $myTime -= 5;
}
mysqli_close($link);
fclose($fh);
