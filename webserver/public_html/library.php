<?php

/***
	library.php
 ***/

define('DOC_ROOT_PATH', $_SERVER['DOCUMENT_ROOT'] . '/');

define('NL',	"\n");
define('TAB',	'    ');

function readCfgFile(string $file, $delim = '=')
{
	$cfg = array('file' => $file);
	if (file_exists($file)) {
		$fh = fopen($file, 'r');
		if ($fh) {
			while (($line = fgets($fh, 4096)) !== false) {
				if ((substr($line, 0, 1) != '#')
					&& (strlen($line) > 3)
					&& (strpos($line, '=') !== false)
				) {
					$lineElements = explode($delim, $line);
					$key = trim($lineElements[0]);
					$cfg[$key] = trim($lineElements[1]);
				}
			}
		} else {
			echo "Error opening file: $file" . NL;
		}
		fclose($fh);
	}
	return $cfg;
}

function writeCfgFile(string $file, array $dataArray, $comment = FALSE, $delim = '=')
{
	$fh = fopen($file, 'w');
	if ($comment) {
		fwrite($fh, '# ' . $comment . "\n");
	}
	foreach ($dataArray as $key => $value) {
		fwrite($fh, $key . $delim . $value . "\n");
	}
	fclose($fh);
}
