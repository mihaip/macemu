#include "sysdeps.h"

#include <emscripten.h>

#include <vector>

#include "SpookyV2.h"
#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "video.h"
#include "video_blit.h"
#include "video_defs.h"

#define DEBUG 0
#include "debug.h"

/*
 *  SheepShaver glue
 */

#ifdef SHEEPSHAVER

using std::vector;

// Color depth modes type
typedef int video_depth;

// 1, 2, 4 and 8 bit depths use a color palette
static inline bool IsDirectMode(VIDEO_MODE const& mode) {
  return IsDirectMode(mode.viAppleMode);
}

// Abstract base class representing one (possibly virtual) monitor
// ("monitor" = rectangular display with a contiguous frame buffer)
class monitor_desc {
 public:
  monitor_desc(const vector<VIDEO_MODE>& available_modes,
               video_depth default_depth,
               uint32 default_id) {}
  virtual ~monitor_desc() {}

  // Get current Mac frame buffer base address
  uint32 get_mac_frame_base(void) const { return screen_base; }

  // Set Mac frame buffer base address (called from switch_to_mode())
  void set_mac_frame_base(uint32 base) { screen_base = base; }

  // Get current video mode
  const VIDEO_MODE& get_current_mode(void) const { return VModes[cur_mode]; }

  // Called by the video driver to switch the video mode on this display
  // (must call set_mac_frame_base())
  virtual void switch_to_current_mode(void) = 0;

  // Called by the video driver to set the color palette (in indexed modes)
  // or the gamma table (in direct modes)
  virtual void set_palette(uint8* pal, int num) = 0;
};

// Vector of pointers to available monitor descriptions, filled by VideoInit()
static vector<monitor_desc*> VideoMonitors;

// Find Apple mode matching best specified dimensions
static int find_apple_resolution(int xsize, int ysize) {
  if (xsize == 640 && ysize == 480)
    return APPLE_640x480;
  if (xsize == 800 && ysize == 600)
    return APPLE_800x600;
  if (xsize == 1024 && ysize == 768)
    return APPLE_1024x768;
  if (xsize == 1152 && ysize == 768)
    return APPLE_1152x768;
  if (xsize == 1152 && ysize == 900)
    return APPLE_1152x900;
  if (xsize == 1280 && ysize == 1024)
    return APPLE_1280x1024;
  if (xsize == 1600 && ysize == 1200)
    return APPLE_1600x1200;
  return APPLE_CUSTOM;
}

// Display error alert
static void ErrorAlert(int error) {
  ErrorAlert(GetString(error));
}

#endif

class JS_monitor_desc : public monitor_desc {
 public:
  JS_monitor_desc(const vector<VIDEO_MODE>& available_modes,
                  video_depth default_depth,
                  uint32 default_id)
      : monitor_desc(available_modes, default_depth, default_id),
        last_blit_hash(0),
        last_palette_hash(0) {}
  ~JS_monitor_desc() {}

  virtual void switch_to_current_mode();
  virtual void set_palette(uint8* pal, int num);
  virtual void set_gamma(uint8* gamma, int num);

  bool video_open();
  void video_close();
  void video_blit();

 private:
  uint8* mac_framebuffer;
  size_t mac_framebuffer_size;
  uint8* browser_framebuffer;
  size_t browser_framebuffer_size;
  bool is_raw_screen_blit;
  uint64 last_blit_hash;
  uint64 last_palette_hash;
};

void JS_monitor_desc::switch_to_current_mode() {
  video_close();
  video_open();

  if (mac_framebuffer == NULL) {
    ErrorAlert(STR_OPEN_WINDOW_ERR);
    QuitEmulator();
  }
}

bool JS_monitor_desc::video_open() {
  D(bug("video_open()\n"));
  const VIDEO_MODE& mode = get_current_mode();
  D(bug("Current video mode:\n"));
  D(bug(" %dx%d (ID %02x), %d bpp\n", VIDEO_MODE_X, VIDEO_MODE_Y, VIDEO_MODE_RESOLUTION,
        1 << (VIDEO_MODE_DEPTH & 0x0f)));

  mac_framebuffer_size = VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y;
  mac_framebuffer = (uint8*)malloc(mac_framebuffer_size);
  if (mac_framebuffer == NULL) {
    return false;
  }

  set_mac_frame_base((unsigned int)Host2MacAddr((uint8*)mac_framebuffer));

  VisualFormat visualFormat;
  visualFormat.depth = 32;
  // Magic values to force a Blit_Copy_Raw in 32-bit mode.
  visualFormat.Rmask = 0xff00;
  visualFormat.Gmask = 0xff0000;
  visualFormat.Bmask = 0xff000000;

  is_raw_screen_blit = !Screen_blitter_init(visualFormat, true, 1 << (VIDEO_MODE_DEPTH & 0x0f));
  browser_framebuffer_size = VIDEO_MODE_Y * 4 * VIDEO_MODE_X;
  browser_framebuffer = (uint8*)malloc(browser_framebuffer_size);
  if (browser_framebuffer == NULL) {
    return false;
  }

  EM_ASM_({ workerApi.didOpenVideo($0, $1); }, VIDEO_MODE_X, VIDEO_MODE_Y);

  return true;
}

