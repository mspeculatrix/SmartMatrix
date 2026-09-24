<!DOCTYPE html>
<html lang="en-GB">
<?php
// ***** PAGE SETTINGS *****
define('APP_NAME', 'calvary');
define('PAGE_TYPE',	'col1');

// ***** DO NOT EDIT *****
date_default_timezone_set('Europe/Paris');
require_once($_SERVER['DOCUMENT_ROOT'] . '/include/std_config.php');
$pagemode = pagemode();
// ----- END OF DO NOT EDIT -----

include_once(module('head_begin'));
include_once(module('head_end'));
?>

<body id="body_img">

	<?php include_once(module('nav')); ?>

	<main id="content_main">
		<div class="container">

			<?php include_once(module('masthead')); ?>

			<?php
			$imgFiles = fileList('img', 'sm-calvary-');
			$pageImg = $imgFiles[array_rand($imgFiles)];
			?>
			<div class="row">
				<div class="centreCol wide <?php echo RUNMODE . ' ' . KIOSKMODE; ?>">
					<div id="imgwrap">
						<?php
						echo '<img id="postcard" src="" class="switchimg">';
						?>
					</div>
				</div>
			</div>

			<div class="row">
				<div class="centreCol narrow">
					<p id="notice" class="notice"></p>
				</div>
			</div>

			<!-- "Look" - DS -->

			<?php
			if (RUNMODE != 'kiosk') {
				include_once(module('tour'));
			}
			// include_once(module('message'));
			include_once(module('numbers'));
			include_once(module('citation'));
			?>

			<div class="row">
				<div class="centreCol narrow">
					<p id="aiinfo" class="aiinfo">Supervisory Intelligence: NT-Rev.14:6<br>Source: Dept of Approved Publications.</p>
				</div>
			</div>

		</div><!-- container -->
	</main><!-- /#content_main .container -->

	<script>
		function getCommandment(elemnt) {
			getRndAPIitem('commandment')
				.then((resp) => {
					if (resp.status === 200) {
						elemnt.innerHTML = resp.item
					}
				})
				.catch(error => {
					elemnt.innerHTML = "."
				})
		}

		window.addEventListener('DOMContentLoaded', () => {
			tourBox()
			const numPics = 19
			const picPrefix = 'sm-calvary'
			let imgwrap = document.getElementById("imgwrap")
			let pic = getRandomImage(picPrefix, numPics, 'webp')
			let postcard = document.getElementById("postcard")
			postcard.setAttribute('src', pic)
			setInterval(() => {
				postcard.classList.add('hide')
				setTimeout(() => {
					pic = getRandomImage(picPrefix, numPics, 'webp')
					postcard.setAttribute('src', pic)
					setTimeout(() => {
						postcard.classList.remove('hide')
					}, 1000)
				}, 1500)
			}, 10000)

			let notice = document.getElementById("notice")
			getCommandment(notice)
			setInterval(() => {
				getCommandment(notice)
			}, 9000)
		})
	</script>

	<?php
	include_once(module('footer'));
	include_once(module('body_end'));
	?>

</html>