#include "sysdeps.h"

#include <emscripten.h>

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

class JS_monitor_desc : public monitor_desc {
 public:
  JS_monitor_desc(const vector<video_mode>& available_modes,
                  video_depth default_depth,
                  uint32 default_id)
      : monitor_desc(available_modes, default_depth, default_id) {}
  ~JS_monitor_desc() {}

  virtual void switch_to_current_mode();
  virtual void set_palette(uint8* pal, int num);

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
  const video_mode& mode = get_current_mode();
  D(bug("Current video mode:\n"));
  D(bug(" %dx%d (ID %02x), %d bpp\n", mode.x, mode.y, mode.resolution_id,
        1 << (mode.depth & 0x0f)));

  mac_framebuffer_size = mode.bytes_per_row * mode.y;
  mac_framebuffer = (uint8*)malloc(mac_framebuffer_size);
  if (mac_framebuffer == NULL) {
    return false;
  }

  set_mac_frame_base(MacFrameBaseMac);
  MacFrameBaseHost = mac_framebuffer;
  MacFrameSize = mode.bytes_per_row * mode.y;
  MacFrameLayout = FLAYOUT_DIRECT;
  InitFrameBufferMapping();

  VisualFormat visualFormat;
  visualFormat.depth = 32;
  // Magic values to force a Blit_Copy_Raw in 32-bit mode.
  visualFormat.Rmask = 0xff00;
  visualFormat.Gmask = 0xff0000;
  visualFormat.Bmask = 0xff000000;

  is_raw_screen_blit = !Screen_blitter_init(visualFormat, true, 1 << (mode.depth & 0x0f));
  D(bug(" is_raw_screen_blit=%s\n", is_raw_screen_blit ? "true" : "false"));
  browser_framebuffer_size = mode.y * 4 * mode.x;
  browser_framebuffer = (uint8*)malloc(browser_framebuffer_size);
  if (browser_framebuffer == NULL) {
    return false;
  }

  EM_ASM_({ workerApi.didOpenVideo($0, $1); }, mode.x, mode.y);

  return true;
}

void JS_monitor_desc::video_close() {
  D(bug("video_close()\n"));
  free(mac_framebuffer);
  free(browser_framebuffer);
}

void JS_monitor_desc::video_blit() {
  const video_mode& mode = get_current_mode();

  uint64 hash = SpookyHash::Hash64(mac_framebuffer, mode.x * mode.y * 4, 0);
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
  const video_mode& mode = get_current_mode();

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
  }
}

bool VideoInit(bool classic) {
  D(bug("VideoInit(classic=%s)\n", classic ? "true" : "false"));

  // Determine display type and default dimensions. "classic" is supposed to
  // default to a 512x342 1-bit display, but we ignore that.
  int default_width = 640;
  int default_height = 480;
  int default_depth = VIDEO_DEPTH_32BIT;
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
              {512, 384, 0x80},
              {640, 480, 0x81},
              {800, 600, 0x82},
              {1024, 768, 0x83},
              {1152, 870, 0x84},
              {1280, 1024, 0x85},
              {1600, 1200, 0x86},
              {
                  0,
              }};

  static vector<video_mode> video_modes;

  for (int i = 0; defs[i].w != 0; i++) {
    const int w = defs[i].w;
    const int h = defs[i].h;
    if (i > 0 && (w >= default_width || h >= default_height)) {
      continue;
    }
    for (int d = VIDEO_DEPTH_1BIT; d <= default_depth; d++) {
      video_mode mode;
      mode.x = w;
      mode.y = h;
      mode.resolution_id = defs[i].resolution_id;
      mode.bytes_per_row = TrivialBytesPerRow(w, (video_depth)d);
      mode.depth = (video_depth)d;
      mode.user_data = 0;
      video_modes.push_back(mode);
    }
  }

  JS_monitor_desc* monitor =
      new JS_monitor_desc(video_modes, (video_depth)default_depth, video_modes[0].resolution_id);
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
