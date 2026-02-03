#include <Arduino.h>
#include <BluetoothA2DPSink.h>

// ======================= User settings =======================
static const int PWM_PIN = 18;
static const int PWM_CH  = 0;
static const int PWM_RES = 9;

static const int FC      = 40000;   // ultrasonic carrier (LEDC base freq)
static const int FS_ENV  = 40000;   // envelope sample rate

// Duty limits
static const int PWM_MAX  = (1 << PWM_RES) - 1;
static const int DUTY_MIN = (PWM_MAX * 1) / 100;
static const int DUTY_MAX = (PWM_MAX * 99) / 100;

// ======================= Globals ============================
BluetoothA2DPSink a2dp;

// Simple mono sample ring buffer (int16_t)
static const uint32_t RB_SIZE = 512;
static volatile int16_t rb_data[RB_SIZE];
static volatile uint32_t rb_head = 0;  // write index
static volatile uint32_t rb_tail = 0;  // read index

// HW timer
hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// ======================= Utility ============================
static inline uint16_t clamp_u16(uint32_t x, uint16_t lo, uint16_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return (uint16_t)x;
}

static inline bool rb_is_full() {
  uint32_t next = (rb_head + 1) & (RB_SIZE - 1);
  return next == rb_tail;
}

static inline bool rb_is_empty() {
  return rb_head == rb_tail;
}

// push from callback
static inline void rb_push(int16_t v) {
  uint32_t h = rb_head;
  uint32_t next = (h + 1) & (RB_SIZE - 1);
  if (next == rb_tail) {
    // overflow: drop sample
    return;
  }
  rb_data[h] = v;
  rb_head = next;
}

// pop from ISR
static inline int16_t rb_pop_or_last(int16_t last) {
  if (rb_is_empty()) return last;
  uint32_t t = rb_tail;
  int16_t v = rb_data[t];
  rb_tail = (t + 1) & (RB_SIZE - 1);
  return v;
}

// ======================= PWM ISR ============================
void IRAM_ATTR onTimer() {
  static int16_t last_sample = 0;

  portENTER_CRITICAL_ISR(&timerMux);

  // Get next mono sample (or reuse last if buffer empty)
  int16_t s = rb_pop_or_last(last_sample);
  last_sample = s;

  // Scale from signed 16-bit to 0..32767 (envelope)
  int32_t interp = s;             // -32768..32767
  interp >>= 1;                   // headroom
  interp += 16384;                // DC bias to center
  if (interp < 0)     interp = 0;
  if (interp > 32767) interp = 32767;

  uint32_t duty =
      DUTY_MIN +
      ((uint32_t)interp * (DUTY_MAX - DUTY_MIN)) / 32767;

  ledcWrite(PWM_CH, clamp_u16(duty, DUTY_MIN, DUTY_MAX));

  portEXIT_CRITICAL_ISR(&timerMux);
}

// ================== Bluetooth audio callback =================
void audio_data_callback(const uint8_t *data, uint32_t len) {
  const int16_t *pcm = (const int16_t *)data;
  uint32_t frames = len / 4; // stereo 16-bit

  // static int32_t lp = 0;     // simple LPF state

  for (uint32_t i = 0; i < frames; i++) {
    int32_t l = pcm[2 * i];
    // int32_t r = pcm[2 * i + 1];

    // int32_t mono = (l + r) >> 1;

    // simple lowpass to tame aliasing
    //lp += (mono - lp) >> 3;
    //int16_t out = (int16_t)lp;

    portENTER_CRITICAL(&timerMux);
    rb_push(l);
    portEXIT_CRITICAL(&timerMux);
  }
}

// ========================== Setup ============================
void setup() {
  Serial.begin(115200);
  delay(500);

  // PWM carrier
  ledcSetup(PWM_CH, FC, PWM_RES);
  ledcAttachPin(PWM_PIN, PWM_CH);
  ledcWrite(PWM_CH, (DUTY_MIN + DUTY_MAX) / 2);

  // Timer @ FS_ENV Hz
  // APB = 80 MHz -> divider 80 => 1 MHz tick
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000 / FS_ENV, true);
  timerAlarmEnable(timer);

  // Bluetooth A2DP sink, raw PCM callback
  a2dp.set_stream_reader(audio_data_callback, false);
  a2dp.start("Ultrasonic Speaker");
}

// =========================== Loop ============================
void loop() {
  delay(1000);
}
