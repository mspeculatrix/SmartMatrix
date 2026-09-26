<?php
require_once($_SERVER['DOCUMENT_ROOT'] . '/library.php');
$svrconfig = readCfgFile('printsvr.cfg');
?>

<!DOCTYPE html>
<html lang="en-GB">

<head>
	<meta charset="UTF-8">
	<meta http-equiv="X-UA-Compatible" content="IE=edge">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>SmartMatrix</title>
	<!-- ----- CSS ----------------------------------------------- -->
	<link href="/css/bootstrap.css" rel="stylesheet">
	<link href="/css/bootstrap-grid.css" rel="stylesheet">
	<link href="/css/bootstrap-reboot.css" rel="stylesheet">
	<link href="/css/main.css" type="text/css" media="all" rel="stylesheet">

	<link href="/assets/fontawesome/css/fontawesome.css" rel="stylesheet" />
	<link href="/assets/fontawesome/css/solid.css" rel="stylesheet" />

	<!-- ----- JAVASCRIPT -------------------------------------------------- -->
	<script src="/js/jquery-3.3.1.min.js"></script> <!-- for FontAwesome -->
	<script src="/js/bootstrap.js"></script>
	<script src="/js/bootstrap.bundle.js"></script>
	<script src="app.js"></script>
</head>

<body>
	<div id="main" class="content container" role="main">
		<div id="mainSection" class="container">

			<div id="masthead">
				<h1 id="pagehead" class="pghead">SmartMatrix Server</h1>
			</div>

			<!-- FILE LIST -->
			<div class="row">
				<div class="col-12 centreCol">
					<div id="filelist"></div>
				</div>
			</div>

			<div class="row">
				<!-- CONTROL BUTTONS etc -->
				<div id="ctrls" class="col-4">
					<div class="ctrl">
						<button id="lf" class="ctrlBtn">LF</button>
					</div>
					<div class="ctrl">
						<button id="ff" class="ctrlBtn">FF</button>
					</div>
					<div class="ctrl">
						<button id="init" class="ctrlBtn">INIT</button>
					</div>
				</div>
				<!-- UPDATE LIST & MESSAGES -->
				<div id="manage" class="col-4">
					<div class="ctrl">
						<button id="updateBtn" class="ctrlBtn">update list</button>
					</div>
					<div class="ctrl">
						<img src="/img/mx80.png" class="mx80img" alt="mx80" width="150">
					</div>
				</div>
				<!-- STATES -->
				<div class="col-4" id="states">
					<div class="ctrl mode">
						<button id="cond" class="ctrlBtn modeBtn">CONDENSED</button>
					</div>
					<div class="ctrl mode">
						<button id="emph" class="ctrlBtn modeBtn">EMPHASISED</button>
					</div>
				</div>
			</div>

			<div class="row">
				<p id="result" class="msg"></p>
			</div>

			<div class="row">
				<p class="cfg">SmartMatrix IP: <span id="cfg_ip">
						<?php echo $svrconfig['smartmatrix_ip']; ?></span> Port:
					<span id="cfg_port"><?php echo $svrconfig['smartmatrix_port']; ?></span>
				</p>
			</div>

		</div><!-- #mainSection -->

	</div><!-- .content .container -->
</body>

</html>