Fork of the Basilisk II Macintosh emulator modified to be compiled to WebAssembly and run in the browser. Part of the [Infinite Mac](https://github.com/mihaip/infinite-mac), see [its README](https://github.com/mihaip/infinite-mac/blob/main/README.md) and [introductory blog post](https://blog.persistent.info/2022/03/blog-post.html) for more details.

Based on [James Friend's original browser port of Basilisk II](https://jamesfriend.com.au/basilisk-ii-classic-mac-emulator-in-the-browser).

## How It Works

The [TECH](BasiliskII/TECH) document describes the basic Basilisk II architecture. The WebAssembly port ends up being a modified Unix version, with a subset of [Unix implementations](BasiliskII/src/Unix) of Basilisk II parts are replaced with [JavaScript-based ones](BasiliskII/src/Unix/JS). They generally use the [ES_ASM()](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#interacting-with-code-call-javascript-from-native) macros to call into JavaScript code, with `EmulatorWorkerApi` in the [infinite-mac repo](https://github.com/mihaip/infinite-mac/blob/main/src/BasiliskII/emulator-worker.ts) serving as the receiving endpoint.

The Basilisk emulator runs in an infinite loop (simulating the classic Mac and its CPU) thus it cannot yield to the browser's event loop. Therefore it is executed in a web worker, which interacts with the main browser thread for input and output. [`SharedArrayBuffer`s](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer) and [`Atomics`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Atomics) operations are used to efficiently communicate with the worker in the absence of `postMessage`/`message` event handlers.

## Build Instructions

Not a standalone project, see [the build instructions in the infinite-mac repo](https://github.com/mihaip/infinite-mac#building-basilisk-ii) for details on how to build this.
