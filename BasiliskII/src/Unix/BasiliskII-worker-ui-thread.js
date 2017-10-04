
var SCREEN_WIDTH = 640;
var SCREEN_HEIGHT = 480;
var SCREEN_BUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT * 4; // 32bpp;

var canvasCtx = canvas.getContext('2d');
var imageData = canvasCtx.createImageData(SCREEN_WIDTH, SCREEN_HEIGHT);

var screenBuffer = new SharedArrayBuffer(SCREEN_BUFFER_SIZE);
var screenBufferView = new Uint8Array(screenBuffer);

var VIDEO_MODE_BUFFER_SIZE = 10;
var videoModeBuffer = new SharedArrayBuffer(VIDEO_MODE_BUFFER_SIZE);
var videoModeBufferView = new Uint8Array(videoModeBuffer);

var AUDIO_CONFIG_BUFFER_SIZE = 10;
var audioConfigBuffer = new SharedArrayBuffer(AUDIO_CONFIG_BUFFER_SIZE);
var audioConfigBufferView = new Uint8Array(audioConfigBuffer);

var AUDIO_DATA_BUFFER_SIZE = 10;
var audioDataBuffer = new SharedArrayBuffer(AUDIO_DATA_BUFFER_SIZE);
var audioDataBufferView = new Uint8Array(audioDataBuffer);

var INPUT_BUFFER_SIZE = 100;
var inputBuffer = new SharedArrayBuffer(INPUT_BUFFER_SIZE * 4);
var inputBufferView = new Int32Array(inputBuffer);
var inputQueue = [];

var InputBufferAddresses = {
  globalLockAddr: 0,
  mouseMoveFlagAddr: 1,
  mouseMoveXDeltaAddr: 2,
  mouseMoveYDeltaAddr: 3,
  mouseButtonStateAddr: 4,
  keyEventFlagAddr: 5,
  keyCodeAddr: 6,
  keyStateAddr: 7,
};

var InputLockStates = {
  EMPTY: 0,
  WRITING: 1,
  FULL: 2,
  READING: 3,
};

function acquireInputLock() {
  var res = Atomics.compareExchange(
    inputBufferView,
    InputBufferAddresses.globalLockAddr,
    InputLockStates.EMPTY,
    InputLockStates.WRITING
  );
  if (res === InputLockStates.EMPTY) {
    return true;
  }
  return false;
}
function releaseInputLock() {
  Atomics.store(
    inputBufferView,
    InputBufferAddresses.globalLockAddr,
    InputLockStates.FULL
  ); // unlock
}
function tryToSendInput() {
  if (!acquireInputLock()) {
    return;
  }
  var hasMouseMove = false;
  var mouseMoveX = 0;
  var mouseMoveY = 0;
  var mouseButtonState = -1;
  var hasKeyEvent = false;
  var keyCode = -1;
  var keyState = -1;
  // currently only one key event can be sent per sync
  // TODO: better key handling code
  var remainingKeyEvents = [];
  for (var i = 0; i < inputQueue.length; i++) {
    var inputEvent = inputQueue[i];
    switch (inputEvent.type) {
      case 'mousemove':
        hasMouseMove = true;
        mouseMoveX += inputEvent.dx;
        mouseMoveY += inputEvent.dy;
        break;
      case 'mousedown':
      case 'mouseup':
        mouseButtonState = inputEvent.type === 'mousedown' ? 1 : 0;
        break;
      case 'keydown':
      case 'keyup':
        if (hasKeyEvent) {
          remainingKeyEvents.push(inputEvent);
          break;
        }
        hasKeyEvent = true;
        keyState = inputEvent.type === 'keydown' ? 1 : 0;
        keyCode = inputEvent.keyCode;
        break;
    }
  }
  if (hasMouseMove) {
    inputBufferView[InputBufferAddresses.mouseMoveFlagAddr] = 1;
    inputBufferView[InputBufferAddresses.mouseMoveXDeltaAddr] = mouseMoveX;
    inputBufferView[InputBufferAddresses.mouseMoveYDeltaAddr] = mouseMoveY;
  }
  inputBufferView[InputBufferAddresses.mouseButtonStateAddr] = mouseButtonState;
  if (hasKeyEvent) {
    inputBufferView[InputBufferAddresses.keyEventFlagAddr] = 1;
    inputBufferView[InputBufferAddresses.keyCodeAddr] = keyCode;
    inputBufferView[InputBufferAddresses.keyStateAddr] = keyState;
  }
  releaseInputLock();
  inputQueue = remainingKeyEvents;
}
canvas.addEventListener('mousemove', function (event) {
  inputQueue.push({type: 'mousemove', dx: event.offsetX, dy: event.offsetY});
});
canvas.addEventListener('mousedown', function (event) {
  inputQueue.push({type: 'mousedown'});
});
canvas.addEventListener('mouseup', function (event) {
  inputQueue.push({type: 'mouseup'});
});
window.addEventListener('keydown', function (event) {
  inputQueue.push({type: 'keydown', keyCode: event.keyCode});
});
window.addEventListener('keyup', function (event) {
  inputQueue.push({type: 'keyup', keyCode: event.keyCode});
});


var worker = new Worker('BasiliskII-worker-boot.js');

worker.postMessage({
  inputBuffer: inputBuffer,
  inputBufferSize: INPUT_BUFFER_SIZE,
  screenBuffer: screenBuffer,
  screenBufferSize: SCREEN_BUFFER_SIZE,
  videoModeBuffer: videoModeBuffer,
  videoModeBufferSize: VIDEO_MODE_BUFFER_SIZE,
  SCREEN_WIDTH: SCREEN_WIDTH,
  SCREEN_HEIGHT: SCREEN_HEIGHT,
});

function drawScreen() {
  const pixelsRGBA = imageData.data;
  const numPixels = SCREEN_WIDTH * SCREEN_HEIGHT;
  const expandedFromPalettedMode = videoModeBufferView[3];
  if (expandedFromPalettedMode) {
    for (var i = 0; i < numPixels; i++) {
      // palette
      pixelsRGBA[i * 4 + 0] = screenBufferView[i * 4 + 0];
      pixelsRGBA[i * 4 + 1] = screenBufferView[i * 4 + 1];
      pixelsRGBA[i * 4 + 2] = screenBufferView[i * 4 + 2];
      pixelsRGBA[i * 4 + 3] = 255; // full opacity
    }
  } else {
    for (var i = 0; i < numPixels; i++) {
      // ARGB
      pixelsRGBA[i * 4 + 0] = screenBufferView[i * 4 + 1];
      pixelsRGBA[i * 4 + 1] = screenBufferView[i * 4 + 2];
      pixelsRGBA[i * 4 + 2] = screenBufferView[i * 4 + 3];
      pixelsRGBA[i * 4 + 3] = 255; // full opacity
    }
  }

  canvasCtx.putImageData(imageData, 0, 0);
}


function asyncLoop() {
  drawScreen();
  tryToSendInput();
  requestAnimationFrame(asyncLoop);
}

asyncLoop();