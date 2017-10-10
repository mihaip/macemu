/*
 *  browser_io.h
 */

#ifndef BROWSER_IO_H
#define BROWSER_IO_H

#include <vector>

#ifndef NO_STD_NAMESPACE
using std::vector;
#endif

extern void audio_enter_stream(void);
extern void audio_exit_stream(void);

extern bool audio_set_sample_rate(int index);
extern bool audio_set_sample_size(int index);
extern bool audio_set_channels(int index);

extern bool audio_get_main_mute(void);
extern uint32 audio_get_main_volume(void);
extern bool audio_get_speaker_mute(void);
extern uint32 audio_get_speaker_volume(void);
extern void audio_set_main_mute(bool mute);
extern void audio_set_main_volume(uint32 vol);
extern void audio_set_speaker_mute(bool mute);
extern void audio_set_speaker_volume(uint32 vol);

// Current audio status
struct audio_status {
  uint32 sample_rate;   // 16.16 fixed point
  uint32 sample_size;   // 8 or 16
  uint32 channels;    // 1 (mono) or 2 (stereo)
  uint32 mixer;     // Mac address of Apple Mixer
  int num_sources;    // Number of active sources
};
extern struct audio_status AudioStatus;

extern bool audio_open;         // Flag: audio is open and ready
extern int audio_frames_per_block;    // Number of audio frames per block
extern uint32 audio_component_flags;  // Component feature flags

extern vector<uint32> audio_sample_rates; // Vector of supported sample rates (16.16 fixed point)
extern vector<uint16> audio_sample_sizes; // Vector of supported sample sizes
extern vector<uint16> audio_channel_counts; // Array of supported channels counts

// Audio component global data and 68k routines
enum {
  adatDelegateCall = 0,   // 68k code to call DelegateCall()
  adatOpenMixer = 14,     // 68k code to call OpenMixerSoundComponent()
  adatCloseMixer = 36,    // 68k code to call CloseMixerSoundComponent()
  adatGetInfo = 54,     // 68k code to call GetInfo()
  adatSetInfo = 78,     // 68k code to call SetInfo()
  adatPlaySourceBuffer = 102, // 68k code to call PlaySourceBuffer()
  adatGetSourceData = 126,  // 68k code to call GetSourceData()
  adatStartSource = 146,    // 68k code to call StartSource()
  adatData = 168,       // SoundComponentData struct
  adatMixer = 196,      // Mac address of mixer, returned by adatOpenMixer
  adatStreamInfo = 200,   // Mac address of stream info, returned by adatGetSourceData
  SIZEOF_adat = 204
};

extern uint32 audio_data;   // Mac address of global data area

#endif
