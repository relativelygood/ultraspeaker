// ----------------
// File: src/audio_pipeline.cpp
// ----------------
#include "audio_pipeline.h"
#include "pwm_ultrasonic.h"
#include <BluetoothA2DPSink.h>

BluetoothA2DPSink a2dp;

static const int DECIM = 3;   // 48 kHz → 16 kHz
static int decim_cnt = 0;

// 1-pole low-pass filter state
static int32_t lp = 0;

// Cutoff ≈ 7–8 kHz at 48 kHz fs
// alpha = 1/8 → (>>3)
static inline int16_t process_sample(int16_t l, int16_t r) {
  // Stereo → mono
  int32_t mono = (l + r) >> 1;   // -32768 .. +32767

  // Low-pass filter (anti-alias + smoothing)
  lp += (mono - lp) >> 3;

  // Scale down (headroom)
  int32_t audio = lp >> 1;       // ~±16000

  // Add DC bias for AM (CRITICAL)
  audio += 16384;                // 0 .. 32767

  // Clamp
  if (audio < 0) audio = 0;
  if (audio > 32767) audio = 32767;

  return (int16_t)audio;
}

void audio_data_callback(const uint8_t *data, uint32_t len) {
  const int16_t *pcm = (const int16_t *)data;
  uint32_t frames = len / 4; // stereo 16-bit

  for (uint32_t i = 0; i < frames; i++) {
    int16_t mod = process_sample(pcm[2*i], pcm[2*i + 1]);

    // Proper decimation (after filtering)
    if (++decim_cnt >= DECIM) {
      decim_cnt = 0;

      // Scale 0..32767 → 0..PWM_MAX (1023)
      uint16_t pwm_env = (uint16_t)((mod * 1023) >> 15);
      pwm_set_envelope(pwm_env);
    }
  }
}


void audio_init() {
  a2dp.set_stream_reader(audio_data_callback, false);
  a2dp.start("Ultrasonic Speaker");
}
