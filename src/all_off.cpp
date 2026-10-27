/**
 * TAT TOAN BO ESP32-S3
 * - Tat PWM tat ca kenh LEDC
 * - Set moi GPIO ve INPUT (floating) - khong co xung ra
 * - WS2812 GPIO48 tat (mau den)
 * env: all_off
 */
#include <Arduino.h>
#include <driver/ledc.h>

void setup() {
  Serial.begin(115200);

  // Tat tat ca 8 kenh LEDC (tranh servo con nhan xung cu)
  for (int ch = 0; ch < 8; ch++) {
    ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)ch, 0);
  }

  // Tat LED WS2812 GPIO48
  neopixelWrite(48, 0, 0, 0);

  // Dua tat ca GPIO an toan ve INPUT floating
  const int gpios[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18,
    21, 33, 34, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48
  };
  for (int pin : gpios) {
    pinMode(pin, INPUT);
  }

  Serial.println("=== ALL OFF ===");
  Serial.println("LEDC: tat. GPIO: INPUT. LED48: den.");
}

void loop() {
  delay(5000);
}
