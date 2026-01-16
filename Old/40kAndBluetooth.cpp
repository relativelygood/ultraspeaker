#include <Arduino.h>
#include <math.h>
#include "BluetoothA2DPSink.h"
#include "esp_bt.h"

// --- PWM & Carrier Settings ---
static const int PWM_PIN = 18;         // Output GPIO
static const int PWM_CH  = 0;          // LEDC channel
static const int PWM_RES = 10;         // 10-bit resolution
static const int FC      = 40000;      // 40kHz carrier frequency
static const int FS_ENV  = 20000;      // Envelope update rate (Hz)
static const int F_TONE  = 1000;       // Internal test tone (1kHz)

static const int PWM_MAX = (1 << PWM_RES) - 1;
static const int DUTY_MIN = (PWM_MAX * 10) / 100;
static const int DUTY_MAX = (PWM_MAX * 90) / 100;

// --- Global Variables ---
BluetoothA2DPSink a2dp_sink;
volatile bool audio_detected = false;
static const int LUT_SIZE = 256;
static uint16_t lut[LUT_SIZE];
static volatile uint32_t phase = 0;
static uint32_t phase_inc = 0;

hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// --- Helper Functions ---
static inline uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi) {
    return (x < lo) ? lo : (x > hi ? hi : x);
}

// --- Interrupt Service Routine (ISR) ---
// This runs at 20kHz to update the PWM duty cycle
void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);

    phase += phase_inc;
    uint32_t idx = (phase >> 16) & (LUT_SIZE - 1);

    // If Bluetooth audio is detected, modulate the wave. 
    // Otherwise, stay at a neutral 50% duty cycle or silent.
    uint32_t duty;
    if (audio_detected) {
        uint32_t x = lut[idx];
        duty = DUTY_MIN + (x * (uint32_t)(DUTY_MAX - DUTY_MIN)) / (uint32_t)PWM_MAX;
    } else {
        duty = (DUTY_MIN + DUTY_MAX) / 2; // Idle state
    }

    ledcWrite(PWM_CH, clamp_u16((uint16_t)duty, DUTY_MIN, DUTY_MAX));

    portEXIT_CRITICAL_ISR(&timerMux);
}

// --- Bluetooth Callback ---
void audio_data_callback(const uint8_t *data, uint32_t length) {
    int32_t sum = 0;
    int samples = length / 2; 
    const int16_t *pcm = (const int16_t *)data;

    for (int i = 0; i < samples; i++) {
        sum += abs(pcm[i]);
    }

    int32_t avg = sum / samples;

    // Threshold for detection
    if (avg > 30) {
        if (!audio_detected) Serial.println("🔊 Audio Stream Active");
        audio_detected = true;
    } else {
        audio_detected = false;
    }
}

static void buildLUT() {
    for (int i = 0; i < LUT_SIZE; i++) {
        float s = sinf(2.0f * (float)M_PI * (float)i / (float)LUT_SIZE);
        float u = 0.5f * (s + 1.0f); 
        uint32_t v = (uint32_t)(u * (float)PWM_MAX + 0.5f);
        lut[i] = (uint16_t)min((uint32_t)PWM_MAX, v);
    }
    phase_inc = (uint32_t)(((uint64_t)LUT_SIZE * (uint64_t)F_TONE << 16) / (uint64_t)FS_ENV);
}

void setup() {
    Serial.begin(115200);
    
    // 1. Initialize PWM hardware
    buildLUT();
    ledcSetup(PWM_CH, FC, PWM_RES);
    ledcAttachPin(PWM_PIN, PWM_CH);
    ledcWrite(PWM_CH, (DUTY_MIN + DUTY_MAX) / 2);

    // 2. Initialize Hardware Timer (Timer 0)
    timer = timerBegin(0, 80, true); 
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000000 / FS_ENV, true);
    timerAlarmEnable(timer);

    // 3. Initialize Bluetooth
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    a2dp_sink.set_stream_reader(audio_data_callback, false);
    a2dp_sink.start("ESP32_Ultasonic_BT");

    Serial.println("🚀 System Started: 40kHz Carrier + Bluetooth Ready");
}

void loop() {
    // Bluetooth and Timer run on interrupts/background cores.
    // Loop stays empty to keep CPU usage low for the BT stack.
    delay(1000);
}
