#include <Arduino.h>
#include <math.h>

// ---------------- User settings ----------------
static const int PWM_PIN = 18;          // pick a valid output GPIO
static const int PWM_CH  = 0;           // LEDC channel 0..15
static const int PWM_RES = 10;          // bits (8-10 recommended at 40k)
static const int FC      = 40000;       // carrier frequency (Hz)
static const int FS_ENV  = 20000;       // envelope update rate (Hz)
static const int F_TONE  = 1000;        // test envelope tone (Hz)
// ------------------------------------------------

static const int PWM_MAX = (1 << PWM_RES) - 1;

// Limit duty swing (helps avoid extreme drive / distortion)
static const int DUTY_MIN = (PWM_MAX * 10) / 100;  // 10%
static const int DUTY_MAX = (PWM_MAX * 90) / 100;  // 90%

// Sine LUT for the envelope
static const int LUT_SIZE = 256;
static uint16_t lut[LUT_SIZE];

// Phase accumulator for LUT indexing (fixed-point)
static volatile uint32_t phase = 0;
static uint32_t phase_inc = 0;

// Hardware timer
hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Very small clamp helper
static inline uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);

  phase += phase_inc;
  uint32_t idx = (phase >> 16) & (LUT_SIZE - 1);

  // lut[idx] is 0..PWM_MAX (unipolar). Map into [DUTY_MIN..DUTY_MAX]
  uint32_t x = lut[idx];
  uint32_t duty = DUTY_MIN + (x * (uint32_t)(DUTY_MAX - DUTY_MIN)) / (uint32_t)PWM_MAX;

  duty = clamp_u16((uint16_t)duty, DUTY_MIN, DUTY_MAX);

  ledcWrite(PWM_CH, duty);

  portEXIT_CRITICAL_ISR(&timerMux);
}

static void buildLUT() {
  for (int i = 0; i < LUT_SIZE; i++) {
    float s = sinf(2.0f * (float)M_PI * (float)i / (float)LUT_SIZE);
    float u = 0.5f * (s + 1.0f); // 0..1
    uint32_t v = (uint32_t)(u * (float)PWM_MAX + 0.5f);
    if (v > (uint32_t)PWM_MAX) v = (uint32_t)PWM_MAX;
    lut[i] = (uint16_t)v;
  }

  // phase_inc = LUT_SIZE * F_TONE / FS_ENV in Q16.16
  // (idx comes from phase>>16)
  phase_inc = (uint32_t)(((uint64_t)LUT_SIZE * (uint64_t)F_TONE << 16) / (uint64_t)FS_ENV);
}

void setup() {
  buildLUT();

  // LEDC setup: 40 kHz carrier PWM
  ledcSetup(PWM_CH, FC, PWM_RES);
  ledcAttachPin(PWM_PIN, PWM_CH);

  // Start at mid duty
  ledcWrite(PWM_CH, (DUTY_MIN + DUTY_MAX) / 2);

  // Timer: 1 tick = 1 us when prescaler = 80 on 80 MHz APB clock
  timer = timerBegin(0, 80, true);                  // timer 0, prescaler 80, count up
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000 / FS_ENV, true);   // period in us
  timerAlarmEnable(timer);
}

void loop() {
  // Nothing required; ISR updates duty continuously
  delay(1000);
}
