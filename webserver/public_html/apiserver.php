<?php
// apiserver.php - Backend API for SmartMatrix webserver

require_once($_SERVER['DOCUMENT_ROOT'] . '/library.php');

// Set JSON header and CORS
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, OPTIONS');

// Configuration
define('FILES_DIR', __DIR__ . '/files/'); // Path to file directory

// Control Character Byte Definitions (ASCII / Epson MX-80)
define('BYTE_LF',       "\x0A");		// Line Feed
define('BYTE_FF',       "\x0C");		// Form Feed
define('BYTE_INIT',     "\x1B\x40"); 	// ESC @ (Initialize Printer)
define('BYTE_COND_ON',  "\x0F"); 		// SI (Condensed Mode ON)
define('BYTE_COND_OFF', "\x12"); 		// DC2 (Condensed Mode OFF)
define('BYTE_EMPH_ON',  "\x1B\x45"); 	// ESC E (Emphasized Mode ON)
define('BYTE_EMPH_OFF', "\x1B\x46");	// ESC F (Emphasized Mode OFF)

// -----------------------------------------------------------------------------
// Send Raw Bytes to SmartMatrix TCP Socket
// -----------------------------------------------------------------------------
function send_to_smartmatrix(string $data, int $timeout_sec = 3): int
{
	$length = strlen($data);
	$bytes_written = 0;

	$svrconfig = readCfgFile('printsvr.cfg');

	// Create TCP IPv4 Socket
	$socket = @socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
	if ($socket === false) {
		return 0;
	}

	// Set Send & Receive Timeouts
	$timeout = ['sec' => $timeout_sec, 'usec' => 0];
	socket_set_option($socket, SOL_SOCKET, SO_SNDTIMEO, $timeout);
	socket_set_option($socket, SOL_SOCKET, SO_RCVTIMEO, $timeout);

	// Connect to SmartMatrix
	$result = @socket_connect(
		$socket,
		$svrconfig['smartmatrix_ip'],
		$svrconfig['smartmatrix_port']
	);
	// Check that we connected OK, otherwise bug out
	if ($result === false) {
		socket_close($socket);
		return 0;
	}

	// Write raw binary data out to socket
	while ($bytes_written < $length) {
		$written = @socket_write(
			$socket,
			substr($data, $bytes_written),
			$length - $bytes_written
		);
		if ($written === false) {
			socket_close($socket);
			return false;
		}
		$bytes_written += $written;
	}

	// Clean disconnect
	socket_close($socket);
	return $bytes_written;
}

// -----------------------------------------------------------------------------
// ROUTE HANDLING
// -----------------------------------------------------------------------------

$uri = $_SERVER['REQUEST_URI'];
$path = parse_url($uri, PHP_URL_PATH);

// ROUTE: /filelist - List printable files in designated folder
if (strpos($path, '/filelist') !== false) {
	if (!is_dir(FILES_DIR)) {
		echo json_encode(['status' => 'ERROR', 'result' => 'Files directory missing']);
		exit;
	}

	// Read directory files, skipping hidden files and directories
	$files = array_values(array_filter(scandir(FILES_DIR), function ($file) {
		return !is_dir(FILES_DIR . $file) && $file[0] !== '.';
	}));

	echo json_encode([
		'status' => 'OK',
		'result'  => $files
	]);
	exit;
}

// ROUTE: /pf?f=filename - Print a full file
if (strpos($path, '/pf') !== false) {
	$filename = $_GET['f'] ?? '';
	// Sanitize filename to prevent directory traversal
	$filename = basename($filename);
	$filepath = FILES_DIR . $filename;

	if (empty($filename) || !file_exists($filepath)) {
		echo json_encode(
			['status' => 'ERR', 'result' => 'File not found', 'bytes' => 0]
		);
		exit;
	}

	$file_content = file_get_contents($filepath);
	if ($file_content === false) {
		echo json_encode(
			['status' => 'ERR', 'result' => 'Read failure', 'bytes' => 0]
		);
		exit;
	}

	$bytes_sent = send_to_smartmatrix($file_content, 10);
	if ($bytes_sent > 0) {
		echo json_encode([
			'status' => 'OK',
			'result' => 'Sent ' . $filename	. ' to printer',
			'bytes' => $bytes_sent
		]);
	} else {
		echo json_encode([
			'status' => 'ERR',
			'result' => 'Printer connection failed',
			'bytes' => 0
		]);
	}
	exit;
}

// ROUTE: /cmd/{code} - Control codes and state queries
if (preg_match('#/cmd/([a-zA-Z0-9_-]+)#', $path, $matches)) {
	$cmd = strtolower($matches[1]);

	$response = ['status' => 'none', 'result' => 'fault', 'bytes' => 0];

	$offline_error = [
		'status' => 'ERR',
		'result' => 'Printer offline',
		'bytes' => 0
	];

	switch ($cmd) {
		case 'lf':
			$bytes_sent = send_to_smartmatrix(BYTE_LF);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Line feed sent',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;
		case 'ff':
			$bytes_sent = send_to_smartmatrix(BYTE_FF);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Form feed sent',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;

		case 'init':
			$bytes_sent = send_to_smartmatrix(BYTE_INIT);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Printer reset',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;

		case 'condon':
			$bytes_sent = send_to_smartmatrix(BYTE_COND_ON);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Condensed mode ON',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;

		case 'condoff':
			$bytes_sent = send_to_smartmatrix(BYTE_COND_OFF);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Condensed mode OFF',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;

		case 'emphon':
			$bytes_sent = send_to_smartmatrix(BYTE_EMPH_ON);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Emphasized mode ON',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;

		case 'emphoff':
			$bytes_sent = send_to_smartmatrix(BYTE_EMPH_OFF);
			$response = $bytes_sent > 0
				? [
					'status' => 'OK',
					'result' => 'Emphasized mode OFF',
					'bytes' => $bytes_sent
				]
				: $offline_error;
			break;

		default:
			$response = [
				'status' => 'ERR',
				'result' => 'Unknown command',
				'bytes' => 0
			];
			break;
	}
	echo json_encode($response);
	exit;
}

// Fallback response for unhandled endpoints
echo json_encode([
	'status' => 'ERR',
	'result' => 'Invalid endpoint',
	'bytes' => 0
]);
