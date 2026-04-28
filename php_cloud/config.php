<?php
/*
// Includi config.php dalla root del documento
require_once $_SERVER['DOCUMENT_ROOT'] . '/config.php';

// Ora puoi accedere alle variabili e funzioni definite in config.php
*/
define('CONFIG_LOADED', true);

require_once 'telegram_functions.php';
require_once 'database_class.php';
require_once 'utility_functions.php';
require_once 'sanitization.php';
require_once 'validation.php';

$swpipwd = "admin";