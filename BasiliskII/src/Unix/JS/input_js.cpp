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
      D(bug("[input_js] mouse button: state=%d\n", mouse_button_state));
      if (mouse_button_state == 0) {
        ADBMouseUp(0);
      } else {
        ADBMouseDown(0);
      }
    }

    int has_mouse_position = EM_ASM_INT_V(
        { return workerApi.getInputValue(workerApi.InputBufferAddresses.mousePositionFlagAddr); });
    if (has_mouse_position) {
      int mouseX = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.mousePositionXAddr); });

      int mouseY = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.mousePositionYAddr); });

      D(bug("[input_js] mouse move: dx=%d, dy=%d\n", mouseX, mouseY));
      ADBMouseMoved(mouseX, mouseY);
    }

    int has_key_event = EM_ASM_INT_V(
        { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyEventFlagAddr); });
    if (has_key_event) {
      int keycode = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyCodeAddr); });

      int keystate = EM_ASM_INT_V(
          { return workerApi.getInputValue(workerApi.InputBufferAddresses.keyStateAddr); });

      D(bug("[input_js] key event: keycode=%d, keystate=%d\n", keycode, keystate));
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
      D(bug("[input_js] ethernet interrupt\n"));
      SetInterruptFlag(INTFLAG_ETHER);
      TriggerInterrupt();
    }

    EM_ASM({ workerApi.releaseInputLock(); });
  }
}
