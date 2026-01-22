#include <Arduino.h>
#include <AudioTools.h>          // First
#include <BluetoothA2DPSink.h>
#include "esp_bt.h"

using namespace audio_tools;

I2SStream i2s;                   // AudioTools I2S output
BluetoothA2DPSink a2dp_sink(i2s); // Required constructor

void audio_data_callback(const uint8_t *data, uint32_t length) {
    int32_t sum = 0;
    int samples = length / 2; // 16-bit PCM
    const int16_t *pcm = (const int16_t *)data;

    for (int i = 0; i < samples; i++) {
        sum += abs(pcm[i]);
    }

    int32_t avg = sum / samples;
    if (avg > 5) {
        Serial.println("🔊 Audio present");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    // Configure I2S for your Freenove board (adjust pins if needed)
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    i2s.begin(cfg);

    a2dp_sink.set_stream_reader(audio_data_callback, false);
    a2dp_sink.start("ESP32 Audio Test");
}

void loop() {}
// This code sets up an ESP32 as a Bluetooth A2DP sink using the AudioTools library. It configures the I2S output for audio playback and defines a callback function to analyze incoming audio data for audio presence, printing a message to the Serial Monitor when audio is detected.