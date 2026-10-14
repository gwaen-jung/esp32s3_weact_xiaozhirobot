/**
 * ============================================================================
 * XIAOZHI ROBOT DOG - BÀI TEST TỔNG DUYỆT 4 CHÂN LẦN CUỐI (FINAL SUITE)
 * Môi trường: [env:test_servo] (PlatformIO)
 * MCU: WeAct Studio ESP32-S3 CoreBoard N16R8
 * Display: 2.4" SPI TFT LCD (ILI9341, 240x320)
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI 4 CHÂN SERVO:
 *   - FL (Trên Trái  - Front-Left):  GPIO 7 (LEDC Ch 0) - Servo 180°
 *   - FR (Trên Phải  - Front-Right): GPIO 4 (LEDC Ch 1) - [SERVO 360° ĐÃ XỬ LÝ]
 *   - RL (Dưới Trái  - Rear-Left):   GPIO 6 (LEDC Ch 2) - Servo 180°
 *   - RR (Dưới Phải  - Rear-Right):  GPIO 5 (LEDC Ch 3) - Servo 180°
 *   - Nguồn: 5V ngoài cấp chung GND với ESP32
 * 
 * TÍNH NĂNG ĐẶC BIỆT TRONG BÀI TEST CUỐI CÙNG:
 *   1. Chế độ TỰ ĐỘNG DIỄN (Auto Demo Cycle):
 *      Cún tự động diễn mượt mà các tư thế: STAND -> SIT -> WAVE -> CROUCH -> WIGGLE.
 *   2. Chế độ THỦ CÔNG (Manual Control):
 *      Gõ lệnh qua Serial ('stand', 'sit', 'wave', 'crouch', 'wiggle', 'all 90'...)
 *      hoặc nhấn nút BOOT để chuyển tư thế theo ý muốn.
 *   3. Điều khiển mượt mà Cosine S-curve chống giật và chống sụt áp nguồn.
 *   4. Chân số 4 (Servo 360) được điều khiển giữ điểm dừng (90°) và nhích theo nhịp,
 *      hoàn toàn không bị quay tròn tít mù.
 *   5. Giao diện mắt RoboEyes sống động biểu cảm theo từng tư thế trên màn hình LCD 2.4".
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>

// --- CẤU HÌNH PIN MÀN HÌNH ILI9341 ---
#ifndef TFT_CS
#define TFT_CS   39
#define TFT_DC   38
#define TFT_RST  47
#define TFT_MOSI 41
#define TFT_SCLK 40
#define TFT_MISO -1
#endif

#ifndef TFT_SPI_HZ
#define TFT_SPI_HZ 27000000
#endif

static SPIClass tftSPI(FSPI);
static Adafruit_ILI9341 tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// --- CẤU HÌNH 4 CHÂN SERVO ---
#ifndef SERVO_PIN_FL
#define SERVO_PIN_FL 7
#define SERVO_PIN_FR 4
#define SERVO_PIN_RL 6
#define SERVO_PIN_RR 5
#endif

#ifndef PIN_BOOT_BTN
#define PIN_BOOT_BTN 0
#endif

enum LegIndex {
  LEG_FL = 0,  // GPIO 7 (180° - Trên Trái)
  LEG_FR = 1,  // GPIO 4 (360° - Trên Phải)
  LEG_RL = 2,  // GPIO 6 (180° - Dưới Trái)
  LEG_RR = 3,  // GPIO 5 (180° - Dưới Phải)
  LEG_COUNT = 4
};

struct LegState {
  const char* name;
  const char* code;
  int pin;
  int channel;
  int currentAngle;
  int targetAngle;
  bool isServo360;
  bool attached;
};

static LegState legs[LEG_COUNT] = {
  {"Tren Trai (FL)", "FL", SERVO_PIN_FL, 0, 90, 90, false, false}, // Chân 180° (GPIO 7)
  {"Tren Phai (FR)", "FR", SERVO_PIN_FR, 1, 90, 90, true,  false}, // Chân 360° (GPIO 4)
  {"Duoi Trai (RL)", "RL", SERVO_PIN_RL, 2, 90, 90, false, false}, // Chân 180° (GPIO 6)
  {"Duoi Phai (RR)", "RR", SERVO_PIN_RR, 3, 90, 90, false, false}  // Chân 180° (GPIO 5)
};

static const int MIN_ANGLE = 40;
static const int MAX_ANGLE = 140;

// Cấu hình điểm dừng cho servo 360
static int deadband360 = 90;

enum OperationalMode {
  MODE_AUTO_DEMO,  // Tự động diễn chuỗi tư thế
  MODE_MANUAL,     // Giữ nguyên tư thế và nhận lệnh
  MODE_SWEEP       // Quét góc 10s
};

static OperationalMode currentMode = MODE_AUTO_DEMO;
static bool globalAttached = true;
static String currentPoseName = "STAND (Dung thang)";
static String lastHudPose = "";
static uint16_t currentEyeColor = ILI9341_CYAN;

// ----------------------------------------------------------------------------
// ĐIỀU KHIỂN XUNG LEDC PWM CHO SERVO
// ----------------------------------------------------------------------------
int angleToDuty(int angle) {
  angle = constrain(angle, 0, 180);
  // 50Hz, 14-bit resolution (0..16383). 0.5ms = 410, 2.5ms = 2048
  return map(angle, 0, 180, 410, 2048);
}

void applyServoDuty(int legIdx, int angle) {
  if (!globalAttached || !legs[legIdx].attached) {
    ledcWrite(legs[legIdx].channel, 0); // Ngắt xung PWM
    return;
  }
  int duty = angleToDuty(angle);
  ledcWrite(legs[legIdx].channel, duty);
}

void attachLeg(int legIdx) {
  ledcSetup(legs[legIdx].channel, 50, 14);
  ledcAttachPin(legs[legIdx].pin, legs[legIdx].channel);
  legs[legIdx].attached = true;
  applyServoDuty(legIdx, legs[legIdx].currentAngle);
}

void detachLeg(int legIdx) {
  ledcWrite(legs[legIdx].channel, 0);
  legs[legIdx].attached = false;
}

void attachAllServos() {
  globalAttached = true;
  Serial.println("\n[POWER] >>> Bat xung PWM cho ca 4 servos (ATTACH)...");
  for (int i = 0; i < LEG_COUNT; i++) {
    attachLeg(i);
    delay(50);
  }
}

void detachAllServos() {
  globalAttached = false;
  Serial.println("\n[POWER] >>> Ngat xung PWM cho ca 4 servos (RELAX/DETACH)...");
  for (int i = 0; i < LEG_COUNT; i++) {
    detachLeg(i);
  }
}

// ----------------------------------------------------------------------------
// GIAO DIỆN ROBO-EYES & HUD (ILI9341 240x320)
// ----------------------------------------------------------------------------
void drawEye(int cx, int cy, int w, int h, uint16_t color, int pupOffsetX = 0, int pupOffsetY = 0) {
  tft.fillRoundRect(cx - w/2, cy - h/2, w, h, 14, color);
  int pw = w / 2;
  int ph = h / 2;
  tft.fillRoundRect(cx + pupOffsetX - pw/2, cy + pupOffsetY - ph/2, pw, ph, 8, ILI9341_BLACK);
  tft.fillCircle(cx + pupOffsetX - pw/4, cy + pupOffsetY - ph/4, 4, ILI9341_WHITE);
}

// Vẽ lại riêng vùng mắt (không xóa cả màn hình -> không nháy)
void drawEyes(uint16_t eyeColor, int eyeH, int pupX, int pupY) {
  tft.fillRect(40, 30, 240, 92, ILI9341_BLACK);
  drawEye(105, 80, 65, eyeH, eyeColor, pupX, pupY);
  drawEye(215, 80, 65, eyeH, eyeColor, pupX, pupY);
}

// Vẽ lại riêng dòng thông số góc
void drawAngles() {
  const int xs[LEG_COUNT] = {12, 90, 168, 246};
  tft.fillRect(0, 154, 320, 14, ILI9341_BLACK);
  tft.setTextSize(1);
  for (int i = 0; i < LEG_COUNT; i++) {
    tft.setCursor(xs[i], 158);
    if (legs[i].isServo360) {
      tft.setTextColor(ILI9341_ORANGE);
      tft.printf("%s(360):%d", legs[i].code, legs[i].currentAngle);
    } else {
      tft.setTextColor(ILI9341_GREEN);
      tft.printf("%s(G%d):%d", legs[i].code, legs[i].pin, legs[i].currentAngle);
    }
  }
}

void drawHUD(const char* poseName, uint16_t eyeColor, int eyeH = 60, int pupX = 0, int pupY = 0) {
  tft.fillScreen(ILI9341_BLACK);

  // 1. Tiêu đề
  tft.fillRect(0, 0, 320, 24, 0x18C3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 7);
  tft.printf("XIAOZHI DOG - 4-LEG SERVO TEST [%s]",
             (currentMode == MODE_AUTO_DEMO) ? "AUTO" : "MANUAL");

  // 2. Hai mắt RoboEyes sinh động
  drawEyes(eyeColor, eyeH, pupX, pupY);

  // 3. Tên tư thế
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  int nameX = 160 - (int)(strlen(poseName) * 6);
  if (nameX < 4) nameX = 4;
  tft.setCursor(nameX, 126);
  tft.print(poseName);

  // 4. Bảng thông số góc 4 Chân
  tft.drawFastHLine(10, 150, 300, 0x4208);
  drawAngles();

  // 5. Khung lệnh nhanh
  tft.drawRoundRect(10, 174, 300, 42, 4, 0x2124);
  tft.fillRect(11, 175, 298, 40, 0x0841);
  tft.setTextColor(0x7BEF);
  tft.setTextSize(1);
  tft.setCursor(16, 180);
  tft.print("LENH: stand|sit|wave|crouch|wiggle|demo");
  tft.setCursor(16, 192);
  tft.print("relax | attach | all <do> | fl <do> | trim4 <n>");
  tft.setCursor(16, 204);
  tft.print("BOOT: nhan nhanh = doi tu the | giu = relax");

  // 6. Thanh đáy
  tft.fillRect(0, 220, 320, 20, 0x10A2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 226);
  tft.printf("PWM: %s | FR(G%d): %s", globalAttached ? "ON" : "OFF (relax)",
             legs[LEG_FR].pin, legs[LEG_FR].isServo360 ? "360, dung o trim4" : "180");

  lastHudPose = poseName;
}

// ----------------------------------------------------------------------------
// ĐIỀU KHIỂN CHUYỂN ĐỘNG MƯỢT (COSINE S-CURVE INTERPOLATION)
// ----------------------------------------------------------------------------
void setPoseSmooth(const char* poseName, int targetFL, int targetFR, int targetRL, int targetRR,
                   int durationMs = 500, uint16_t eyeCol = ILI9341_CYAN, int eyeH = 60,
                   int pupX = 0, int pupY = 0) {
  currentPoseName = poseName;
  currentEyeColor = eyeCol;
  if (!globalAttached) attachAllServos();

  int startAngles[LEG_COUNT];
  for (int i = 0; i < LEG_COUNT; i++) {
    startAngles[i] = legs[i].currentAngle;
  }

  // Chân 360 (FR - Trên Phải - GPIO 4) giữ quanh deadband để không quay tròn
  int safeFR = legs[LEG_FR].isServo360 ? deadband360 : constrain(targetFR, MIN_ANGLE, MAX_ANGLE);

  int targets[LEG_COUNT] = {
    constrain(targetFL, MIN_ANGLE, MAX_ANGLE),
    safeFR,
    constrain(targetRL, MIN_ANGLE, MAX_ANGLE),
    constrain(targetRR, MIN_ANGLE, MAX_ANGLE)
  };

  int steps = durationMs / 20;
  if (steps < 1) steps = 1;

  for (int s = 1; s <= steps; s++) {
    float t = (float)s / steps;
    float factor = (1.0f - cosf(t * M_PI)) / 2.0f; // S-curve

    for (int i = 0; i < LEG_COUNT; i++) {
      int deg = (int)(startAngles[i] + (targets[i] - startAngles[i]) * factor);
      legs[i].currentAngle = deg;
      applyServoDuty(i, deg);
    }
    delay(20);
  }

  for (int i = 0; i < LEG_COUNT; i++) {
    legs[i].currentAngle = targets[i];
    applyServoDuty(i, targets[i]);
  }

  // Cùng tư thế (WAVE/WIGGLE lặp) -> chỉ vẽ lại mắt + góc, tránh fillScreen gây nháy/giật
  if (lastHudPose == poseName) {
    drawEyes(eyeCol, eyeH, pupX, pupY);
    drawAngles();
  } else {
    drawHUD(poseName, eyeCol, eyeH, pupX, pupY);
  }
  Serial.printf("[POSE] >>> %s (FL:%d FR:%d RL:%d RR:%d)\n",
                poseName, legs[0].currentAngle, legs[1].currentAngle,
                legs[2].currentAngle, legs[3].currentAngle);
}

// ----------------------------------------------------------------------------
// CÁC TƯ THẾ CHUẨN
// ----------------------------------------------------------------------------
void poseStand() {
  setPoseSmooth("STAND (Dung thang)", 90, deadband360, 90, 90, 600, ILI9341_CYAN, 60);
}

void poseSit() {
  setPoseSmooth("SIT (Ngoi xuong)", 100, deadband360, 120, 120, 700, ILI9341_GREEN, 50);
}

void poseCrouch() {
  setPoseSmooth("CROUCH (Cui thap)", 65, deadband360, 65, 65, 600, ILI9341_ORANGE, 40);
}

void poseWave() {
  poseSit();
  delay(150);
  // Vẫy chân trên trái FL (GPIO 7 - 180°)
  for (int w = 0; w < 3; w++) {
    setPoseSmooth("WAVE (Vay chao)", 125, deadband360, 120, 120, 200, ILI9341_YELLOW, 70, 15, -10);
    delay(80);
    setPoseSmooth("WAVE (Vay chao)", 80, deadband360, 120, 120, 200, ILI9341_YELLOW, 70, -15, -10);
    delay(80);
  }
  setPoseSmooth("SIT (Ngoi xuong)", 100, deadband360, 120, 120, 300, ILI9341_GREEN, 50);
}

void poseWiggle() {
  poseStand();
  delay(100);
  // Lắc 2 chân sau RL (GPIO 6) và RR (GPIO 5)
  for (int w = 0; w < 3; w++) {
    setPoseSmooth("WIGGLE (Lac mong)", 90, deadband360, 110, 70, 180, ILI9341_MAGENTA, 65, 12, 0);
    delay(60);
    setPoseSmooth("WIGGLE (Lac mong)", 90, deadband360, 70, 110, 180, ILI9341_MAGENTA, 65, -12, 0);
    delay(60);
  }
  poseStand();
}

// ----------------------------------------------------------------------------
// XỬ LÝ LỆNH TỪ SERIAL CONSOLE
// ----------------------------------------------------------------------------
void handleSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  cmd.toLowerCase();
  Serial.printf("\n[CMD] >>> '%s'\n", cmd.c_str());

  if (cmd == "demo" || cmd == "auto") {
    currentMode = MODE_AUTO_DEMO;
    Serial.println("[MODE] Bat che do TU DONG DIEN CAC TU THE (Auto Demo)!");
    return;
  }

  // Chuyển sang Manual khi có lệnh tư thế cụ thể
  if (cmd == "stand") {
    currentMode = MODE_MANUAL;
    poseStand();
    return;
  }

  if (cmd == "sit") {
    currentMode = MODE_MANUAL;
    poseSit();
    return;
  }

  if (cmd == "crouch") {
    currentMode = MODE_MANUAL;
    poseCrouch();
    return;
  }

  if (cmd == "wave") {
    currentMode = MODE_MANUAL;
    poseWave();
    return;
  }

  if (cmd == "wiggle") {
    currentMode = MODE_MANUAL;
    poseWiggle();
    return;
  }

  if (cmd == "relax" || cmd == "detach" || cmd == "off") {
    detachAllServos();
    currentMode = MODE_MANUAL;
    drawHUD("RELAX (PWM OFF)", ILI9341_RED, 20);
    return;
  }

  if (cmd == "attach" || cmd == "on") {
    attachAllServos();
    poseStand();
    return;
  }

  // Đổi loại servo FR: xử lý TRƯỚC lệnh "all"/"fr <do>" để không bị nhánh khác bắt mất
  if (cmd == "fr 180") {
    legs[LEG_FR].isServo360 = false;
    Serial.println("[CONFIG] >>> Da chuyen FR (GPIO 4) thanh SERVO 180 TIEU CHUAN!");
    drawHUD("FR -> SERVO 180", ILI9341_GREEN, 60);
    return;
  }

  if (cmd == "fr 360") {
    legs[LEG_FR].isServo360 = true;
    Serial.println("[CONFIG] >>> Da chuyen FR (GPIO 4) thanh SERVO 360 (quay lien tuc)!");
    legs[LEG_FR].currentAngle = deadband360;
    applyServoDuty(LEG_FR, deadband360);
    drawHUD("FR -> SERVO 360", ILI9341_ORANGE, 60);
    return;
  }

  if (cmd.startsWith("all ")) {
    int deg = cmd.substring(4).toInt();
    currentMode = MODE_MANUAL;
    // FR được setPoseSmooth tự giữ deadband nếu là servo 360
    setPoseSmooth("ALL SERVO DEG", deg, deg, deg, deg, 400);
    return;
  }

  // Chỉnh điểm dừng cho chân 360 (FR - GPIO 4)
  if (cmd.startsWith("trim4")) {
    int spaceIdx = cmd.indexOf(' ');
    if (spaceIdx > 0) {
      deadband360 = cmd.substring(spaceIdx + 1).toInt();
      deadband360 = constrain(deadband360, 80, 100);
      legs[LEG_FR].currentAngle = deadband360;
      applyServoDuty(LEG_FR, deadband360);
      Serial.printf("[SERVO 360] Diem dung moi cho FR (GPIO 4): %d do\n", deadband360);
      return;
    }
  }

  // Góc từng chân
  for (int i = 0; i < LEG_COUNT; i++) {
    String codeStr = legs[i].code;
    codeStr.toLowerCase();
    codeStr += ' ';
    if (cmd.startsWith(codeStr)) {
      if (legs[i].isServo360) {
        Serial.printf("[WARN] %s dang la servo 360 (goc = toc do). Dung 'trim4 <80-100>' "
                      "de chinh diem dung, hoac 'fr 180' neu da thay servo 180.\n", legs[i].code);
        return;
      }
      int val = cmd.substring(codeStr.length()).toInt();
      int safeVal = constrain(val, MIN_ANGLE, MAX_ANGLE);
      currentMode = MODE_MANUAL;
      if (!globalAttached) attachAllServos();
      legs[i].currentAngle = safeVal;
      applyServoDuty(i, safeVal);
      Serial.printf("[SERVO] Chan %s -> %d do%s\n", legs[i].code, safeVal,
                    (safeVal != val) ? " (da gioi han 40-140)" : "");
      drawHUD("CHINH GOC LE", ILI9341_YELLOW, 60);
      return;
    }
  }

  Serial.println("[WARN] Lenh: stand | sit | wave | crouch | wiggle | demo | relax | attach |"
                 " all <do> | fl/rl/rr <do> | fr 180 | fr 360 | trim4 <80-100>");
}

// ----------------------------------------------------------------------------
// NÚT BẤM VẬT LÝ BOOT (GPIO 0)
// ----------------------------------------------------------------------------
static unsigned long btnPressStart = 0;
static bool btnIsDown = false;
static int manualPoseStep = 0;

void handleBootButton() {
  bool pressed = (digitalRead(PIN_BOOT_BTN) == LOW);

  if (pressed && !btnIsDown) {
    btnIsDown = true;
    btnPressStart = millis();
  } else if (!pressed && btnIsDown) {
    unsigned long duration = millis() - btnPressStart;
    btnIsDown = false;

    if (duration > 1200) {
      if (globalAttached) {
        detachAllServos();
        currentMode = MODE_MANUAL;
        drawHUD("RELAX (PWM OFF)", ILI9341_RED, 20);
      } else {
        attachAllServos();
        poseStand();
      }
    } else if (duration > 50) {
      // Nhấn nhanh: Chuyển tư thế kế tiếp
      currentMode = MODE_MANUAL;
      manualPoseStep = (manualPoseStep + 1) % 5;
      switch (manualPoseStep) {
        case 0: poseStand(); break;
        case 1: poseSit(); break;
        case 2: poseWave(); break;
        case 3: poseCrouch(); break;
        case 4: poseWiggle(); break;
      }
    }
  }
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  delay(600);

  Serial.println("\n========================================================");
  Serial.println("  XIAOZHI ROBOT DOG - 4-LEG FINAL VALIDATION TEST SUITE ");
  Serial.println("========================================================");

  // Khởi tạo màn hình ILI9341
  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(1);
  tft.setTextWrap(false);

  // Khởi tạo 4 chân servo
  attachAllServos();
  poseStand();

  Serial.println("[READY] He thong san sang! Bat dau chu trinh Auto Demo...");
}

// ----------------------------------------------------------------------------
// LOOP CHÍNH
// ----------------------------------------------------------------------------
static unsigned long lastAutoStepTime = 0;
static int autoStep = 0;
static String serialInputBuffer = "";

void loop() {
  unsigned long now = millis();

  // 1. Nếu ở chế độ AUTO DEMO: Tự động tuần hoàn các tư thế mỗi 3.5 giây
  if (currentMode == MODE_AUTO_DEMO && globalAttached) {
    if (now - lastAutoStepTime >= 3500) {
      autoStep = (autoStep + 1) % 5;

      switch (autoStep) {
        case 0: poseStand(); break;
        case 1: poseSit(); break;
        case 2: poseWave(); break;
        case 3: poseCrouch(); break;
        case 4: poseWiggle(); break;
      }
      lastAutoStepTime = millis();  // nghỉ đủ 3.5s SAU khi diễn xong
    }
  }

  // 2. Đọc lệnh từ Serial Monitor
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        handleSerialCommand(serialInputBuffer);
        serialInputBuffer = "";
      }
    } else if (serialInputBuffer.length() < 64) {
      serialInputBuffer += c;
    }
  }

  // 3. Kiểm tra nút bấm vật lý BOOT
  handleBootButton();

  delay(15);
}
