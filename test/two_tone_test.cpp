#include <Arduino.h>
#include <math.h>

// ---------------- User settings ----------------
static const int PWM_PIN = 18;          // pick a valid output GPIO
static const int PWM_CH  = 0;           // LEDC channel 0..15
static const int PWM_RES = 10;          // bits (8-10 recommended at 40k)
static const int FC      = 40000 ;        // carrier frequency (Hz)
static const int FS_ENV  = 40000;        // envelope update rate (Hz)

// Switch between these two sine envelope frequencies:
static const int F_TONE_A = 400;       // Hz
static const int F_TONE_B = 400;        // Hz

// How often to switch between A and B:
static const uint32_t SWITCH_MS = 1000; // ms
// ------------------------------------------------

static const int PWM_MAX = (1 << PWM_RES) - 1;

// Limit duty swing (helps avoid extreme drive / distortion)
static const int DUTY_MIN = (PWM_MAX * 12) / 100;  // 10%
static const int DUTY_MAX = (PWM_MAX * 82) / 100;  // 90%

// Sine LUT for the envelope
static const int LUT_SIZE = 256;
static uint16_t lut[LUT_SIZE];

// Phase accumulator for LUT indexing (fixed-point)
static volatile uint32_t phase = 0;
static volatile uint32_t phase_inc = 0;

// Hardware timer
hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Very small clamp helper
static inline uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

// Compute phase increment for a desired envelope tone frequency.
// phase_inc = LUT_SIZE * F_TONE / FS_ENV in Q16.16
static inline uint32_t calcPhaseInc(uint32_t f_tone_hz) {
  return (uint32_t)(((uint64_t)LUT_SIZE * (uint64_t)f_tone_hz << 16) / (uint64_t)FS_ENV);
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
}

void setup() {
  buildLUT();

  // Start on tone A
  phase_inc = calcPhaseInc(F_TONE_A);

  // LEDC setup: carrier PWM
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
  static bool useA = true;
  static uint32_t lastSwitch = 0;

  uint32_t now = millis();
  if (now - lastSwitch >= SWITCH_MS) {
    lastSwitch = now;
    useA = !useA;

    uint32_t newInc = calcPhaseInc(useA ? F_TONE_A : F_TONE_B);

    // Update safely relative to ISR
    portENTER_CRITICAL(&timerMux);
    phase_inc = newInc;

    // Optional: reset phase at the switch so it restarts cleanly
    // phase = 0;
    portEXIT_CRITICAL(&timerMux);
  }

  delay(1);
}
