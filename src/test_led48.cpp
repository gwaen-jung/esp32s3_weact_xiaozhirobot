/**
 * TEST LED GPIO48 (WS2812 onboard)
 * - Bat LED trang nhe tren GPIO 48
 * - Tat / float tat ca chan con lai
 * env: test_led48
 */
#include <Arduino.h>

#define LED_PIN   48
#define LED_R     20    // do sang: 0-255 (giu thap de khong choi mat)
#define LED_G     20
#define LED_B     20

void setup() {
  Serial.begin(115200);

  // -- Dua moi GPIO ve INPUT (floating) de khong gay xung la --
  // Bo qua: 0 (BOOT btn), 19/20 (USB D-/D+), 26-32 (flash), 35-37 (OPI PSRAM)
  const int safe_gpios[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18,
    21, 33, 34, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47
  };
  for (int pin : safe_gpios) {
    pinMode(pin, INPUT);
  }

  // -- Bat LED WS2812 tren GPIO 48 --
  // neopixelWrite() co san trong Arduino ESP32 core (khong can them lib)
  neopixelWrite(LED_PIN, LED_R, LED_G, LED_B);

  Serial.println("LED GPIO48 ON (R=20, G=20, B=20) - moi GPIO khac la INPUT");
}

void loop() {
  // Khong lam gi them - LED giu nguyen
  delay(1000);
}
