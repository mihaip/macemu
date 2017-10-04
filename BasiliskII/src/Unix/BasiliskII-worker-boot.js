importScripts('js/Emitter.js');

var window = Emitter({});
window.location = location;
window.encodeURIComponent = self.encodeURIComponent.bind(self);
var document = Emitter({});

var pathGetFilenameRegex = /\/([^\/]+)$/;

function pathGetFilename(path) {
  var matches = path.match(pathGetFilenameRegex);
  if (matches && matches.length) {
    return matches[1];
  } else {
    return path;
  }
}

function addAutoloader(module) {
  var loadDatafiles = function() {
    module.autoloadFiles.forEach(function(filepath) {
      module.FS_createPreloadedFile('/', pathGetFilename(filepath), filepath, true, true);
    });
  };

  if (module.autoloadFiles) {
    module.preRun = module.preRun || [];
    module.preRun.unshift(loadDatafiles);
  }

  return module;
}

function addCustomAsyncInit(module) {
  if (module.asyncInit) {
    module.preRun = module.preRun || [];
    module.preRun.push(function waitForCustomAsyncInit() {
      module.addRunDependency('__moduleAsyncInit');

      module.asyncInit(module, function asyncInitCallback() {
        module.removeRunDependency('__moduleAsyncInit');
      });
    });
  }
}

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

var Module = null

self.onmessage = function(msg) {
  console.log('init worker');
  startEmulator(msg.data);
}

function startEmulator(parentConfig) {
  var screenBufferView = new Uint8Array(
    parentConfig.screenBuffer,
    0,
    parentConfig.screenBufferSize
  );

  var videoModeBufferView = new Uint8Array(
    parentConfig.videoModeBuffer,
    0,
    parentConfig.videoModeBufferSize
  );

  var inputBufferView = new Int32Array(
    parentConfig.inputBuffer,
    0,
    parentConfig.inputBufferSize
  );

  function acquireInputLock() {
    var res = Atomics.compareExchange(
      inputBufferView,
      InputBufferAddresses.globalLockAddr,
      InputLockStates.FULL,
      InputLockStates.READING
    );
    if (res === InputLockStates.FULL) {
      return 1;
    }
    return 0;
  }

  function releaseInputLock() {
    // reset
    inputBufferView[InputBufferAddresses.mouseMoveFlagAddr] = 0;
    inputBufferView[InputBufferAddresses.mouseMoveXDeltaAddr] = 0;
    inputBufferView[InputBufferAddresses.mouseMoveYDeltaAddr] = 0;
    inputBufferView[InputBufferAddresses.mouseButtonStateAddr] = 0;
    inputBufferView[InputBufferAddresses.keyEventFlagAddr] = 0;
    inputBufferView[InputBufferAddresses.keyCodeAddr] = 0;
    inputBufferView[InputBufferAddresses.keyStateAddr] = 0;

    Atomics.store(
      inputBufferView,
      InputBufferAddresses.globalLockAddr,
      InputLockStates.EMPTY
    ); // unlock
  }

  var AudioConfig = null;

  var AudioBufferQueue = [];

  Module = {
    autoloadFiles: [
      'MacOS753',
      'DCImage.img',
      'Quadra-650.rom',
      'prefs',
    ],

    "arguments": ["--config", "prefs"],
    canvas: null,

    blit: function blit(bufPtr, width, height, depth, usingPalette) {
      // debugger
      videoModeBufferView[0] = width;
      videoModeBufferView[1] = height;
      videoModeBufferView[2] = depth;
      videoModeBufferView[3] = usingPalette;
      var length = (width * height) * (depth === 32 ? 4 : 1); // 32bpp or 8bpp
      for (var i = 0; i < length; i++) {
        screenBufferView[i] = Module.HEAPU8[bufPtr + i];
      }
    },

    openAudio: function openAudio(sampleRate, sampleSize, channels, framesPerBuffer) {
      AudioConfig = {
        sampleRate: sampleRate,
        sampleSize: sampleSize,
        channels: channels,
        framesPerBuffer: framesPerBuffer,
      };
      console.log(AudioConfig);
    },

    enqueueAudio: function enqueueAudio(bufPtr, nbytes) {
      bufPtr;
      nbytes;
      // AudioBufferQueue.push(Module.HEAPU8.slice(bufPtr, nbytes));
    },


    acquireInputLock: acquireInputLock,

    InputBufferAddresses: InputBufferAddresses,

    getInputValue: function getInputValue(addr) {
      return inputBufferView[addr]
    },

    print: console.log.bind(console),
    
    printErr: console.warn.bind(console),

    releaseInputLock: releaseInputLock,
  };


  // inject extra behaviours
  addAutoloader(Module);
  addCustomAsyncInit(Module);

  importScripts('BasiliskII.js');
}
