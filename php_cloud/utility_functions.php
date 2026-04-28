<?php
/**
 *  Given a file, i.e. /css/base.css, replaces it with a string containing the
 *  file's mtime, i.e. /css/base.1221534296.css.
 *
 *  @param $file  The file to be loaded. works on all type of paths.
 */

if (!defined('CONFIG_LOADED')) {
    die('Direct access not allowed');
} 
function auto_version($file)
{
  if ($file[0] !== '/') {
    $file = rtrim(str_replace(DIRECTORY_SEPARATOR, '/', dirname($_SERVER['PHP_SELF'])), '/') . '/' . $file;
  }

  if (!file_exists($_SERVER['DOCUMENT_ROOT'] . $file))
    return $file;

  $mtime = filemtime($_SERVER['DOCUMENT_ROOT'] . $file);
  return preg_replace('{\\.([^./]+)$}', ".$mtime.\$1", $file);
}