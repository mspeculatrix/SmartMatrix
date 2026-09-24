// app.js for webserver

const PRT_API = '/apiserver.php'
var condensed = false
var emphasised = false

/* Pop up a dialogue box asking if the user is sure they want to print */
function confirmPrint(filename) {
	let conf = confirm("Are you sure you want to print " + filename + "?");
	if (conf === true) {
		fetch(PRT_API + "/pf?f=" + filename)
			.then((response) => response.json())
			.then((json) => {
				updateResultMsg(json['status'], json['result'], json['bytes'])
			})
	}
}

/* Make the file buttons clickable */
function makeFileBtnsClickable() {
	var files = document.getElementsByClassName('fileBtn')
	for (let i = 0; i < files.length; i++) {
		files[i].addEventListener('click', () => {
			confirmPrint(files[i].textContent)
		})
	}
}

/* Update CSS for a button */
function toggleState(state, btnId) {
	if(state) {
		btnId.classList.remove('active')
	} else {
		btnId.classList.add('active')
	}
	return !state
}

/* Send a special code to the printer - eg, formfeed, linefeed */
function sendCode(code, btn) {
	const condBtn = document.getElementById('cond')
	const emphBtn = document.getElementById('emph')
	btn.classList.add('pushed')
	var sendcode = code
	switch (code) {
		case 'init':
			condBtn.classList.remove('active')
			condensed = false
			break
		case 'cond':
			sendcode = condensed ? 'condoff' : 'condon'
			condensed = toggleState(condensed, btn)
			if(condensed) {
				// Turn off emphasised as two states are not compatible
				emphBtn.classList.remove('active')
				emphasised = false
			}
			break
		case 'emph':
			sendcode = emphasised ? 'emphoff' : 'emphon'
			emphasised = toggleState(emphasised, btn)
			if(emphasised) {
				// Turn off condensed as two states are not compatible
				condBtn.classList.remove('active')
				condensed = false
			}
			break
		}
	fetch(PRT_API + "/cmd/" + sendcode)
		.then((response) => response.json())
		.then((json) => {
			updateResultMsg(json['status'], json['result'], json['bytes'])
			if(json['status'] !== "OK") {
				switch (code) {
					case 'cond':
						condBtn.classList.remove('active')
						condensed = false
						break
					case 'emph':
						emphBtn.classList.remove('active')
						emphasised = false
						break
				}
			}
		})
	setTimeout(() => { btn.classList.remove('pushed') }, 250)
}

/* Get the API to send an up to date list of files in the 'files/' folder */
function updateFileList() {
	let filelistDiv = document.getElementById('filelist')
	filelistDiv.style.opacity = 0.3
	fetch(PRT_API + "/filelist")
	.then((response) => response.json())
	.then((filejson) => {
		let fileListBtns = []
		if(filejson['status'] == 'OK') {
			let fileslist = filejson['result']
			for(var i = 0; i < fileslist.length; i++) {
				fileListBtns.push('<button class="fileBtn" id="file'
					+ i	+ '">' + fileslist[i] + '</button>'
				)
			}
			filelistDiv.innerHTML = fileListBtns.join("\n")
			makeFileBtnsClickable()
		}
		updateResultMsg(filejson['status'], 'file list', 0)
	})
	filelistDiv.style.opacity = "1.0"
}

/* Print a new result message on the page */
function updateResultMsg(status, result, bytes) {
	const resultEl = document.getElementById('result')
	let resultStr = status + ' : ' + result
	if(bytes > 0) {
		resultStr += ' [' + bytes + ' bytes]'
	}
	resultEl.innerHTML = resultStr
	if (status == "ERR") {
		resultEl.classList.add(status)
	} else {
		resultEl.classList.remove('ERR')
	}
}

window.addEventListener('DOMContentLoaded', () => {
	const updateBtn = document.getElementById('updateBtn')
	const lfBtn = document.getElementById('lf')
	const ffBtn = document.getElementById('ff')
	const condBtn = document.getElementById('cond')
	const emphBtn = document.getElementById('emph')
	const initBtn = document.getElementById('init')
	const result = document.getElementById('result')

	// Get the current file list
	updateFileList()

	// Set up buttons
	updateBtn.addEventListener('click', () => {
		updateFileList()
	})
	lfBtn.addEventListener('click', () => {
		sendCode('lf', lfBtn, result)
	})
	ffBtn.addEventListener('click', () => {
		sendCode('ff', ffBtn, result)
	})
	condBtn.addEventListener('click', () => {
		sendCode('cond', condBtn, result)
	})
	emphBtn.addEventListener('click', () => {
		sendCode('emph', emphBtn, result)
	})
	initBtn.addEventListener('click', () => {
		sendCode('init', initBtn, result)
	})

	setTimeout(() => {
        result.classList.add('hide')
    }, 5000)

	makeFileBtnsClickable()
})
