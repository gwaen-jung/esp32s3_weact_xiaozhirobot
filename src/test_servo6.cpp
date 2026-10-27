/**
 * TEST CHỈ SERVO GPIO 6
 * - Quét 0° -> 180° -> 0° lặp lại
 * - In góc ra Serial (115200 baud)
 * - Không động gì đến các GPIO khác
 * env: test_servo6
 */
#include <Arduino.h>

#define SERVO_PIN   6
#define LEDC_CH     0
#define LEDC_FREQ   50      // Hz
#define LEDC_BITS   14      // 14-bit duty

// 50Hz, 14-bit: period = 20ms = 16384 ticks
// 0°   -> 0.5ms pulse -> duty = 0.5/20 * 16384 = 410
// 180° -> 2.4ms pulse -> duty = 2.4/20 * 16384 = 1966
#define DUTY_MIN    410
#define DUTY_MAX    1966

int angleToDuty(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, DUTY_MIN, DUTY_MAX);
}

void setAngle(int angle) {
  ledcWrite(LEDC_CH, angleToDuty(angle));
  Serial.printf("GPIO6 servo -> %d deg (duty=%d)\n", angle, angleToDuty(angle));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== TEST SERVO GPIO 6 ===");
  Serial.println("Quet 0 -> 90 -> 180 -> 90 -> 0, lap lai.");
  Serial.println("Gui so (0-180) qua Serial de dat goc cu the.");

  ledcSetup(LEDC_CH, LEDC_FREQ, LEDC_BITS);
  ledcAttachPin(SERVO_PIN, LEDC_CH);

  // Bat dau o 90 do (trung tinh)
  setAngle(90);
  delay(1000);
}

static String serialBuf = "";

void loop() {
  // --- Doc lenh tu Serial ---
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuf.length() > 0) {
        int deg = serialBuf.toInt();
        deg = constrain(deg, 0, 180);
        setAngle(deg);
        serialBuf = "";
      }
    } else {
      serialBuf += c;
    }
  }

  // --- Quet tu dong ---
  static bool sweeping = true;
  static int  angle    = 90;
  static int  dir      = 1;   // 1 = tang, -1 = giam
  static unsigned long lastMove = 0;

  if (Serial.available() == 0 && millis() - lastMove >= 25) {
    angle += dir * 2;
    if (angle >= 180) { angle = 180; dir = -1; }
    if (angle <= 0)   { angle = 0;   dir =  1; }
    setAngle(angle);
    lastMove = millis();
  }
}