void JS_monitor_desc::video_close() {
  D(bug("video_close()\n"));
  free(mac_framebuffer);
  free(browser_framebuffer);
}

void JS_monitor_desc::video_blit() {
  const VIDEO_MODE& mode = get_current_mode();

  uint64 hash = SpookyHash::Hash64(mac_framebuffer, mac_framebuffer_size, 0);
  if (!is_raw_screen_blit) {
    hash ^= last_palette_hash;
  }
  if (hash == last_blit_hash) {
    // Screen has not changed, but we still let the JS know so that it can
    // keep track of screen refreshes when deciding how long to idle for.
    EM_ASM({ workerApi.blit(0, 0); });
    return;
  }
  last_blit_hash = hash;

  if (is_raw_screen_blit) {
    // Offset by 1 to go from ARGB to RGBA (we don't actually care about the
    // alpha, it should be the same for all pixels)
    memcpy(browser_framebuffer, mac_framebuffer + 1, mac_framebuffer_size - 1);
    // Ensure that alpha is set, the Mac defaults to it being 0, which the
    // browser renders as transparent.
    for (size_t i = 3; i < mac_framebuffer_size; i += 4) {
      browser_framebuffer[i] = 0xff;
    }
  } else {
    Screen_blit(browser_framebuffer, mac_framebuffer, mac_framebuffer_size);
  }
  EM_ASM_({ workerApi.blit($0, $1); }, browser_framebuffer, browser_framebuffer_size);
}

void JS_monitor_desc::set_palette(uint8* pal, int num_in) {
  const VIDEO_MODE& mode = get_current_mode();

  if (!IsDirectMode(mode)) {
    for (int i = 0; i < 256; i++) {
      // If there are less than 256 colors, we repeat the first entries
      // (this makes color expansion easier).
      int c = i & (num_in - 1);
      uint8 red = pal[c * 3 + 0];
      uint8 green = pal[c * 3 + 1];
      uint8 blue = pal[c * 3 + 2];
      ExpandMap[i] = red | green << 8 | blue << 16 | 0xff000000;
    }
    last_palette_hash = SpookyHash::Hash64(pal, num_in * 3, 0);
  }
}

void JS_monitor_desc::set_gamma(uint8* gamma, int num) {
  // Not implemented
}

