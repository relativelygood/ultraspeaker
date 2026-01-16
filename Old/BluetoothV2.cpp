#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include "esp_bt.h"

BluetoothA2DPSink a2dp_sink;

void setup() {
    Serial.begin(115200);
    delay(1500);

    // Free BLE memory → required for A2DP stability
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    Serial.println("Starting A2DP sink...");
    a2dp_sink.start("ESP32 Audio Test");
}

void loop() {}