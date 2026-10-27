/**
 * ============================================================================
 *   XIAOZHI SMART COMPANION ROBOT (ALL-IN-ONE COMBINED FIRMWARE)
 *   Dự án: Robot Để Bàn Thông Minh Tương Tác Giọng Nói & Cử Động Bánh Xe
 *   Phần cứng: ESP32-S3 (Clone / DevKitC-1 N8/N16) + Custom PCB
 * ============================================================================
 * 
 * TỔNG HỢP CÁC TÍNH NĂNG ĐỈNH CAO:
 *  1. Micro I2S INMP441 (BCLK=11, WS=12, SD=14):
 *     - Bộ lọc số DC Blocker (High-Pass 20Hz) khử sạch trôi áp tĩnh.
 *     - Tiền khuếch đại Pre-amp số +12dB (dịch 14-bit).
 *     - Tự động thích ứng tạp âm nền phòng (Adaptive Noise Floor VAD).
 *     - Thu âm tự động khi người dùng nói, tự ngắt khi dừng nói 0.55s.
 * 
 *  2. Loa I2S MAX98357A (BCLK=11, WS=12, DIN=13, SD nối 3.3V qua JP1):
 *     - Thuật toán Auto-Gain Normalization kích âm tự động lên đến 16x.
 *     - Thuật toán Pitch-Shifting 3 chế độ giọng:
 *       + TALKING TOM (Mèo Tom cao độ x1.35)
 *       + GIỌNG CHUẨN (Nguyên bản x1.0)
 *       + ROBOT TRẦM (Người máy siêu trầm x0.80)
 *     - Cơ chế chống tự kích hoạt (Anti-Echo Suppression + Cooldown 400ms).
 * 
 *  3. Màn hình OLED SSD1306 1.3" / 0.96" I2C (SDA=GPIO 1, SCL=GPIO 2, Addr=0x3C):
 *     - Trạng thái chờ (IDLE): Mắt RoboEyes chớp nháy, thỉnh thoảng liếc nhìn trái/phải.
 *     - Trạng thái nghe (RECORDING): Mắt mở to chú ý, miệng mở theo cường độ âm thanh.
 *     - Trạng thái nhại lại (PLAYBACK): Miệng co giãn nhịp nhàng theo từng câu nói.
 * 
 *  4. Động cơ Mini L298N (Motor A: GPIO 4, 5; Motor B: GPIO 6, 7):
 *     - Phản ứng gật đầu / nhích nhẹ khi phát hiện tiếng người nói.
 *     - Lắc lư, đung đưa bánh xe vui nhộn nhịp nhàng trong lúc phát loa nhại giọng.
 *     - Tính năng AN TOÀN TRÊN BÀN CAO (Desk Safety Lock): Giữ nút BOOT > 1.2s để Bật/Tắt motor.
 * 
 *  5. Nút bấm BOOT (GPIO 0) & Đèn LED RGB WS2812 (GPIO 48):
 *     - Bấm nhả ngắn: Đổi chế độ giọng (Talking Tom -> Chuẩn -> Trầm).
 *     - Bấm giữ > 1.2s: Bật / Tắt động cơ di chuyển (Motor ON/OFF).
 *     - LED RGB: Xanh dương (Chờ nghe) -> Đỏ (Đang ghi âm) -> Xanh lá (Đang phát loa & nhảy múa).
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <math.h>

// --- PINOUT PHẦN CỨNG ---
#define PIN_OLED_SDA     1
#define PIN_OLED_SCL     2

#define PIN_I2S_BCLK     11
#define PIN_I2S_WS       12
#define PIN_I2S_DIN      14   // Micro INMP441 SD
#define PIN_I2S_DOUT     13   // Loa MAX98357A DIN

#define PIN_MOTOR_IN1    4    // Motor A (Trái)
#define PIN_MOTOR_IN2    5    // Motor A (Trái)
#define PIN_MOTOR_IN3    6    // Motor B (Phải)
#define PIN_MOTOR_IN4    7    // Motor B (Phải)

#define PIN_BOOT_BTN     0    // Nút BOOT Onboard
#define STATUS_LED_PIN   48   // LED RGB WS2812 Onboard

// --- PWM ĐỘNG CƠ MINI L298N ---
#define PWM_CH_M1_IN1    0
#define PWM_CH_M1_IN2    1
#define PWM_CH_M2_IN3    2
#define PWM_CH_M2_IN4    3
#define PWM_FREQ         20000 // 20 kHz siêu âm, êm ái không rít
#define PWM_RES          8     // Độ phân giải 8-bit (0 - 255)

// --- CẤU HÌNH ÂM THANH I2S ---
#define I2S_PORT         I2S_NUM_0
#define SAMPLE_RATE      16000     // 16 kHz chuẩn giọng nói
#define CHUNK_SIZE       256
#define MAX_RECORD_SEC   3.5f      // Bộ đệm ghi âm 3.5 giây
#define TOTAL_SAMPLES    ((int)(SAMPLE_RATE * MAX_RECORD_SEC))

#define SILENCE_TIMEOUT_MS     550     // Dừng nói quá 550ms thì bắt đầu phát lại
#define MIN_SPEECH_DURATION_MS 350     // Nói tối thiểu 350ms mới nhại

// --- BỘ NHỚ RAM LƯU ÂM THANH ---
static int16_t* audioBuffer = nullptr;
static int recordedSamples = 0;
static int maxRecordSamples = TOTAL_SAMPLES;
static uint32_t lastPlaybackEnd = 0; // Chống echo feedback

// --- MÀN HÌNH OLED 1.3" / 0.96" SSD1306 ---
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static bool oledReady = false;

// --- TRẠNG THÁI HỆ THỐNG ---
enum RobotState {
  STATE_IDLE_WAIT = 0, // Đang lắng nghe, mắt RoboEyes quan sát
  STATE_RECORDING,     // Đang thu âm câu nói, mắt mở to chú ý
  STATE_PLAYBACK       // Đang nhại lại qua loa & cử động miệng, bánh xe
};

static RobotState robotState = STATE_IDLE_WAIT;
static float currentRms = 0.0f;
static float noiseFloor = 100.0f;
static uint32_t lastVoiceTime = 0;
static uint32_t speechStartTime = 0;
static int mouthAnimTick = 0;

// --- CHẾ ĐỘ GIỌNG NÓI (VOICE MODES) ---
enum VoiceMode {
  MODE_CHIPMUNK = 0,   // Giọng Talking Tom / Sóc chuột (Cao độ x1.35)
  MODE_NORMAL,         // Giọng chuẩn nguyên bản (x1.0)
  MODE_DEEP_ROBOT      // Giọng người máy trầm (Hạ cao độ x0.80)
};

static VoiceMode currentMode = MODE_CHIPMUNK;
static const char* modeNames[] = {"TALKING TOM", "GIONG CHUAN", "ROBOT TRAM"};
static float pitchSpeeds[]     = {1.35f, 1.0f, 0.80f};

// --- CHẾ ĐỘ ĐỘNG CƠ (MOTOR ENABLE / DISABLE) ---
static bool motorEnabled = true; // Mặc định bật cử động, giữ BOOT 1.2s để tắt (chống rơi bàn)

// Khai báo nguyên mẫu hàm
void updateDisplay();
void playMimicVoice();
void setMotorA(int spd);
void setMotorB(int spd);
void stopAllMotors();

// ============================================================================
// 1. ĐIỀU KHIỂN ĐỘNG CƠ BÁNH XE MINI L298N
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

  stopAllMotors();
}

void setMotorA(int spd) {
  if (!motorEnabled) spd = 0;
  spd = constrain(spd, -255, 255);
  if (spd > 0) {
    ledcWrite(PWM_CH_M1_IN1, spd);
    ledcWrite(PWM_CH_M1_IN2, 0);
  } else if (spd < 0) {
    ledcWrite(PWM_CH_M1_IN1, 0);
    ledcWrite(PWM_CH_M1_IN2, -spd);
  } else {
    ledcWrite(PWM_CH_M1_IN1, 0);
    ledcWrite(PWM_CH_M1_IN2, 0);
  }
}

void setMotorB(int spd) {
  if (!motorEnabled) spd = 0;
  spd = constrain(spd, -255, 255);
  if (spd > 0) {
    ledcWrite(PWM_CH_M2_IN3, spd);
    ledcWrite(PWM_CH_M2_IN4, 0);
  } else if (spd < 0) {
    ledcWrite(PWM_CH_M2_IN3, 0);
    ledcWrite(PWM_CH_M2_IN4, -spd);
  } else {
    ledcWrite(PWM_CH_M2_IN3, 0);
    ledcWrite(PWM_CH_M2_IN4, 0);
  }
}

void stopAllMotors() {
  ledcWrite(PWM_CH_M1_IN1, 0);
  ledcWrite(PWM_CH_M1_IN2, 0);
  ledcWrite(PWM_CH_M2_IN3, 0);
  ledcWrite(PWM_CH_M2_IN4, 0);
}

// Cử động nhảy múa / lắc hông vui nhộn theo nhịp câu nói
void stepDanceMotion(int tick) {
  if (!motorEnabled) return;
  int pattern = (tick / 2) % 6;
  switch (pattern) {
    case 0: setMotorA(140);  setMotorB(-140); break; // Lắc trái
    case 1: setMotorA(-140); setMotorB(140);  break; // Lắc phải
    case 2: setMotorA(150);  setMotorB(150);   break; // Nhích tiến nhẹ
    case 3: setMotorA(-150); setMotorB(-150);  break; // Nhích lùi nhẹ
    case 4: setMotorA(-130); setMotorB(130);  break; // Lắc phải
    case 5: setMotorA(130);  setMotorB(-130); break; // Lắc trái
    default: stopAllMotors(); break;
  }
}

// Cử động gật đầu nhẹ khi vừa nghe tiếng nói
void triggerListeningNod() {
  if (!motorEnabled) return;
  setMotorA(160);
  setMotorB(160);
  delay(60);
  setMotorA(-160);
  setMotorB(-160);
  delay(50);
  stopAllMotors();
}

// ============================================================================
// 2. KHỞI TẠO MÀN HÌNH OLED SSD1306 (SDA=1, SCL=2, Addr=0x3C)
// ============================================================================
void initOLED() {
  int pinPairs[][2] = {{1, 2}, {2, 1}};

  for (int i = 0; i < 2; i++) {
    int sda = pinPairs[i][0];
    int scl = pinPairs[i][1];

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delay(10);

    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(30);
    delay(20);

    uint8_t addrs[] = {0x3C, 0x3D};
    for (uint8_t addr : addrs) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        if (display.begin(SSD1306_SWITCHCAPVCC, addr, false, false)) {
          oledReady = true;

          display.ssd1306_command(SSD1306_DISPLAYON);
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(0xFF);

          // Chớp trắng toàn màn hình chào mừng
          display.clearDisplay();
          display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
          display.display();
          delay(120);
          display.clearDisplay();
          display.display();

          Serial.printf("[OLED] Khoi dong thanh cong tai 0x%02X tren SDA=%d, SCL=%d!\n", addr, sda, scl);
          return;
        }
      }
    }
  }

  oledReady = false;
  Serial.println("[OLED] Khong tim thay man hinh OLED tai 0x3C/0x3D.");
}

// ============================================================================
// 3. KHỞI TẠO ÂM THANH I2S FULL-DUPLEX (MAX98357A & INMP441)
// ============================================================================
void initI2SAudio() {
  Serial.println("[I2S] Khoi tao Full-Duplex (16kHz, 32-bit slot cho INMP441 & MAX98357A)...");

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 6,
      .dma_buf_len = CHUNK_SIZE,
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

  if (err == ESP_OK) {
    Serial.println("[I2S] Khoi tao am thanh Loa & Mic thanh cong 100%!");
  } else {
    Serial.printf("[I2S ERROR] Loi khoi tao: %d\n", err);
  }
}

// Phát âm thanh tiếng bíp / chuông hiệu ứng
void playTone(float freqHz, int durationMs, float vol = 0.5f) {
  int total = (SAMPLE_RATE * durationMs) / 1000;
  float phase = 0.0f;
  float inc = (2.0f * M_PI * freqHz) / SAMPLE_RATE;
  static int32_t buf[CHUNK_SIZE * 2];

  while (total > 0) {
    int chunk = (total > CHUNK_SIZE) ? CHUNK_SIZE : total;
    for (int i = 0; i < chunk; i++) {
      int32_t val16 = (sinf(phase) >= 0) ? (int32_t)(30000.0f * vol) : (int32_t)(-30000.0f * vol);
      int32_t s32 = val16 << 16;
      buf[i * 2] = s32;
      buf[i * 2 + 1] = s32;
      phase += inc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t written = 0;
    i2s_write(I2S_PORT, buf, chunk * 2 * sizeof(int32_t), &written, pdMS_TO_TICKS(100));
    total -= chunk;
  }
}

// Giai điệu khởi động Mario Chime vui nhộn
void playStartupMario() {
  struct Note { float f; int d; };
  Note melody[] = {
    {659.25f, 100}, {0, 20},
    {659.25f, 100}, {0, 60},
    {659.25f, 100}, {0, 60},
    {523.25f, 100}, {0, 20},
    {659.25f, 120}, {0, 60},
    {783.99f, 160}
  };
  for (auto& n : melody) {
    if (n.f > 0) playTone(n.f, n.d, 0.45f);
    else delay(n.d);
  }
}

// ============================================================================
// 4. PHÁT LẠI GIỌNG NÓI VỚI AUTO-GAIN, PITCH SHIFT & CỬ ĐỘNG NHẢY MÚA
// ============================================================================
void playMimicVoice() {
  if (recordedSamples < (int)(SAMPLE_RATE * (MIN_SPEECH_DURATION_MS / 1000.0f))) {
    Serial.println("[PARROT] Doan noi qua ngan, bo qua.");
    robotState = STATE_IDLE_WAIT;
    stopAllMotors();
    return;
  }

  robotState = STATE_PLAYBACK;
  neopixelWrite(STATUS_LED_PIN, 0, 50, 0); // Xanh lá báo phát loa

  // 1. Tìm biên độ đỉnh thực tế của câu nói (bỏ qua tiếng click/pop đầu & cuối)
  int numBlocks = recordedSamples / 256;
  int32_t genuinePeak = 150;
  if (numBlocks > 2) {
    for (int b = 1; b < numBlocks - 1; b++) {
      int32_t blockPeak = 0;
      for (int i = 0; i < 256; i++) {
        int32_t val = abs(audioBuffer[b * 256 + i]);
        if (val > blockPeak) blockPeak = val;
      }
      if (blockPeak < 31500 && blockPeak > genuinePeak) {
        genuinePeak = blockPeak;
      }
    }
  } else {
    for (int i = 0; i < recordedSamples; i++) {
      int32_t a = abs(audioBuffer[i]);
      if (a < 31500 && a > genuinePeak) genuinePeak = a;
    }
  }

  // 2. Tính hệ số khuếch đại tự động (Auto Gain) đưa âm lượng lên 26.000 (~80% full công suất loa)
  float autoGain = 26000.0f / (float)genuinePeak;
  if (autoGain > 16.0f) autoGain = 16.0f; // Khuếch đại tối đa x16 cho mic nhỏ
  if (autoGain < 1.0f)  autoGain = 1.0f;  // Không giảm nếu đã to
  Serial.printf("\n[PARROT] Nhai giong: %s (%d mau) | Speech Peak: %d -> AutoGain: %.2fx | Motor: %s\n", 
                modeNames[currentMode], recordedSamples, genuinePeak, autoGain, motorEnabled ? "ON" : "OFF");

  float speed = pitchSpeeds[currentMode];
  float sampleIdx = 0.0f;
  static int32_t txChunk[CHUNK_SIZE * 2];
  uint32_t lastDispUpdate = 0;

  while (sampleIdx < (float)(recordedSamples - 1)) {
    int chunk = 0;
    for (int i = 0; i < CHUNK_SIZE && sampleIdx < (float)(recordedSamples - 1); i++) {
      int idx0 = (int)sampleIdx;
      int idx1 = idx0 + 1;
      float frac = sampleIdx - (float)idx0;

      // Nội suy tuyến tính (Linear Interpolation) cho âm thanh mượt mà
      float s = (1.0f - frac) * (float)audioBuffer[idx0] + frac * (float)audioBuffer[idx1];
      
      // Khuếch đại âm lượng tự động
      int32_t val16 = (int32_t)constrain((int)(s * autoGain), -32767, 32767);
      int32_t s32 = val16 << 16;

      txChunk[i * 2]     = s32;
      txChunk[i * 2 + 1] = s32;
      sampleIdx += speed;
      chunk++;
    }

    if (chunk > 0) {
      size_t written = 0;
      i2s_write(I2S_PORT, txChunk, chunk * 2 * sizeof(int32_t), &written, pdMS_TO_TICKS(100));
    }

    // Cập nhật khuôn mặt OLED & cử động nhảy múa của động cơ (20 FPS / 50ms)
    if (millis() - lastDispUpdate >= 50) {
      lastDispUpdate = millis();
      mouthAnimTick++;
      updateDisplay();
      stepDanceMotion(mouthAnimTick);
    }
  }

  // Dừng động cơ sau khi nói xong
  stopAllMotors();

  // Tắt tiếng đệm sau khi phát xong
  static int32_t zeroBuf[CHUNK_SIZE * 2] = {0};
  size_t w = 0;
  i2s_write(I2S_PORT, zeroBuf, sizeof(zeroBuf), &w, pdMS_TO_TICKS(50));

  // Xả sạch âm thanh loa phát ra còn sót lại trong đệm Micro để chống feedback tự nhại chính mình
  delay(120);
  int32_t dumpBuf[CHUNK_SIZE * 2];
  size_t dumpRead = 0;
  for (int d = 0; d < 8; d++) {
    i2s_read(I2S_PORT, dumpBuf, sizeof(dumpBuf), &dumpRead, 0);
  }
  currentRms = 0.0f;
  lastPlaybackEnd = millis();

  Serial.println("[PARROT] Da phat xong! San sang nghe tiep...");
  recordedSamples = 0;
  robotState = STATE_IDLE_WAIT;
  neopixelWrite(STATUS_LED_PIN, 0, 0, 40); // Xanh dương báo chờ
  updateDisplay();
}

// ============================================================================
// 5. HIỂN THỊ ĐỒ HỌA OLED: MẮT BIỂU CẢM ROBOEYES & KHUÔN MIỆNG CỬ ĐỘNG
// ============================================================================
void drawRoboEyes(int eyeH, int lookOffset = 0) {
  int eyeW = 34;
  int leftEyeX = 22 + lookOffset;
  int rightEyeX = 72 + lookOffset;
  int eyeY = 16 + (28 - eyeH) / 2;

  display.fillRoundRect(leftEyeX, eyeY, eyeW, eyeH, 6, SSD1306_WHITE);
  display.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 6, SSD1306_WHITE);

  // Con ngươi mắt & điểm sáng phản chiếu
  if (eyeH > 10) {
    display.fillCircle(leftEyeX + 14, 30, 4, SSD1306_BLACK);
    display.fillCircle(rightEyeX + 14, 30, 4, SSD1306_BLACK);
    display.fillCircle(leftEyeX + 16, 28, 2, SSD1306_WHITE);
    display.fillCircle(rightEyeX + 16, 28, 2, SSD1306_WHITE);
  }
}

void drawMouth(int mouthH) {
  int mW = 38;
  int mX = (128 - mW) / 2;
  int mY = 48 + (10 - mouthH) / 2;
  if (mouthH <= 2) {
    display.drawFastHLine(mX, 52, mW, SSD1306_WHITE);
  } else {
    display.fillRoundRect(mX, mY, mW, mouthH, 3, SSD1306_WHITE);
  }
}

void updateDisplay() {
  if (!oledReady) return;
  display.clearDisplay();

  // 1. Header trên cùng hiển thị Mode và Trạng thái Motor
  display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.printf("%s", modeNames[currentMode]);
  display.setCursor(92, 2);
  display.printf("[%s]", motorEnabled ? "M:ON" : "M:OFF");

  display.setTextColor(SSD1306_WHITE);

  // 2. Nội dung theo trạng thái
  if (robotState == STATE_IDLE_WAIT) {
    // Trạng thái Chờ: Chớp mắt tự nhiên và liếc nhìn sinh động
    static uint32_t lastBlink = 0;
    static int blinkState = 0;
    static int lookDir = 0;

    if (millis() - lastBlink > 3000) {
      lastBlink = millis();
      blinkState = 1;
      // Ngẫu nhiên đổi hướng nhìn
      int r = random(0, 3);
      lookDir = (r == 0) ? -4 : ((r == 1) ? 4 : 0);
    }

    int eyeH = 26;
    if (blinkState == 1) {
      eyeH = 4; // Nhắm mắt chớp
      if (millis() - lastBlink > 150) blinkState = 0;
    }

    drawRoboEyes(eyeH, lookDir);
    drawMouth(2); // Miệng mỉm cười nhẹ

    // Thang sóng âm thanh môi trường nhỏ ở góc dưới
    int vu = map(constrain((int)currentRms, 0, 1200), 0, 1200, 0, 36);
    display.setCursor(2, 56);
    display.print("DANG NGHE... [  ]");
    display.fillRect(80, 57, vu, 4, SSD1306_WHITE);

  } else if (robotState == STATE_RECORDING) {
    // Trạng thái Ghi âm: Mắt mở to tròn chăm chú, miệng co giãn theo tiếng
    int mouthH = map(constrain((int)currentRms, 250, 3000), 250, 3000, 3, 10);
    drawRoboEyes(28, 0);
    drawMouth(mouthH);

    // Thanh tiến trình ghi âm
    int recW = map(recordedSamples, 0, maxRecordSamples, 0, 124);
    display.drawRect(2, 57, 124, 6, SSD1306_WHITE);
    if (recW > 0) display.fillRect(2, 57, recW, 6, SSD1306_WHITE);

    display.setCursor(2, 46);
    display.print("* DANG THU GIONG...");

  } else if (robotState == STATE_PLAYBACK) {
    // Trạng thái Nhại lại: Mắt cười biểu cảm, miệng nhấp nhô theo câu nói
    int mH = ((mouthAnimTick % 4) == 0) ? 9 : ((mouthAnimTick % 2 == 0) ? 5 : 2);
    drawRoboEyes(26, 0);
    drawMouth(mH);

    display.setCursor(2, 46);
    display.print(">> ROBOT NHAI LAI >>");

    // Sóng âm thanh nhảy dưới đáy
    for (int x = 2; x < 126; x += 4) {
      int h = (abs((int)(sinf((x + mouthAnimTick * 6) * 0.35f) * 6.0f))) + 1;
      display.drawFastVLine(x, 62 - h, h, SSD1306_WHITE);
    }
  }

  display.display();
}

// ============================================================================
// 6. XỬ LÝ NÚT NHẤN BOOT (GPIO 0): ĐỔI GIỌNG & KHÓA MOTOR CHỐNG RƠI BÀN
// ============================================================================
void checkBootButton() {
  static bool lastBtn = HIGH;
  static uint32_t pressTime = 0;
  static bool longHandled = false;

  bool curBtn = digitalRead(PIN_BOOT_BTN);
  if (lastBtn == HIGH && curBtn == LOW) {
    pressTime = millis();
    longHandled = false;
  } else if (lastBtn == LOW && curBtn == LOW) {
    // Giữ nút > 1.2s: BẬT / TẮT ĐỘNG CƠ DI CHUYỂN
    if (!longHandled && (millis() - pressTime > 1200)) {
      longHandled = true;
      motorEnabled = !motorEnabled;
      stopAllMotors();
      Serial.printf("\n[SAFETY LOCK] CHE DO DONG CO: %s!\n", motorEnabled ? "BAT (ENABLED)" : "TAT (DISABLED - CHONG ROI BAN)");
      
      // Bíp báo hiệu và chớp đèn vàng
      playTone(motorEnabled ? 1400.0f : 500.0f, 150, 0.4f);
      neopixelWrite(STATUS_LED_PIN, 40, 30, 0);
      delay(100);
      neopixelWrite(STATUS_LED_PIN, 0, 0, 40);
      updateDisplay();
    }
  } else if (lastBtn == LOW && curBtn == HIGH) {
    // Bấm nhả ngắn (< 500ms): ĐỔI CHẾ ĐỘ GIỌNG NÓI
    if (!longHandled && (millis() - pressTime > 50)) {
      int next = (int)currentMode + 1;
      if (next > MODE_DEEP_ROBOT) next = MODE_CHIPMUNK;
      currentMode = (VoiceMode)next;

      Serial.printf("\n[MODE] Doi sang che do giong: %s\n", modeNames[currentMode]);
      playTone(1200.0f, 60, 0.4f);
      neopixelWrite(STATUS_LED_PIN, 40, 20, 0);
      delay(60);
      neopixelWrite(STATUS_LED_PIN, 0, 0, 40);
      updateDisplay();
    }
  }
  lastBtn = curBtn;
}

// ============================================================================
// 7. SETUP VÀ MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // 1. Cấu hình driver động cơ Mini L298N
  initMotors();

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  neopixelWrite(STATUS_LED_PIN, 0, 0, 40); // Xanh dương

  delay(400);
  Serial.println("\n\n==========================================================");
  Serial.println("   XIAOZHI SMART COMPANION ROBOT (ALL-IN-ONE COMBINED)    ");
  Serial.println("  1. Nhai giong thong minh Talking Tom / Chuan / Tram     ");
  Serial.println("  2. Mat bieu cam RoboEyes & mieng cu dong theo tieng     ");
  Serial.println("  3. Dong co banh xe lac lu, nhay mua theo nhip cau noi   ");
  Serial.println("  - Bam nha BOOT (IO0): Doi che do giong                  ");
  Serial.println("  - Giu BOOT > 1.2s: Bat/Tat Dong co (Chong roi ban cao)  ");
  Serial.println("==========================================================");

  // 2. Khởi tạo OLED SSD1306
  initOLED();

  // 3. Khởi tạo I2S Loa & Mic
  initI2SAudio();

  // 4. Cấp phát bộ nhớ RAM lưu âm thanh an toàn
  size_t maxAlloc = ESP.getMaxAllocHeap();
  size_t targetSize = (TOTAL_SAMPLES * sizeof(int16_t));
  if (targetSize > maxAlloc - 16384) {
    targetSize = (maxAlloc > 32768) ? (maxAlloc - 16384) : 20000;
  }
  audioBuffer = (int16_t*)malloc(targetSize);
  maxRecordSamples = targetSize / sizeof(int16_t);
  if (audioBuffer) {
    Serial.printf("[RAM] Da cap phat %d KB bo dem ghi am (%.2fs)!\n", 
                  targetSize / 1024, (float)maxRecordSamples / SAMPLE_RATE);
  } else {
    Serial.println("[ERROR] Khong du RAM de cap phat bo dem ghi am!");
  }

  // 5. Chuông khởi động Mario vui nhộn & nhích nhẹ bánh xe chào mừng
  playStartupMario();
  if (motorEnabled) {
    setMotorA(140); setMotorB(-140); delay(60);
    setMotorA(-140); setMotorB(140); delay(60);
    stopAllMotors();
  }

  // Xả sạch đệm sau khi phát chuông khởi động
  delay(150);
  int32_t dumpBuf[CHUNK_SIZE * 2];
  size_t dumpRead = 0;
  for (int d = 0; d < 8; d++) {
    i2s_read(I2S_PORT, dumpBuf, sizeof(dumpBuf), &dumpRead, 0);
  }
  currentRms = 0.0f;
  lastPlaybackEnd = millis();

  updateDisplay();
  Serial.println("\n>>> [SAN SANG] Hay noi bat ky cau gi vao Micro! <<<");
}

void loop() {
  checkBootButton();

  // 1. Đọc dữ liệu từ micro INMP441
  static int32_t rxChunk[CHUNK_SIZE * 2];
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, rxChunk, sizeof(rxChunk), &bytesRead, 0);

  int frames = bytesRead / (2 * sizeof(int32_t));
  if (frames > 0) {
    int64_t sumSq = 0;
    static int16_t tempSamples[CHUNK_SIZE];
    static float dcX = 0.0f, dcY = 0.0f;

    for (int i = 0; i < frames; i++) {
      // INMP441: Dịch 14-bit để khuếch đại tín hiệu thu vào (Pre-amp +12dB)
      int32_t sL = rxChunk[i * 2] >> 14;
      int32_t sR = rxChunk[i * 2 + 1] >> 14;
      int32_t rawSample = (abs(sL) > abs(sR)) ? sL : sR;

      // Bộ lọc loại bỏ thành phần một chiều (DC Blocker High-Pass Filter @ 20Hz)
      float filtered = (float)rawSample - dcX + 0.995f * dcY;
      dcX = (float)rawSample;
      dcY = filtered;

      int16_t s16 = (int16_t)constrain((int)filtered, -32767, 32767);
      tempSamples[i] = s16;
      sumSq += (int64_t)s16 * s16;
    }

    float rawRms = sqrtf((float)sumSq / frames);
    currentRms = (currentRms * 0.70f) + (rawRms * 0.30f);

    // 2. Máy trạng thái thu âm tự động (Voice Activity Detection)
    if (robotState == STATE_IDLE_WAIT) {
      // Tự động thích ứng với mức ồn nền phòng (Adaptive Noise Floor)
      noiseFloor = (noiseFloor * 0.98f) + (currentRms * 0.02f);
      if (noiseFloor < 50.0f) noiseFloor = 50.0f;
      float speechThreshold = noiseFloor + 180.0f;
      if (speechThreshold < 250.0f) speechThreshold = 250.0f;

      // Tránh echo: Chờ tối thiểu 400ms sau khi phát loa xong mới cho phép ghi âm tiếp
      if (millis() - lastPlaybackEnd > 400) {
        // Phát hiện người dùng bắt đầu nói (vượt ngưỡng độ ồn nền)
        if (currentRms > speechThreshold) {
          robotState = STATE_RECORDING;
          recordedSamples = 0;
          speechStartTime = millis();
          lastVoiceTime = millis();
          neopixelWrite(STATUS_LED_PIN, 50, 0, 0); // Đỏ báo đang ghi âm
          Serial.printf("\n[MIC] >> Phat hien tieng noi (RMS: %.0f > Nguong: %.0f), dang ghi am...\n", currentRms, speechThreshold);

          // Phản ứng nhích nhẹ bánh xe biểu cảm "Tôi đang nghe bạn!"
          triggerListeningNod();

          // Lưu những mẫu đầu tiên
          for (int i = 0; i < frames && recordedSamples < maxRecordSamples; i++) {
            if (audioBuffer) audioBuffer[recordedSamples++] = tempSamples[i];
          }
        }
      }

    } else if (robotState == STATE_RECORDING) {
      // Đang trong quá trình ghi âm
      for (int i = 0; i < frames && recordedSamples < maxRecordSamples; i++) {
        if (audioBuffer) audioBuffer[recordedSamples++] = tempSamples[i];
      }

      // Khi biên độ còn trên ngưỡng ồn + 80 -> Người dùng vẫn đang nói
      if (currentRms > noiseFloor + 80.0f) {
        lastVoiceTime = millis();
      }

      // Điều kiện kết thúc ghi âm:
      // a) Người dùng dừng nói quá SILENCE_TIMEOUT_MS (550ms)
      // b) Đã ghi đầy bộ đệm tối đa (3.5 giây)
      bool silenceTimeout = (millis() - lastVoiceTime > SILENCE_TIMEOUT_MS);
      bool bufferFull = (recordedSamples >= maxRecordSamples);

      if (silenceTimeout || bufferFull) {
        Serial.printf("[MIC] >> Ket thuc noi! Thu duoc: %d mau (%.2fs)\n", 
                      recordedSamples, (float)recordedSamples / SAMPLE_RATE);
        
        // Chuyển sang nhại lại giọng ngay lập tức!
        playMimicVoice();
      }
    }
  }

  // Cập nhật màn hình OLED (20 FPS / 50ms)
  static uint32_t lastOled = 0;
  if (millis() - lastOled > 50) {
    lastOled = millis();
    if (oledReady) {
      updateDisplay();
    } else if (millis() % 3000 < 50) {
      initOLED();
    }
  }

  delay(2);
}
