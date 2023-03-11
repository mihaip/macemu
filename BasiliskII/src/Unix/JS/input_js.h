#ifndef INPUT_JS_H
#define INPUT_JS_H

#include "sysdeps.h"

void ReadJSInput();
// Meant to be invoked every tick (60hz) in cases where true idletime support is
// not available. The emulator will briefly sleep and perform periodic
// background tasks.
void FallbackSleep();

#endif
