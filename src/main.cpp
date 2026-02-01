// ----------------
// File: src/main.cpp
// ----------------
#include <Arduino.h>
#include "audio_pipeline.h"
#include "pwm_ultrasonic.h"

void setup() {
  Serial.begin(115200);

  pwm_init();
  audio_init();
}

void loop() {
  // Everything runs via callbacks + ISR
  delay(1000);
}