#ifdef SHEEPSHAVER
bool VideoInit(void) {
  const bool classic = false;
#else
bool VideoInit(bool classic) {
#endif

  D(bug("VideoInit(classic=%s)\n", classic ? "true" : "false"));

  // Determine display type and default dimensions. "classic" is supposed to
  // default to a 512x342 1-bit display, but we ignore that.
  int default_width = 640;
  int default_height = 480;
  int default_depth = VIDEO_DEPTH_8BIT;
  const char* mode_str = PrefsFindString("screen");
  if (mode_str) {
    if (sscanf(mode_str, "win/%d/%d", &default_width, &default_height) != 2)
      D(bug("Could not parse `screen` preference value '%s`", mode_str));
  }

  struct {
    int w;
    int h;
    int resolution_id;
  } defs[] = {{default_width, default_height, 0x80},
#ifndef SHEEPSHAVER  // Omit Classic resolutions
              {512, 384, 0x80},
#endif
              {640, 480, 0x81},
              {800, 600, 0x82},
              {1024, 768, 0x83},
              {1152, 870, 0x84},
              {1280, 1024, 0x85},
              {1600, 1200, 0x86},
              {
                  0,
              }};

  static vector<VIDEO_MODE> video_modes;

  for (int i = 0; defs[i].w != 0; i++) {
    const int w = defs[i].w;
    const int h = defs[i].h;
    if (i > 0 && (w >= default_width || h >= default_height)) {
      continue;
    }
    for (int d = VIDEO_DEPTH_1BIT; d <= VIDEO_DEPTH_32BIT; d++) {
      VIDEO_MODE mode;
      VIDEO_MODE_X = w;
      VIDEO_MODE_Y = h;
#ifdef SHEEPSHAVER
      mode.viAppleID = find_apple_resolution(w, h);
      mode.viType = DIS_WINDOW;
#else
      mode.resolution_id = defs[i].resolution_id;
#endif
      VIDEO_MODE_ROW_BYTES = TrivialBytesPerRow(w, (video_depth)d);
      VIDEO_MODE_DEPTH = (video_depth)d;
      D(bug("adding mode with %dx%d (ID %02x, index: %d), %d bpp\n", VIDEO_MODE_X, VIDEO_MODE_Y,
            VIDEO_MODE_RESOLUTION, video_modes.size(), 1 << (VIDEO_MODE_DEPTH & 0x0f)));

      video_modes.push_back(mode);
    }
  }

  // Find requested default mode with specified dimensions
  bool found_default_mode = false;
  uint32 default_id;
#ifdef SHEEPSHAVER
  cur_mode = 0;
#endif
  for (auto& mode : video_modes) {
    if (VIDEO_MODE_X == default_width && VIDEO_MODE_Y == default_height &&
        VIDEO_MODE_DEPTH == default_depth) {
      default_id = VIDEO_MODE_RESOLUTION;
      found_default_mode = true;
      break;
    }
#ifdef SHEEPSHAVER
    cur_mode++;
#endif
  }
  if (!found_default_mode) {
    const VIDEO_MODE& mode = video_modes[0];
    default_depth = VIDEO_MODE_DEPTH;
    default_id = VIDEO_MODE_RESOLUTION;
#ifdef SHEEPSHAVER
    cur_mode = 0;
#endif
  }

#ifdef SHEEPSHAVER
  for (int i = 0; i < video_modes.size(); i++)
    VModes[i] = video_modes[i];
  VideoInfo* p = &VModes[video_modes.size()];
  p->viType = DIS_INVALID;  // End marker
  p->viRowBytes = 0;
  p->viXsize = p->viYsize = 0;
  p->viAppleMode = 0;
  p->viAppleID = 0;
#endif

  JS_monitor_desc* monitor =
      new JS_monitor_desc(video_modes, (video_depth)default_depth, default_id);
  VideoMonitors.push_back(monitor);

  return monitor->video_open();
}

void VideoExit() {
  D(bug("VideoExit()\n"));

  for (auto monitor : VideoMonitors) {
    dynamic_cast<JS_monitor_desc*>(monitor)->video_close();
  }
}

void VideoRefresh() {
  for (auto monitor : VideoMonitors) {
    dynamic_cast<JS_monitor_desc*>(monitor)->video_blit();
  }
}

void VideoInterrupt() {
  // No-op
}

void VideoQuitFullScreen() {
  // No-op
}

#ifdef SHEEPSHAVER

void VideoVBL(void) {
  VideoRefresh();

  // Execute video VBL
  if (private_data != NULL && private_data->interruptsEnabled)
    VSLDoInterruptService(private_data->vslServiceID);
}

int16 video_mode_change(VidLocals* csSave, uint32 ParamPtr) {
  D(bug("video_mode_change()\n"));
  /* return if no mode change */
  if ((csSave->saveData == ReadMacInt32(ParamPtr + csData)) &&
      (csSave->saveMode == ReadMacInt16(ParamPtr + csMode))) {
    D(bug("  ...unchanged (save data %x, save mode %x, cur_mode=%d)\n", csSave->saveData,
          csSave->saveMode, cur_mode));
    return noErr;
  }

  /* first find video mode in table */
  for (int i = 0; VModes[i].viType != DIS_INVALID; i++) {
    if ((ReadMacInt16(ParamPtr + csMode) == VModes[i].viAppleMode) &&
        (ReadMacInt32(ParamPtr + csData) == VModes[i].viAppleID)) {
      csSave->saveMode = ReadMacInt16(ParamPtr + csMode);
      csSave->saveData = ReadMacInt32(ParamPtr + csData);
      csSave->savePage = ReadMacInt16(ParamPtr + csPage);

      cur_mode = i;
      D(bug("  ...switched to mode %d\n", cur_mode));
      monitor_desc* monitor = VideoMonitors[0];
      monitor->switch_to_current_mode();

      WriteMacInt32(ParamPtr + csBaseAddr, screen_base);
      csSave->saveBaseAddr = screen_base;
      csSave->saveData = VModes[cur_mode].viAppleID; /* First mode ... */
      csSave->saveMode = VModes[cur_mode].viAppleMode;

      return noErr;
    }
  }
  return paramErr;
}

// Find palette size for given color depth
static int palette_size(int mode) {
  switch (mode) {
    case VIDEO_DEPTH_1BIT:
      return 2;
    case VIDEO_DEPTH_2BIT:
      return 4;
    case VIDEO_DEPTH_4BIT:
      return 16;
    case VIDEO_DEPTH_8BIT:
      return 256;
    case VIDEO_DEPTH_16BIT:
      return 32;
    case VIDEO_DEPTH_32BIT:
      return 256;
    default:
      return 0;
  }
}

void video_set_palette(void) {
  monitor_desc* monitor = VideoMonitors[0];
  int n_colors = palette_size(monitor->get_current_mode().viAppleMode);
  D(bug("video_set_palette(n_colors=%d)\n", n_colors));
  uint8 pal[256 * 3];
  for (int c = 0; c < n_colors; c++) {
    pal[c * 3 + 0] = mac_pal[c].red;
    pal[c * 3 + 1] = mac_pal[c].green;
    pal[c * 3 + 2] = mac_pal[c].blue;
  }
  monitor->set_palette(pal, n_colors);
}

bool video_can_change_cursor(void) {
  D(bug("video_can_change_cursor()\n"));
  return false;
}

void video_set_cursor(void) {
  D(bug("video_set_cursor()\n"));
}

void video_set_dirty_area(int x, int y, int w, int h) {
  D(bug("video_set_dirty_area(%d, %d, %d, %d)\n", x, y, w, h));
}

void video_set_gamma(int n_colors) {
  // TODO
}

#endif  // SHEEPSHAVER
