#include "sysdeps.h"

#include <emscripten.h>

#include "adb.h"
#include "cpu_emulation.h"
#include "macos_util.h"
#include "main.h"

#define DEBUG 0
#include "debug.h"

void ReadJSInput() {
  int lock = EM_ASM_INT_V({ return workerApi.acquireInputLock(); });
  if (lock) {
    int mouse_button_state = EM_ASM_INT_V(
        { return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseButtonStateAddr); });

    if (mouse_button_state > -1) {
      if (mouse_button_state == 0) {
        ADBMouseUp(0);
      } else {
        ADBMouseDown(0);
      }
    }

    int has_mouse_move = EM_ASM_INT_V(
        { return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseMoveFlagAddr); });
    if (has_mouse_move) {
      int dx = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseMoveXDeltaAddr); });

      int dy = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.mouseMoveYDeltaAddr); });

      if (dx > 0 || dy > 0) {
        ADBMouseMoved(dx, dy);
      }
    }

    int has_key_event = EM_ASM_INT_V(
        { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyEventFlagAddr); });
    if (has_key_event) {
      int keycode = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyCodeAddr); });

      int keystate = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyStateAddr); });

      if (keystate == 0) {
        ADBKeyUp(keycode);
      } else {
        ADBKeyDown(keycode);
      }
    }
    int has_ethernet_interrupt = EM_ASM_INT_V({
      return workerApi.getInputValue(workerApi.InputBufferAddresses.ethernetInterruptFlagAddr);
    });
    if (has_ethernet_interrupt) {
      SetInterruptFlag(INTFLAG_ETHER);
      TriggerInterrupt();
    }

    EM_ASM({ workerApi.releaseInputLock(); });
  }
}
