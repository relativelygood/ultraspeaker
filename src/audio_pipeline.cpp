// ----------------
// File: src/audio_pipeline.cpp
// ----------------
#include "audio_pipeline.h"
#include "pwm_ultrasonic.h"
#include <BluetoothA2DPSink.h>

BluetoothA2DPSink a2dp;

static const int DECIM = 3; // 48kHz -> 16kHz
static int decim_cnt = 0;

static inline uint16_t process_sample(int16_t l, int16_t r) {
  int32_t mono = (l + r) >> 1;
  mono = abs(mono);
  mono >>= 1; // headroom
  if (mono > 32767) mono = 32767;
  return (uint16_t)((mono * 1023) >> 15);
}

void audio_data_callback(const uint8_t *data, uint32_t len) {
  const int16_t *pcm = (const int16_t *)data;
  uint32_t samples = len / 4;

  for (uint32_t i = 0; i < samples; i++) {
    if (++decim_cnt >= DECIM) {
      decim_cnt = 0;
      uint16_t env = process_sample(pcm[2*i], pcm[2*i+1]);
      pwm_set_envelope(env);
    }
  }
}

void audio_init() {
  a2dp.set_stream_reader(audio_data_callback, false);
  a2dp.start("Ultrasonic Speaker");
}
