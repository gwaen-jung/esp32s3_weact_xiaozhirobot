/**
 * ============================================================================
 * XIAOZHI CUSTOM PCB - TEST TẤT CẢ TỰ ĐỘNG 100% (AUTO ALL-IN-ONE TEST V3)
 * ============================================================================
 * Tự động chạy toàn bộ chu trình kiểm tra liên hoàn ngay khi khởi động:
 *  1. Màn hình OLED SSD1306:
 *     - Tự động dò chân I2C (SDA=2/SCL=1 hoặc SDA=1/SCL=2)
 *     - Tự động dò địa chỉ (0x3C / 0x3D)
 *     - Bật Charge Pump & Contrast cực đại, chớp trắng màn hình rồi hiện đồ họa
 *     - Tự động thử kết nối lại liên tục nếu cắm lỏng
 *  2. Loa MAX98357A (I2S):
 *     - BCLK=11, WS=12, DIN=13
 *     - Tự động phát nhạc Mario Theme, âm quét tần số Sweep, nhạc chime
 *  3. Micro INMP441 (I2S):
 *     - BCLK=11, WS=12, SD=14
 *     - Thu âm và hiển thị cột sóng VU Meter nhảy theo tiếng loa/tiếng vỗ tay
 *  4. Driver Động Cơ Mini L298N:
 *     - Motor A: IN1 -> GPIO 4, IN2 -> GPIO 5
 *     - Motor B: IN3 -> GPIO 6, IN4 -> GPIO 7
 *     - Tự động chạy: Motor A Thuận/Nghịch -> Motor B Thuận/Nghịch -> Cả 2 Tiến/Lùi/Rẽ
 *  5. Nút BOOT (GPIO 0):
 *     - Bấm nhả: Chuyển nhanh sang bài test kế tiếp
 *     - Giữ > 1.2s: DỪNG KHẨN CẤP toàn bộ động cơ
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <math.h>

// --- PINOUT CẤU HÌNH ---
#define PIN_1 1
#define PIN_2 2

#ifndef PIN_I2S_BCLK
#define PIN_I2S_BCLK  11
#endif
#ifndef PIN_I2S_WS
#define PIN_I2S_WS    12
#endif
#ifndef PIN_I2S_DIN
#define PIN_I2S_DIN   14
#endif
#ifndef PIN_I2S_DOUT
#define PIN_I2S_DOUT  13
#endif

#ifndef PIN_MOTOR_IN1
#define PIN_MOTOR_IN1 4
#endif
#ifndef PIN_MOTOR_IN2
#define PIN_MOTOR_IN2 5
#endif
#ifndef PIN_MOTOR_IN3
#define PIN_MOTOR_IN3 6
#endif
#ifndef PIN_MOTOR_IN4
#define PIN_MOTOR_IN4 7
#endif

#ifndef PIN_BOOT_BTN
#define PIN_BOOT_BTN  0
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 48
#endif

// --- PWM MOTOR LEDC ---
#define PWM_CH_M1_IN1 0
#define PWM_CH_M1_IN2 1
#define PWM_CH_M2_IN3 2
#define PWM_CH_M2_IN4 3
#define PWM_FREQ      20000 // 20kHz
#define PWM_RES       8     // 8-bit

// --- OLED ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static bool oledReady = false;
static int activeSda = PIN_2;
static int activeScl = PIN_1;
static uint8_t activeOledAddr = 0x3C;

// --- I2S AUDIO ---
#define I2S_PORT      I2S_NUM_0
#define AUDIO_RATE    44100
#define AUDIO_CHUNK   256
static bool i2sReady = false;
static float micRms = 0.0f;
static float micNoiseFloor = 150.0f;

// --- MOTOR STATE ---
static int speedA = 0;
static int speedB = 0;
static const char* stateA = "STOP";
static const char* stateB = "STOP";

// --- AUTO SEQUENCE STATE MACHINE ---
enum AutoState {
  STATE_INIT = 0,
  STATE_OLED_INTRO,      // 1. Chớp màn hình, hiện Intro
  STATE_MUSIC_MARIO,     // 2. Loa phát nhạc Mario
  STATE_M1_FORWARD,      // 3. Motor A quay thuận
  STATE_M1_REVERSE,      // 4. Motor A quay nghịch
  STATE_M2_FORWARD,      // 5. Motor B quay thuận
  STATE_M2_REVERSE,      // 6. Motor B quay nghịch
  STATE_BOTH_FORWARD,    // 7. Cả 2 cùng tiến
  STATE_BOTH_REVERSE,    // 8. Cả 2 cùng lùi
  STATE_TURN_LEFT,       // 9. Xoay trái
  STATE_TURN_RIGHT,      // 10. Xoay phải
  STATE_AUDIO_SWEEP,     // 11. Loa quét tần số + Mic VU nhảy
  STATE_ROBO_EYES,       // 12. Mắt robot cử động & Nhạc kết thúc
  STATE_PAUSE,           // 13. Nghỉ 2.5s rồi lặp lại từ đầu
  STATE_STOP_ALL         // Dừng khẩn cấp
};

static AutoState currentState = STATE_INIT;
static uint32_t stateStartTime = 0;
static const char* currentTaskName = "KHOI DONG";

// Khai báo hàm âm thanh
void playBeep(float freqHz, int durationMs, float volume = 0.35f);
void playChirp();

// ============================================================================
// 1. MOTOR DRIVER MINI L298N (GPIO 4, 5, 6, 7)
// ============================================================================
void initMotors() {
  ledcSetup(PWM_CH_M1_IN1, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_M1_IN2, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_M2_IN3, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_M2_IN4, PWM_FREQ, PWM_RES);

  ledcAttachPin(PIN_MOTOR_IN1, PWM_CH_M1_IN1);
  ledcAttachPin(PIN_MOTOR_IN2, PWM_CH_M1_IN2);
  ledcAttachPin(PIN_MOTOR_IN3, PWM_CH_M2_IN3);
  ledcAttachPin(PIN_MOTOR_IN4, PWM_CH_M2_IN4);

  ledcWrite(PWM_CH_M1_IN1, 0);
  ledcWrite(PWM_CH_M1_IN2, 0);
  ledcWrite(PWM_CH_M2_IN3, 0);
  ledcWrite(PWM_CH_M2_IN4, 0);
}

void setMotorA(int spd) {
  spd = constrain(spd, -255, 255);
  speedA = spd;
  if (spd > 0) {
    ledcWrite(PWM_CH_M1_IN1, spd);
    ledcWrite(PWM_CH_M1_IN2, 0);
    stateA = "THUAN";
  } else if (spd < 0) {
    ledcWrite(PWM_CH_M1_IN1, 0);
    ledcWrite(PWM_CH_M1_IN2, -spd);
    stateA = "NGHICH";
  } else {
    ledcWrite(PWM_CH_M1_IN1, 0);
    ledcWrite(PWM_CH_M1_IN2, 0);
    stateA = "DUNG";
  }
}

void setMotorB(int spd) {
  spd = constrain(spd, -255, 255);
  speedB = spd;
  if (spd > 0) {
    ledcWrite(PWM_CH_M2_IN3, spd);
    ledcWrite(PWM_CH_M2_IN4, 0);
    stateB = "THUAN";
  } else if (spd < 0) {
    ledcWrite(PWM_CH_M2_IN3, 0);
    ledcWrite(PWM_CH_M2_IN4, -spd);
    stateB = "NGHICH";
  } else {
    ledcWrite(PWM_CH_M2_IN3, 0);
    ledcWrite(PWM_CH_M2_IN4, 0);
    stateB = "DUNG";
  }
}

void stopAllMotors() {
  setMotorA(0);
  setMotorB(0);
}

// ============================================================================
// 2. KHỞI TẠO MÀN HÌNH OLED SSD1306 (QUÉT NHANH TRÊN CHÂN IO1 & IO2)
// ============================================================================
void detectAndInitOled() {
  int pinPairs[][2] = {{PIN_2, PIN_1}, {PIN_1, PIN_2}};

  for (int i = 0; i < 2; i++) {
    int sda = pinPairs[i][0];
    int scl = pinPairs[i][1];

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delay(10);
    int vsda = digitalRead(sda);
    int vscl = digitalRead(scl);
    Serial.printf("[I2C PROBE] SDA(IO%d)=%d, SCL(IO%d)=%d\n", sda, vsda, scl, vscl);

    Wire.end();
    delay(10);
    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(30);

    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      uint8_t err = Wire.endTransmission();
      if (err == 0) {
        Serial.printf("[I2C FOUND] Co thiet bi tai 0x%02X tren SDA=%d, SCL=%d\n", addr, sda, scl);
        if (display.begin(SSD1306_SWITCHCAPVCC, addr, false, false)) {
          activeSda = sda;
          activeScl = scl;
          activeOledAddr = addr;
          oledReady = true;

          display.ssd1306_command(SSD1306_DISPLAYON);
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(0xFF);

          // Chớp trắng toàn màn hình xác nhận hiển thị
          display.clearDisplay();
          display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
          display.display();
          delay(200);
          display.clearDisplay();
          display.display();

          Serial.printf("[OLED] Khoi dong thanh cong tai SDA=%d, SCL=%d, Addr=0x%02X!\n", sda, scl, addr);
          return;
        }
      }
    }
  }

  oledReady = false;
  Serial.println("[OLED] Khong tim thay bat ky thiet bi I2C nao tren IO1 & IO2!");
}

// ============================================================================
// 3. ÂM THANH I2S FULL-DUPLEX (MAX98357A & INMP441)
// ============================================================================
void setupI2SAudio() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
      .sample_rate = AUDIO_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 6,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = PIN_I2S_BCLK,
      .ws_io_num = PIN_I2S_WS,
      .data_out_num = PIN_I2S_DOUT,
      .data_in_num = PIN_I2S_DIN
  };

  i2s_driver_uninstall(I2S_PORT);
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err == ESP_OK) {
    err = i2s_set_pin(I2S_PORT, &pin_config);
  }
  i2sReady = (err == ESP_OK);
}

void playBeep(float freqHz, int durationMs, float volume) {
  if (!i2sReady) {
    Serial.println("[AUDIO ERROR] I2S chua khoi tao duoc!");
    return;
  }
  const int CHUNK = 256;
  static int32_t txBuf[CHUNK * 2];
  int samplesLeft = (AUDIO_RATE * durationMs) / 1000;
  float phase = 0.0f;
  const float phaseInc = (2.0f * M_PI * freqHz) / AUDIO_RATE;

  while (samplesLeft > 0) {
    int chunk = (samplesLeft > CHUNK) ? CHUNK : samplesLeft;
    for (int i = 0; i < chunk; i++) {
      // Sóng vuông âm lượng lớn (đanh rõ, dễ nghe nhất trên loa nhỏ)
      int32_t val16 = (sinf(phase) >= 0) ? (int32_t)(32500.0f * volume) : (int32_t)(-32500.0f * volume);
      int32_t sample32 = val16 << 16;
      txBuf[i * 2]     = sample32;
      txBuf[i * 2 + 1] = sample32;
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t written = 0;
    i2s_write(I2S_PORT, txBuf, chunk * 2 * sizeof(int32_t), &written, pdMS_TO_TICKS(100));
    samplesLeft -= chunk;
  }
}

void playChirp() {
  for (float f = 600.0f; f <= 1500.0f; f += 200.0f) {
    playBeep(f, 18, 0.25f);
  }
}

void processMicAudio() {
  if (!i2sReady) return;
  static int32_t rxBuffer[AUDIO_CHUNK * 2];
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, rxBuffer, sizeof(rxBuffer), &bytesRead, 0);
  int frames = bytesRead / (2 * sizeof(int32_t));
  if (frames == 0) return;

  int64_t sumSqA = 0, sumSqB = 0;
  for (int i = 0; i < frames; i++) {
    int32_t sA = rxBuffer[i * 2] >> 16;
    int32_t sB = rxBuffer[i * 2 + 1] >> 16;
    sumSqA += (int64_t)sA * sA;
    sumSqB += (int64_t)sB * sB;
  }
  float rawRms = sqrtf((float)max(sumSqA, sumSqB) / frames);
  micRms = (micRms * 0.70f) + (rawRms * 0.30f);
}

// ============================================================================
// 4. HIỂN THỊ OLED TRỰC QUAN THEO TỪNG BƯỚC TEST
// ============================================================================
void renderOLED() {
  if (!oledReady) return;

  display.clearDisplay();

  // Thanh tiêu đề đảo màu
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 2);
  display.print("AUTO TEST: ");
  display.print(currentTaskName);

  display.setTextColor(SSD1306_WHITE);

  // Hiển thị trạng thái Motor A & B
  display.setCursor(2, 16);
  display.printf("M1(4,5): %-7s %3d%%", stateA, (abs(speedA) * 100) / 255);
  display.setCursor(2, 27);
  display.printf("M2(6,7): %-7s %3d%%", stateB, (abs(speedB) * 100) / 255);

  // Thanh thước đo công suất động cơ đồ họa
  display.drawRect(2, 38, 58, 6, SSD1306_WHITE);
  int wA = map(abs(speedA), 0, 255, 0, 56);
  if (wA > 0) display.fillRect(3, 39, wA, 4, SSD1306_WHITE);

  display.drawRect(68, 38, 58, 6, SSD1306_WHITE);
  int wB = map(abs(speedB), 0, 255, 0, 56);
  if (wB > 0) display.fillRect(69, 39, wB, 4, SSD1306_WHITE);

  // Cột sóng âm thanh Mic VU
  display.setCursor(2, 48);
  display.print("MIC VU:");
  display.drawRect(48, 48, 78, 7, SSD1306_WHITE);
  float activeLevel = micRms - micNoiseFloor;
  if (activeLevel < 0) activeLevel = 0;
  int vuW = map(constrain((int)activeLevel, 0, 2500), 0, 2500, 0, 74);
  if (vuW > 0) display.fillRect(50, 50, vuW, 3, SSD1306_WHITE);

  // Footer
  display.setCursor(2, 57);
  display.printf("I2C:%d,%d | I2S:11..14", activeSda, activeScl);

  display.display();
}

void renderRoboEyes() {
  if (!oledReady) return;
  display.clearDisplay();

  static int eyeTick = 0;
  eyeTick++;
  int eyeH = (eyeTick % 20 == 0) ? 4 : 30;
  int eyeW = 36;

  display.fillRoundRect(22, 16 + (30 - eyeH) / 2, eyeW, eyeH, 6, SSD1306_WHITE);
  display.fillRoundRect(70, 16 + (30 - eyeH) / 2, eyeW, eyeH, 6, SSD1306_WHITE);

  if (eyeH > 8) {
    display.fillCircle(36, 30, 4, SSD1306_BLACK);
    display.fillCircle(84, 30, 4, SSD1306_BLACK);
    display.fillCircle(38, 28, 2, SSD1306_WHITE);
    display.fillCircle(86, 28, 2, SSD1306_WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 54);
  display.print("< TEST ALL OK! >");
  display.display();
}

// ============================================================================
// 5. MÁY TRẠNG THÁI TỰ ĐỘNG CHẠY LIÊN TỤC (AUTO LOOP)
// ============================================================================
void gotoState(AutoState nextState) {
  currentState = nextState;
  stateStartTime = millis();

  switch (currentState) {
    case STATE_OLED_INTRO:
      currentTaskName = "INTRO";
      stopAllMotors();
      neopixelWrite(STATUS_LED_PIN, 0, 0, 40); // Xanh dương
      break;

    case STATE_MUSIC_MARIO:
      currentTaskName = "LOA MARIO";
      stopAllMotors();
      neopixelWrite(STATUS_LED_PIN, 40, 20, 0); // Vàng
      break;

    case STATE_M1_FORWARD:
      currentTaskName = "M1 THUAN";
      neopixelWrite(STATUS_LED_PIN, 0, 40, 0); // Xanh lá
      break;

    case STATE_M1_REVERSE:
      currentTaskName = "M1 NGHICH";
      neopixelWrite(STATUS_LED_PIN, 0, 40, 20); // Cyan
      break;

    case STATE_M2_FORWARD:
      currentTaskName = "M2 THUAN";
      neopixelWrite(STATUS_LED_PIN, 0, 40, 0);
      break;

    case STATE_M2_REVERSE:
      currentTaskName = "M2 NGHICH";
      neopixelWrite(STATUS_LED_PIN, 0, 40, 20);
      break;

    case STATE_BOTH_FORWARD:
      currentTaskName = "CA 2 TIEN";
      neopixelWrite(STATUS_LED_PIN, 40, 40, 0);
      playBeep(900, 50, 0.2f);
      break;

    case STATE_BOTH_REVERSE:
      currentTaskName = "CA 2 LUI";
      neopixelWrite(STATUS_LED_PIN, 40, 10, 0);
      playBeep(500, 50, 0.2f);
      break;

    case STATE_TURN_LEFT:
      currentTaskName = "RE TRAI";
      playBeep(700, 40, 0.2f);
      break;

    case STATE_TURN_RIGHT:
      currentTaskName = "RE PHAI";
      playBeep(750, 40, 0.2f);
      break;

    case STATE_AUDIO_SWEEP:
      currentTaskName = "LOA SWEEP";
      stopAllMotors();
      break;

    case STATE_ROBO_EYES:
      currentTaskName = "HOAN TAT";
      stopAllMotors();
      neopixelWrite(STATUS_LED_PIN, 0, 50, 0);
      break;

    case STATE_PAUSE:
      currentTaskName = "NGHI 2.5S";
      stopAllMotors();
      break;

    default:
      stopAllMotors();
      break;
  }
  Serial.printf("\n>>> BUOC TIEP THEO: [%s] <<<\n", currentTaskName);
}

void stepAutoTest() {
  uint32_t elapsed = millis() - stateStartTime;

  switch (currentState) {
    case STATE_INIT:
      gotoState(STATE_OLED_INTRO);
      break;

    case STATE_OLED_INTRO:
      if (elapsed < 1200) {
        // Giữ màn hình intro
      } else {
        gotoState(STATE_MUSIC_MARIO);
      }
      break;

    case STATE_MUSIC_MARIO: {
      // Phát 7 nốt Mario kinh điển âm lượng lớn 85%
      struct Note { float f; int d; };
      Note notes[] = {
        {659.25f, 120}, {0, 30},
        {659.25f, 120}, {0, 90},
        {659.25f, 120}, {0, 90},
        {523.25f, 120}, {0, 30},
        {659.25f, 140}, {0, 90},
        {783.99f, 180}, {0, 160},
        {392.00f, 200}, {0, 120}
      };
      for (auto& n : notes) {
        if (n.f > 0) playBeep(n.f, n.d, 1.0f);
        else delay(n.d);
        processMicAudio();
        renderOLED();
      }
      gotoState(STATE_M1_FORWARD);
      break;
    }

    case STATE_M1_FORWARD:
      if (elapsed < 1400) {
        int spd = map(elapsed, 0, 1400, 110, 230);
        setMotorA(spd);
        setMotorB(0);
      } else {
        stopAllMotors();
        playBeep(650, 30, 0.2f);
        gotoState(STATE_M1_REVERSE);
      }
      break;

    case STATE_M1_REVERSE:
      if (elapsed < 1400) {
        int spd = map(elapsed, 0, 1400, -110, -230);
        setMotorA(spd);
        setMotorB(0);
      } else {
        stopAllMotors();
        playBeep(700, 30, 0.2f);
        gotoState(STATE_M2_FORWARD);
      }
      break;

    case STATE_M2_FORWARD:
      if (elapsed < 1400) {
        int spd = map(elapsed, 0, 1400, 110, 230);
        setMotorA(0);
        setMotorB(spd);
      } else {
        stopAllMotors();
        playBeep(750, 30, 0.2f);
        gotoState(STATE_M2_REVERSE);
      }
      break;

    case STATE_M2_REVERSE:
      if (elapsed < 1400) {
        int spd = map(elapsed, 0, 1400, -110, -230);
        setMotorA(0);
        setMotorB(spd);
      } else {
        stopAllMotors();
        playBeep(800, 30, 0.2f);
        gotoState(STATE_BOTH_FORWARD);
      }
      break;

    case STATE_BOTH_FORWARD:
      if (elapsed < 1500) {
        setMotorA(200);
        setMotorB(200);
      } else {
        stopAllMotors();
        gotoState(STATE_BOTH_REVERSE);
      }
      break;

    case STATE_BOTH_REVERSE:
      if (elapsed < 1500) {
        setMotorA(-200);
        setMotorB(-200);
      } else {
        stopAllMotors();
        gotoState(STATE_TURN_LEFT);
      }
      break;

    case STATE_TURN_LEFT:
      if (elapsed < 1100) {
        setMotorA(-180);
        setMotorB(180);
      } else {
        stopAllMotors();
        gotoState(STATE_TURN_RIGHT);
      }
      break;

    case STATE_TURN_RIGHT:
      if (elapsed < 1100) {
        setMotorA(180);
        setMotorB(-180);
      } else {
        stopAllMotors();
        gotoState(STATE_AUDIO_SWEEP);
      }
      break;

    case STATE_AUDIO_SWEEP:
      // Quét tần số 350Hz -> 2200Hz âm lượng lớn 85%
      for (float f = 350.0f; f <= 2200.0f; f += 50.0f) {
        playBeep(f, 22, 1.0f);
        processMicAudio();
        renderOLED();
      }
      gotoState(STATE_ROBO_EYES);
      break;

    case STATE_ROBO_EYES:
      if (elapsed < 2000) {
        renderRoboEyes();
      } else {
        gotoState(STATE_PAUSE);
      }
      break;

    case STATE_PAUSE:
      if (elapsed < 2500) {
        // Nghỉ ngơi trước khi lặp lại vòng mới
      } else {
        // LẶP LẠI TỪ ĐẦU!
        gotoState(STATE_MUSIC_MARIO);
      }
      break;

    case STATE_STOP_ALL:
      stopAllMotors();
      break;
  }
}

// ============================================================================
// 6. XỬ LÝ NÚT BOOT (GPIO 0)
// ============================================================================
void checkInputs() {
  static bool lastBtn = HIGH;
  static uint32_t pressTime = 0;
  static bool longHandled = false;

  bool curBtn = digitalRead(PIN_BOOT_BTN);
  if (lastBtn == HIGH && curBtn == LOW) {
    pressTime = millis();
    longHandled = false;
  } else if (lastBtn == LOW && curBtn == LOW) {
    if (!longHandled && (millis() - pressTime > 1200)) {
      longHandled = true;
      Serial.println("\n[SAFETY] GIU BOOT > 1.2s -> DUNG TOAN BO!");
      gotoState(STATE_STOP_ALL);
      playBeep(350, 300, 0.4f);
    }
  } else if (lastBtn == LOW && curBtn == HIGH) {
    if (!longHandled && (millis() - pressTime > 50)) {
      // Bấm nhả: Bỏ qua sang bài test kế tiếp
      Serial.println("\n[BTN] Chuyen sang buoc tiep theo!");
      playChirp();
      int next = (int)currentState + 1;
      if (next >= STATE_PAUSE) next = STATE_MUSIC_MARIO;
      gotoState((AutoState)next);
    }
  }
  lastBtn = curBtn;

  // Hỗ trợ phím Space / X từ Serial nếu có cắm monitor
  while (Serial.available()) {
    char c = Serial.read();
    if (c == ' ' || c == 'x' || c == 'X') {
      gotoState(STATE_STOP_ALL);
    } else if (c == 'n' || c == 'N') {
      int next = (int)currentState + 1;
      if (next >= STATE_PAUSE) next = STATE_MUSIC_MARIO;
      gotoState((AutoState)next);
    }
  }
}

// ============================================================================
// 7. SETUP & MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  neopixelWrite(STATUS_LED_PIN, 0, 0, 40);

  delay(400);
  Serial.println("\n==============================================================");
  Serial.println("  XIAOZHI CUSTOM PCB - ALL-IN-ONE AUTOMATED TEST SUITE        ");
  Serial.println("==============================================================");

  // 1. Dò tìm và khởi động OLED (thử cả 2 chiều SDA/SCL và cả 0x3C/0x3D)
  detectAndInitOled();

  // 2. Cấu hình driver động cơ
  initMotors();

  // 3. Khởi tạo I2S cho Loa & Mic
  setupI2SAudio();

  // 4. Phát chuông khởi động
  playChirp();

  neopixelWrite(STATUS_LED_PIN, 0, 30, 0); // Xanh lá
  Serial.println("[AUTO] He thong bat dau chay chu trinh test tu dong lien hoan!");
  gotoState(STATE_INIT);
}

void loop() {
  processMicAudio();
  checkInputs();
  stepAutoTest();

  // Cập nhật OLED (~25 FPS)
  static uint32_t lastOled = 0;
  if (millis() - lastOled >= 40) {
    lastOled = millis();
    if (currentState != STATE_ROBO_EYES) {
      renderOLED();
    }

    // Nếu lúc đầu chưa thấy OLED, thử dò lại mỗi 5 giây
    static uint32_t lastOledRetry = 0;
    if (!oledReady && (millis() - lastOledRetry > 5000)) {
      lastOledRetry = millis();
      detectAndInitOled();
    }
  }

  delay(2);
}
