#include "sysdeps.h"

#include "clip.h"
#include "macos_util.h"
#include "macroman.h"

#define DEBUG 1
#include "debug.h"

void ClipInit(void) { D(bug("ClipInit\n")); }

void ClipExit(void) { D(bug("ClipExit\n")); }

// Mac application reads clipboard
void GetScrap(void **handle, uint32 type, int32 offset) {
  D(bug("GetScrap handle %p, type %08x, offset %d\n", handle, type, offset));
}

// ZeroScrap() is called before a Mac application writes to the clipboard;
// clears out the previous contents.
void ZeroScrap() { D(bug("ZeroScrap\n")); }

// Mac application wrote to clipboard
void PutScrap(uint32 type, void *scrap, int32 length) {
  D(bug("PutScrap type %08lx, data %08lx, length %ld\n", type, scrap, length));
  if (length <= 0)
    return;

  switch (type) {
  case FOURCC('T', 'E', 'X', 'T'):
    D(bug(" clipping TEXT\n"));
    D(bug(" '%s'\n", macroman_to_utf8((char *)scrap, length)));
    break;
  }
}
