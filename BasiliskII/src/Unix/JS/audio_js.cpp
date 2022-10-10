#include "sysdeps.h"

#include <emscripten.h>

#include "audio.h"
#include "audio_defs.h"
#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"

#define DEBUG 0
#include "debug.h"

// The currently selected audio parameters (indices in audio_sample_rates[] etc. vectors)
static int audio_sample_rate_index = 0;
static int audio_sample_size_index = 0;
static int audio_channel_count_index = 0;

// Global variables
static int sound_buffer_size;  // Size of sound buffer in bytes
static int16* sound_buffer = NULL;

// Set AudioStatus to reflect current audio stream format
static void set_audio_status_format(void) {
  AudioStatus.sample_rate = audio_sample_rates[audio_sample_rate_index];
  AudioStatus.sample_size = audio_sample_sizes[audio_sample_size_index];
  AudioStatus.channels = audio_channel_counts[audio_channel_count_index];
}

static bool open_audio(void) {
  // JS supports a variety of twisted little audio formats, all different
  if (audio_sample_sizes.empty()) {
    // The reason we do this here is that we don't want to add sample
    // rates etc. unless the JS connection could be opened
    // (if JS fails, dsp might be tried next)
    // audio_sample_rates.push_back(11025 << 16);
    audio_sample_rates.push_back(22050 << 16);
    // audio_sample_rates.push_back(44100 << 16);
    // audio_sample_sizes.push_back(8);
    audio_sample_sizes.push_back(16);
    audio_channel_counts.push_back(1);
    // audio_channel_counts.push_back(2);

    // Default to highest supported values
    audio_sample_rate_index = audio_sample_rates.size() - 1;
    audio_sample_size_index = audio_sample_sizes.size() - 1;
    audio_channel_count_index = audio_channel_counts.size() - 1;
  }

  // Sound buffer size = 4096 frames
  audio_frames_per_block = 4096;

  int opt_sr = (audio_sample_rates[audio_sample_rate_index] >> 16);
  int opt_ss = audio_sample_sizes[audio_sample_size_index];
  int opt_ch = audio_channel_counts[audio_channel_count_index];

  EM_ASM_({ workerApi.openAudio($0, $1, $2, $3); }, opt_sr, opt_ss, opt_ch, audio_frames_per_block);

  sound_buffer_size = (audio_sample_sizes[audio_sample_size_index] >> 3) *
                      audio_channel_counts[audio_channel_count_index] * audio_frames_per_block;
  if (sound_buffer) {
    delete[] sound_buffer;
  }
  sound_buffer = new int16[sound_buffer_size / 2];
  set_audio_status_format();

  audio_open = true;
  return true;
}

static void close_audio(void) {
  audio_open = false;
  if (sound_buffer) {
    delete[] sound_buffer;
    sound_buffer = NULL;
  }
}

void AudioInit(void) {
  // Init audio status (reasonable defaults) and feature flags
  AudioStatus.sample_rate = 44100 << 16;
  AudioStatus.sample_size = 16;
  AudioStatus.channels = 2;
  AudioStatus.mixer = 0;
  AudioStatus.num_sources = 0;
  audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;

  // Sound disabled in prefs? Then do nothing
  if (PrefsFindBool("nosound")) {
    return;
  }

  // Open and initialize audio device
  open_audio();
}

void AudioExit(void) {
  // Close audio device
  close_audio();
}

void audio_enter_stream() {
  // No-op
}

void audio_exit_stream() {
  // No-op
}

/*
 *  MacOS audio interrupt, read next data block
 */
void AudioInterrupt(void) {
  // Get data from apple mixer
  if (!AudioStatus.mixer) {
    WriteMacInt32(audio_data + adatStreamInfo, 0);
    return;
  }

  M68kRegisters r;
  r.a[0] = audio_data + adatStreamInfo;
  r.a[1] = AudioStatus.mixer;
  Execute68k(audio_data + adatGetSourceData, &r);
  D(bug(" GetSourceData() returns %08lx\n", r.d[0]));

  uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
  if (!apple_stream_info) {
    return;
  }
  int sample_count = ReadMacInt32(apple_stream_info + scd_sampleCount);
  if (sample_count == 0) {
    return;
  }
  int work_size = sample_count * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
  D(bug("stream: work_size %d\n", work_size));
  if (work_size > sound_buffer_size) {
    D(bug("  work_size (%d) > sound_buffer_size (%d), truncating\n", work_size, sound_buffer_size));
    work_size = sound_buffer_size;
  }

  int16* p = (int16*)Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer));
  for (int i = 0; i < work_size / 2; i++) {
    sound_buffer[i] = ntohs(p[i]);
  }
  // Fill remaining buffer with silence.
  memset((uint8*)sound_buffer + work_size, 0, sound_buffer_size - work_size);
  EM_ASM_INT({ return workerApi.enqueueAudio($0, $1); }, sound_buffer, sound_buffer_size);
  D(bug("stream: data written\n"));
}

bool audio_set_sample_rate(int index) {
  close_audio();
  audio_sample_rate_index = index;
  return open_audio();
}

bool audio_set_sample_size(int index) {
  close_audio();
  audio_sample_size_index = index;
  return open_audio();
}

bool audio_set_channels(int index) {
  close_audio();
  audio_channel_count_index = index;
  return open_audio();
}

bool audio_get_main_mute(void) {
  return false;
}

/*
 *  Get/set volume controls (volume values received/returned have the left channel
 *  volume in the upper 16 bits and the right channel volume in the lower 16 bits;
 *  both volumes are 8.8 fixed point values with 0x0100 meaning "maximum volume"))
 */
uint32 audio_get_main_volume(void) {
  return 0x01000100;
}

bool audio_get_speaker_mute(void) {
  return false;
}

uint32 audio_get_speaker_volume(void) {
  return 0x01000100;
}

void audio_set_main_mute(bool mute) {
  // TODO
}

void audio_set_main_volume(uint32 vol) {
  // TODO
}

void audio_set_speaker_mute(bool mute) {
  // TODO
}

void audio_set_speaker_volume(uint32 vol) {
  // TODO
}

void AudioRefresh() {
  if (AudioStatus.num_sources) {
    SetInterruptFlag(INTFLAG_AUDIO);
  }
}
