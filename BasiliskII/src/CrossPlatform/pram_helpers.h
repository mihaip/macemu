#include "xpram.h"

#ifdef SHEEPSHAVER
#define XPRAM_OFFSET 0x1300
#else
#define XPRAM_OFFSET 0x0
#endif

static void SetDefaultAlertSound() {
  XPRAM[XPRAM_OFFSET + 0x7d] = 0x09; // Sosumi
}

static void CheckPRAM() {
  static uint8 last_xpram[XPRAM_SIZE];
  static bool initialized_last_xpram = false;
  if (!initialized_last_xpram) {
    memcpy(last_xpram, XPRAM, XPRAM_SIZE);
    initialized_last_xpram = true;
    return;
  }

  if (memcmp(last_xpram, XPRAM, XPRAM_SIZE)) {
    for (int i = XPRAM_OFFSET; i < XPRAM_SIZE; i++) {
      if (last_xpram[i] != XPRAM[i]) {
        printf("PRAM[%x] changed from %02x to %02x\n", i - XPRAM_OFFSET,
               last_xpram[i], XPRAM[i]);
      }
    }
    memcpy(last_xpram, XPRAM, XPRAM_SIZE);
  }
}
