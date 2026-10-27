/**
 * ============================================================================
 * XIAOZHI ROBOT - CHƯƠNG TRÌNH NHẠI GIỌNG THÔNG MINH (VOICE PARROT / TALKING TOM)
 * ============================================================================
 * Phần cứng:
 *  - Vi điều khiển: ESP32-S3 Clone (8MB PSRAM)
 *  - Micro INMP441: BCLK=IO11, WS=IO12, SD=IO14
 *  - Loa MAX98357A: BCLK=IO11, LRC=IO12, DIN=IO13
 *  - Màn hình OLED SSD1306: SDA=IO1, SCL=IO2 (Địa chỉ 0x3C)
 *  - LED RGB WS2812: GPIO 48
 *  - Nút BOOT: GPIO 0 (Bấm để đổi chế độ giọng: Chipmunk -> Bình thường -> Robot)
 *  - Toàn bộ Motor: TẮT HOÀN TOÀN để tránh nhiễu âm
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <math.h>

// --- PINOUT CẤU HÌNH ---
#define PIN_OLED_SDA  1
#define PIN_OLED_SCL  2

#define PIN_I2S_BCLK  11
#define PIN_I2S_WS    12
#define PIN_I2S_DIN   14   // Mic INMP441 SD
#define PIN_I2S_DOUT  13   // Loa MAX98357A DIN

#define PIN_MOTOR_1   4
#define PIN_MOTOR_2   5
#define PIN_MOTOR_3   6
#define PIN_MOTOR_4   7
#define PIN_LED       48
#define PIN_BOOT      0

// --- CẤU HÌNH ÂM THANH ---
#define I2S_PORT        I2S_NUM_0
#define SAMPLE_RATE     16000     // 16 kHz chuẩn giọng nói
#define CHUNK_SIZE      256
#define MAX_RECORD_SEC  3.5f      // Ghi âm 3.5 giây (khoảng 1 câu nói)
#define TOTAL_SAMPLES   ((int)(SAMPLE_RATE * MAX_RECORD_SEC))

// Ngưỡng phát hiện tiếng nói (VAD - Voice Activity Detection)
#define NOISE_THRESHOLD_START  320.0f  // Biên độ bắt đầu nói (đã chuẩn hóa >> 16)
#define SILENCE_TIMEOUT_MS     550     // Dừng nói quá 550ms thì bắt đầu nhại lại
#define MIN_SPEECH_DURATION_MS 350     // Nói tối thiểu 350ms mới phát lại

// --- BỘ NHỚ LƯU ÂM THANH ---
static int16_t* audioBuffer = nullptr;
static int recordedSamples = 0;
static int maxRecordSamples = TOTAL_SAMPLES;
static uint32_t lastPlaybackEnd = 0; // Chống echo feedback

// --- MÀN HÌNH OLED ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static bool oledReady = false;

// --- CHẾ ĐỘ GIỌNG (VOICE MODES) ---
enum VoiceMode {
  MODE_CHIPMUNK = 0,   // Giọng chuột sóc / Talking Tom (Cao độ x1.35)
  MODE_NORMAL,         // Giọng gốc chuẩn 100%
  MODE_DEEP_ROBOT      // Giọng người máy trầm (Hạ cao độ x0.80)
};

static VoiceMode currentMode = MODE_CHIPMUNK;
static const char* modeNames[] = {"TALKING TOM", "GIONG CHUAN", "ROBOT TRAM"};
static float pitchSpeeds[]     = {1.35f, 1.0f, 0.80f};

// --- TRẠNG THÁI HỆ THỐNG ---
enum RobotState {
  STATE_IDLE_WAIT = 0, // Đang lắng nghe, chờ người dùng nói
  STATE_RECORDING,     // Đang ghi âm giọng nói
  STATE_PLAYBACK       // Đang nhại lại qua loa
};

static RobotState robotState = STATE_IDLE_WAIT;
static float currentRms = 0.0f;
static uint32_t speechStartTime = 0;
static uint32_t lastVoiceTime = 0;
static int mouthAnimTick = 0;

// Khai báo hàm
void updateDisplay();
void drawMouth(int openLevel);

// ============================================================================
// 1. KHỞI TẠO MÀN HÌNH OLED SSD1306 (SDA=1, SCL=2, 0x3C)
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
    delay(25);

    // Thử địa chỉ 0x3C trước, nếu không được thử 0x3D
    uint8_t addrs[] = {0x3C, 0x3D};
    for (uint8_t addr : addrs) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        if (display.begin(SSD1306_SWITCHCAPVCC, addr, false, false)) {
          oledReady = true;

          display.ssd1306_command(SSD1306_DISPLAYON);
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(0xFF);

          // Chớp trắng toàn màn hình xác nhận hiển thị
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
}

// ============================================================================
// 2. KHỞI TẠO I2S FULL-DUPLEX (MIC INMP441 & LOA MAX98357A DÙNG CHUNG CLOCK)
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

// ============================================================================
// 3. TIẾNG BÍP BÁO HIỆU
// ============================================================================
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

// ============================================================================
// 4. PHÁT LẠI GIỌNG NÓI VỚI THUẬT TOÁN PITCH SHIFT (NHẠI GIỌNG)
// ============================================================================
void playMimicVoice() {
  if (recordedSamples < (int)(SAMPLE_RATE * (MIN_SPEECH_DURATION_MS / 1000.0f))) {
    Serial.println("[PARROT] Doan noi qua ngan, bo qua.");
    robotState = STATE_IDLE_WAIT;
    return;
  }

  robotState = STATE_PLAYBACK;
  neopixelWrite(PIN_LED, 0, 50, 0); // Xanh lá báo phát loa

  // 1. Tìm biên độ đỉnh thực tế của giọng nói (loại trừ xung nhiễu click/pop ở đầu/cuối)
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
  Serial.printf("\n[PARROT] Nhai giong: %s (%d mau) | Speech Peak: %d -> AutoGain: %.2fx\n", 
                modeNames[currentMode], recordedSamples, genuinePeak, autoGain);

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

    // Cập nhật OLED khoảng 20 FPS (50ms) để không làm nghẽn luồng âm thanh
    if (millis() - lastDispUpdate >= 50) {
      lastDispUpdate = millis();
      mouthAnimTick++;
      updateDisplay();
    }
  }

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
  neopixelWrite(PIN_LED, 0, 0, 40); // Xanh dương báo chờ
  updateDisplay();
}

// ============================================================================
// 5. HIỂN THỊ OLED TRỰC QUAN & ĐỒ HỌA MIỆNG ROBOT NHẢY THEO TIẾNG
// ============================================================================
void drawRobotFace(int eyeH, int mouthH) {
  // 2 Mắt
  display.fillRoundRect(24, 18 + (24 - eyeH) / 2, 28, eyeH, 5, SSD1306_WHITE);
  display.fillRoundRect(76, 18 + (24 - eyeH) / 2, 28, eyeH, 5, SSD1306_WHITE);

  if (eyeH > 8) {
    display.fillCircle(38, 30, 4, SSD1306_BLACK);
    display.fillCircle(90, 30, 4, SSD1306_BLACK);
  }

  // Miệng robot cử động theo nhịp nói
  int mW = 36;
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

  // Header đảo màu
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(6, 2);
  display.printf("PARROT: %s", modeNames[currentMode]);

  display.setTextColor(SSD1306_WHITE);

  if (robotState == STATE_IDLE_WAIT) {
    // Trạng thái chờ nghe
    drawRobotFace(24, 2);

    // Cột sóng âm thanh môi trường
    int vu = map(constrain((int)currentRms, 0, 1500), 0, 1500, 0, 40);
    display.setCursor(2, 56);
    display.print("HAY NOI DI! [  ]");
    display.fillRect(76, 57, vu, 4, SSD1306_WHITE);

  } else if (robotState == STATE_RECORDING) {
    // Trạng thái đang ghi âm
    int eyeH = 22;
    int mouthH = map(constrain((int)currentRms, 400, 2500), 400, 2500, 3, 10);
    drawRobotFace(eyeH, mouthH);

    // Thanh tiến trình ghi âm
    int recW = map(recordedSamples, 0, maxRecordSamples, 0, 124);
    display.drawRect(2, 57, 124, 6, SSD1306_WHITE);
    if (recW > 0) display.fillRect(2, 57, recW, 6, SSD1306_WHITE);

    display.setCursor(2, 46);
    display.print("* DANG THU AM...");

  } else if (robotState == STATE_PLAYBACK) {
    // Trạng thái đang nhại giọng
    int mH = ((mouthAnimTick % 4) == 0) ? 8 : ((mouthAnimTick % 2 == 0) ? 4 : 2);
    drawRobotFace(26, mH);

    display.setCursor(2, 46);
    display.print(">> DANG NHAI LAI >>");

    // Sóng âm thanh nhại
    for (int x = 2; x < 126; x += 4) {
      int h = (abs((int)(sinf((x + mouthAnimTick * 6) * 0.3f) * 6.0f))) + 1;
      display.drawFastVLine(x, 62 - h, h, SSD1306_WHITE);
    }
  }

  display.display();
}

// ============================================================================
// 6. XỬ LÝ NÚT BẤM BOOT (GPIO 0) ĐỔI CHẾ ĐỘ GIỌNG
// ============================================================================
void checkBootButton() {
  static bool lastBtn = HIGH;
  static uint32_t pressTime = 0;

  bool curBtn = digitalRead(PIN_BOOT);
  if (lastBtn == HIGH && curBtn == LOW) {
    pressTime = millis();
  } else if (lastBtn == LOW && curBtn == HIGH) {
    if (millis() - pressTime > 50) {
      // Đổi chế độ giọng
      currentMode = (VoiceMode)((currentMode + 1) % 3);
      Serial.printf("\n[MODE] Chuyen sang che do: %s\n", modeNames[currentMode]);
      
      // Bíp 1 tiếng báo đổi chế độ thành công
      playTone(1200.0f, 60, 0.4f);
      neopixelWrite(PIN_LED, 40, 20, 0); // Vàng
      delay(60);
      neopixelWrite(PIN_LED, 0, 0, 40); // Xanh dương
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

  // Tắt hoàn toàn motor
  pinMode(PIN_MOTOR_1, OUTPUT); digitalWrite(PIN_MOTOR_1, LOW);
  pinMode(PIN_MOTOR_2, OUTPUT); digitalWrite(PIN_MOTOR_2, LOW);
  pinMode(PIN_MOTOR_3, OUTPUT); digitalWrite(PIN_MOTOR_3, LOW);
  pinMode(PIN_MOTOR_4, OUTPUT); digitalWrite(PIN_MOTOR_4, LOW);

  pinMode(PIN_BOOT, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  neopixelWrite(PIN_LED, 0, 0, 40); // Xanh dương

  delay(400);
  Serial.println("\n\n==========================================================");
  Serial.println("  XIAOZHI ROBOT - BỘ NHẠI GIỌNG THÔNG MINH (VOICE PARROT)   ");
  Serial.println("  Noi mot cau -> Robot se tu dong nhai lai ngay lap tuc!    ");
  Serial.println("  Bam nut BOOT (IO0) de doi giong: Talking Tom / Chuan / Robot");
  Serial.println("==========================================================");

  // 1. Khởi tạo OLED
  initOLED();

  // 2. Khởi tạo Loa & Mic
  initI2SAudio();

  // 3. Cấp phát bộ nhớ RAM lưu âm thanh từ phần heap còn lại an toàn
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

  // 4. Bíp chào mừng
  playTone(880.0f, 100, 0.4f);
  playTone(1320.0f, 120, 0.5f);

  // Xả sạch âm thanh loa phát ra khỏi đệm mic để không tự kích hoạt ghi âm
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
    static float noiseFloor = 100.0f;
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
          neopixelWrite(PIN_LED, 50, 0, 0); // Đỏ báo đang ghi âm
          Serial.printf("\n[MIC] >> Phat hien tieng noi (RMS: %.0f > Nguong: %.0f), dang ghi am...\n", currentRms, speechThreshold);

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
        lastVoiceTime = millis(); // Cập nhật thời điểm người dùng vẫn đang nói
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

  // Cập nhật màn hình OLED (20 FPS)
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
