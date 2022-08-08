#include "sysdeps.h"

#include <emscripten.h>

#include "clip.h"
#include "cpu_emulation.h"
#include "emul_op.h"
#include "macos_util.h"
#include "macroman.h"
#include "main.h"

#define DEBUG 0
#include "debug.h"

// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to
// the JS side
static bool we_put_this_data = false;

void ClipInit(void) { D(bug("ClipInit\n")); }

void ClipExit(void) { D(bug("ClipExit\n")); }

EM_JS(char *, getClipboardText, (), {
  const clipboardText = workerApi.getClipboardText();
  if (!clipboardText || !clipboardText.length) {
    return 0;
  }
  const clipboardTextLength = lengthBytesUTF8(clipboardText) + 1;
  const clipboardTextCstr = _malloc(clipboardTextLength);
  stringToUTF8(clipboardText, clipboardTextCstr, clipboardTextLength);
  return clipboardTextCstr;
});

// Mac application reads clipboard
void GetScrap(void **handle, uint32 type, int32 offset) {
  D(bug("GetScrap handle %p, type %08x, offset %d\n", handle, type, offset));
  switch (type) {
  case FOURCC('T', 'E', 'X', 'T'):
    char *clipboardTextCstr = getClipboardText();
    if (!clipboardTextCstr) {
      break;
    }

    char *clipboardTextMacRoman = const_cast<char *>(
        utf8_to_macroman(clipboardTextCstr, strlen(clipboardTextCstr)));
    free(clipboardTextCstr);
    size_t clipboardTextMacRomanLength = strlen(clipboardTextMacRoman);
    for (int i = 0; i < clipboardTextMacRomanLength; i++) {
      // LF -> CR
      if (clipboardTextMacRoman[i] == 10) {
        clipboardTextMacRoman[i] = 13;
      }
    }

    // Allocate space for new scrap in MacOS side
    M68kRegisters r;
    r.d[0] = clipboardTextMacRomanLength;
    Execute68kTrap(0xa71e, &r); // NewPtrSysClear()
    uint32 scrap_area = r.a[0];

    if (!scrap_area) {
      break;
    }
    uint8 *const data = Mac2HostAddr(scrap_area);
    memcpy(data, clipboardTextMacRoman, clipboardTextMacRomanLength);
    free(clipboardTextMacRoman);

    // Add new data to clipboard
    static uint8 proc[] = {
        0x59,          0x8f,                       // subq.l	#4,sp
        0xa9,          0xfc,                       // ZeroScrap()
        0x2f,          0x3c,           0, 0, 0, 0, // move.l	#length,-(sp)
        0x2f,          0x3c,           0, 0, 0, 0, // move.l	#type,-(sp)
        0x2f,          0x3c,           0, 0, 0, 0, // move.l	#outbuf,-(sp)
        0xa9,          0xfe,                       // PutScrap()
        0x58,          0x8f,                       // addq.l	#4,sp
        M68K_RTS >> 8, M68K_RTS & 0xff};
    r.d[0] = sizeof(proc);
    Execute68kTrap(0xa71e, &r); // NewPtrSysClear()
    uint32 proc_area = r.a[0];

    // The procedure is run-time generated because it must lays in
    // Mac address space. This is mandatory for "33-bit" address
    // space optimization on 64-bit platforms because the static
    // proc[] array is not remapped
    Host2Mac_memcpy(proc_area, proc, sizeof(proc));
    WriteMacInt32(proc_area + 6, clipboardTextMacRomanLength);
    WriteMacInt32(proc_area + 12, type);
    WriteMacInt32(proc_area + 18, scrap_area);
    we_put_this_data = true;
    Execute68k(proc_area, &r);

    // We are done with scratch memory
    r.a[0] = proc_area;
    Execute68kTrap(0xa01f, &r); // DisposePtr
    r.a[0] = scrap_area;
    Execute68kTrap(0xa01f, &r); // DisposePtr
  }
}

// ZeroScrap() is called before a Mac application writes to the clipboard;
// clears out the previous contents.
void ZeroScrap() { D(bug("ZeroScrap\n")); }

// Mac application wrote to clipboard
void PutScrap(uint32 type, void *scrap, int32 length) {
  D(bug("PutScrap type %08lx, data %08lx, length %ld\n", type, scrap, length));
  if (we_put_this_data) {
    we_put_this_data = false;
    return;
  }
  if (length <= 0) {
    return;
  }

  switch (type) {
  case FOURCC('T', 'E', 'X', 'T'):
    EM_ASM_({ workerApi.setClipboardText(UTF8ToString($0)); },
            macroman_to_utf8((char *)scrap, length));
    break;
  }
}
