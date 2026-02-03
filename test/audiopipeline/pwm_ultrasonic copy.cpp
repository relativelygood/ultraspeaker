// ----------------
// File: src/pwm_ultrasonic.cpp
// ----------------
#include "pwm_ultrasonic.h"

static const int PWM_PIN = 18;
static const int PWM_CH  = 0;
static const int PWM_RES = 10;
static const int FC      = 40000;
static const int FS_ENV  = 16000;

static const int PWM_MAX = (1 << PWM_RES) - 1;
static const int DUTY_MIN = (PWM_MAX * 12) / 100;
static const int DUTY_MAX = (PWM_MAX * 82) / 100;

static hw_timer_t *timer = nullptr;
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static volatile uint16_t env_sample = 0;

static inline uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);

  uint32_t duty = DUTY_MIN + (env_sample * (DUTY_MAX - DUTY_MIN)) / PWM_MAX;
  duty = clamp_u16(duty, DUTY_MIN, DUTY_MAX);
  // Serial.println(duty);
  ledcWrite(PWM_CH, duty);

  portEXIT_CRITICAL_ISR(&timerMux);
}

void pwm_set_envelope(uint16_t env) {
  env_sample = env;
}

void pwm_init() {
  ledcSetup(PWM_CH, FC, PWM_RES);
  ledcAttachPin(PWM_PIN, PWM_CH);
  ledcWrite(PWM_CH, (DUTY_MIN + DUTY_MAX) / 2);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000 / FS_ENV, true);
  timerAlarmEnable(timer);
}
