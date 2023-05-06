Fork of the Basilisk II and SheepShaver Macintosh emulators, modified to be compiled to WebAssembly and run in the browser. Part of the [Infinite Mac](https://github.com/mihaip/infinite-mac), see [its README](https://github.com/mihaip/infinite-mac/blob/main/README.md) and [introductory blog post](https://blog.persistent.info/2022/03/blog-post.html) for more details.

Based on [James Friend's original browser port of Basilisk II](https://jamesfriend.com.au/basilisk-ii-classic-mac-emulator-in-the-browser).

## How It Works

The [TECH](BasiliskII/TECH) document describes the basic Basilisk II architecture. The WebAssembly port ends up being a modified Unix version, with a subset of [Unix implementations](BasiliskII/src/Unix) of Basilisk II parts are replaced with [JavaScript-based ones](BasiliskII/src/Unix/JS). They generally use the [ES_ASM()](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#interacting-with-code-call-javascript-from-native) macros to call into JavaScript code, with `EmulatorWorkerApi` in the [infinite-mac repo](https://github.com/mihaip/infinite-mac/blob/main/src/emulator/emulator-worker.ts) serving as the receiving endpoint.

The Basilisk emulator runs in an infinite loop (simulating the classic Mac and its CPU) thus it cannot yield to the browser's event loop. Therefore it is executed in a web worker, which interacts with the main browser thread for input and output. [`SharedArrayBuffer`s](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer) and [`Atomics`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Atomics) operations are used to efficiently communicate with the worker in the absence of `postMessage`/`message` event handlers.

SheepShaver ends up working pretty much the same way.

## Build Instructions

Not a standalone project, see [the build instructions in the infinite-mac repo](https://github.com/mihaip/infinite-mac#building-the-emulators) for details on how to build this.

## Branches

This repository contains multiple branches, the notable ones are:

-   [infinite-mac-kanjitalk755](https://github.com/mihaip/macemu/commits/infinite-mac-kanjitalk755): Current Infinite Mac work, based on the [kanjitalk755/macemu](https://github.com/kanjitalk755/macemu) fork of Basilisk II and SheepShaver.
-   [bas-emscripten-release](https://github.com/mihaip/macemu/commits/bas-emscripten-release): Initial Infinite Mac work, based on James Friend's [jsdf/macemu](https://github.com/jsdf/macemu) fork.
-   [infinite-mac-jsdf](https://github.com/mihaip/macemu/commits/infinite-mac-jsdf): Squashed version of the work done in the `bas-emscripten-release` branch, so that it could serve as a starting point for the `infinite-mac-kanjitalk755` work.

To sync with the upstream `kanjitalk755/macemu` fork, run:

```sh
git remote add upstream-kanjitalk755 https://github.com/kanjitalk755/macemu.git
git fetch upstream-kanjitalk755
git rebase --committer-date-is-author-date upstream-kanjitalk755/master
```
