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
static uint8* sound_buffer = NULL;

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

  // The audio worklet API processes things in 128 frame chunks. Have some
  // buffer to make sure we don't starve it, but don't buffer too much either,
  // to avoid latency. Use a larger one for SheepShaver, since it seems to
  // invoke the audio interrupt less frequently.
#ifdef SHEEPSHAVER
  audio_frames_per_block = 1024;
#else
  audio_frames_per_block = 384;
#endif

  int opt_sr = (audio_sample_rates[audio_sample_rate_index] >> 16);
  int opt_ss = audio_sample_sizes[audio_sample_size_index];
  int opt_ch = audio_channel_counts[audio_channel_count_index];

  EM_ASM_({ workerApi.didOpenAudio($0, $1, $2, $3, $4); }, opt_sr, opt_ss, opt_ch);

  sound_buffer_size = (audio_sample_sizes[audio_sample_size_index] >> 3) *
                      audio_channel_counts[audio_channel_count_index] * audio_frames_per_block;
  if (sound_buffer) {
    free(sound_buffer);
  }
  sound_buffer = (uint8 *)malloc(sound_buffer_size);
  set_audio_status_format();

  audio_open = true;
  return true;
}

static void close_audio(void) {
  audio_open = false;
  if (sound_buffer) {
    free(sound_buffer);
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
    printf("  work_size (%d) > sound_buffer_size (%d), truncating\n", work_size, sound_buffer_size);
    work_size = sound_buffer_size;
  }

  uint8* mac_sound_buffer = Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer));
  memcpy(sound_buffer, mac_sound_buffer, work_size);
  EM_ASM_({  workerApi.enqueueAudio($0, $1); }, sound_buffer, work_size);
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
  if (InterruptFlags & INTFLAG_AUDIO) {
    // Already have an audio interrupt pending, don't need to request another
    // one.
    return;
  }
  // Make sure that any buffered audio is drained even after we're normally done
  // playing.
  static int close_grace_period = 0;
  if (!AudioStatus.num_sources) {
    if (close_grace_period == 0) {
      return;
    }
    close_grace_period--;
    SetInterruptFlag(INTFLAG_AUDIO);
    return;
  }
  close_grace_period = 10;

  // Let the JS side get ahead a bit, in case we can't feed it fast enough.
  int jsAudioBufferSize = EM_ASM_INT_V({ return workerApi.audioBufferSize(); });
  if (jsAudioBufferSize < sound_buffer_size * 4) {
    SetInterruptFlag(INTFLAG_AUDIO);
  }
}
