<?php
if (!defined('CONFIG_LOADED')) {
    die('Direct access not allowed');
}
function telegram($msg)
{
  //global $telegrambot,$telegramchatid;
  //Casa2Bot @chittammurtbot 1240314680:AAExn_Ep469YGcU2bEQ9gLSbPuAXK4iaIKA
  //CasaBot @srgCasaBot='613979421:AAFbJBISkDSw4zUFOPv2NxLE1XrQ5KcHzlw';
  $telegrambot = '1240314680:AAExn_Ep469YGcU2bEQ9gLSbPuAXK4iaIKA';
  $telegramchatid = '-1001197831676';
  $url = 'https://api.telegram.org/bot' . $telegrambot . '/sendMessage';
  $data = array('chat_id' => $telegramchatid, 'text' => $msg);
  $options = array('http' => array('method' => 'POST', 'header' => "Content-Type:application/x-www-form-urlencoded\r\n", 'content' => http_build_query($data), ), );
  $context = stream_context_create($options);
  $result = file_get_contents($url, false, $context);
  return $result;
}
function telegramKM($msg)
{
  $telegrambot = '6059314220:AAHXAjClrnnLUNPmLOXMnA9n89Iig1qMBTE';
  $telegramchatid = '-1001703343350';
  $groupMSG = '83';
  $url = 'https://api.telegram.org/bot' . $telegrambot . '/sendMessage';
  $data = array('chat_id' => $telegramchatid, 'text' => $msg, 'reply_to_message_id' => $groupMSG);
  $options = array('http' => array('method' => 'POST', 'header' => "Content-Type:application/x-www-form-urlencoded\r\n", 'content' => http_build_query($data), ), );
  $context = stream_context_create($options);
  $result = file_get_contents($url, false, $context);
  return $result;
}
function telegramToro($msg)
{

  $telegrambot = '5674735909:AAG1mnYqsrIhEzWyfGUlJ2QyDrbuHYYQvUw';
  $telegramchatid = '495122578';
  $url = 'https://api.telegram.org/bot' . $telegrambot . '/sendMessage';
  $data = array('chat_id' => $telegramchatid, 'text' => $msg);
  $options = array('http' => array('method' => 'POST', 'header' => "Content-Type:application/x-www-form-urlencoded\r\n", 'content' => http_build_query($data), ), );
  $context = stream_context_create($options);
  $result = file_get_contents($url, false, $context);
  return $result;
}
function telegramMsg($msg, $thread_id = 1)
{
  $telegrambot = '5674735909:AAG1mnYqsrIhEzWyfGUlJ2QyDrbuHYYQvUw';
  $telegramchatid = '-1002073154198'; // Inserisci qui l'ID della chat
    $url = 'https://api.telegram.org/bot' . $telegrambot . '/sendMessage';

    $data = array(
        'chat_id' => $telegramchatid,
        'text' => $msg,
        'message_thread_id' => $thread_id // Aggiungere l'ID del topic
    );

    $options = array(
        'http' => array(
            'method' => 'POST',
            'header' => "Content-Type:application/x-www-form-urlencoded\r\n",
            'content' => http_build_query($data),
        ),
    );

    $context = stream_context_create($options);
    $result = file_get_contents($url, false, $context);

    return $result;
}