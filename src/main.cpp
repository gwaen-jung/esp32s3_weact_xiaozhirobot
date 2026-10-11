/**
 * ============================================================================
 * XIAOZHI ROBOT DOG - BÀI TEST TỔNG HỢP TOÀN BỘ PHẦN CỨNG (ALL-IN-ONE HARDWARE TEST)
 * Board: WeAct Studio ESP32-S3 CoreBoard N16R8 (16MB Flash, 8MB Octal PSRAM)
 * Cổng COM Nạp: COM18 (CH343 UART) / COM19 (Native USB CDC)
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI CHI TIẾT (WIRING):
 * 1. MÀN HÌNH TFT LCD 2.4" SPI (Controller: ILI9341, Resolution: 240x320)
 *    - SCLK / SCK : GPIO 40
 *    - MOSI / SDI : GPIO 41
 *    - CS         : GPIO 39
 *    - DC / RS    : GPIO 38
 *    - RESET      : GPIO 47
 *    - LED        : 3.3V (Đèn nền)
 *    - VCC / GND  : 3.3V / GND
 * 
 * 2. ÂM THANH FULL-DUPLEX I2S:
 *    - DÙNG CHUNG CLOCK: BCLK = GPIO 16, WS (LRC) = GPIO 17
 *    - MICROPHONE INMP441:
 *        + SCK : GPIO 16
 *        + WS  : GPIO 17
 *        + SD  : GPIO 14 (Data In vào ESP32)
 *        + L/R : GND (Kênh trái)
 *        + VDD : 3.3V
 *    - LOA & DAC WeAct PCM5100A:
 *        + BCK : GPIO 16
 *        + WS  : GPIO 17
 *        + DIN : GPIO 15 (Data Out từ ESP32)
 *        + MC  : GND (BẮT BUỘC: Nối GND để bật PLL xung nội!)
 *        + SD  : 3.3V / VCC (Kéo HIGH để Unmute phát âm thanh)
 *        + VCC : 5.0V (Nguồn cấp cho mạch công suất)
 * 
 * 3. 4 SERVO CHÂN ROBOT (SG90 / MG90S):
 *    - Chân Trước Trái (FL): GPIO 6
 *    - Chân Trước Phải (FR): GPIO 7
 *    - Chân Sau Trái   (RL): GPIO 4
 *    - Chân Sau Phải   (RR): GPIO 5
 *    - Nguồn 5V ngoài & Chung GND với ESP32
 *    * GHI CHÚ: Hiện có 2 servo bị kẹt/hỏng cơ học, cần thay thế hoặc hạn chế góc.
 * 
 * 4. DỰ KIẾN KẾ HOẠCH TIẾP THEO:
 *    - Cảm biến khoảng cách Laser ToF: VL53L0X (I2C SDA=8, SCL=9)
 *    - Cảm biến góc nghiêng con quay hồi chuyển: MPU6050 (I2C SDA=8, SCL=9)
 *    - Tích hợp Voice AI Chatbot Xiaozhi Client
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <driver/i2s.h>
#include <math.h>

// --- CẤU HÌNH PIN MÀN HÌNH ILI9341 (FSPI) ---
#define TFT_CS   39
#define TFT_DC   38
#define TFT_RST  47
#define TFT_MOSI 41
#define TFT_SCLK 40
#define TFT_MISO -1

static SPIClass tftSPI(FSPI);
static Adafruit_ILI9341 tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// --- CẤU HÌNH PIN I2S (SHARED BCLK/WS) ---
#define I2S_PORT       I2S_NUM_0
#define I2S_PIN_BCLK   16
#define I2S_PIN_WS     17
#define I2S_PIN_DIN    14  // Mic INMP441 Data
#define I2S_PIN_DOUT   15  // Amp PCM5100A Data
#define AUDIO_RATE     16000

// --- CẤU HÌNH 4 SERVO ---
#define PIN_SERVO_FL   6
#define PIN_SERVO_FR   7
#define PIN_SERVO_RL   4
#define PIN_SERVO_RR   5

const int SERVO_PINS[] = {PIN_SERVO_FL, PIN_SERVO_FR, PIN_SERVO_RL, PIN_SERVO_RR};
const char* LEG_NAMES[] = {"FL (Tr-Tr)", "FR (Tr-Ph)", "RL (Sau-Tr)", "RR (Sau-Ph)"};
const int NUM_SERVOS = 4;
int currentAngles[NUM_SERVOS] = {90, 90, 90, 90};

// --- NÚT BẤM BOOT ---
#define PIN_BOOT_BTN   0

// ----------------------------------------------------------------------------
// KHỞI TẠO SERVO
// ----------------------------------------------------------------------------
void writeServoDuty(int channel, int angle) {
  if (angle < 45) angle = 45;
  if (angle > 135) angle = 135;
  // 50Hz (chu kỳ 20ms). 14-bit resolution (0-16383)
  int duty = map(angle, 0, 180, 410, 2048);
  ledcWrite(channel, duty);
}

void setupServos() {
  Serial.println("[SERVO] Khoi tao 4 kenh PWM cho Servos...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    ledcSetup(i, 50, 14);
    ledcAttachPin(SERVO_PINS[i], i);
    writeServoDuty(i, 90);
    currentAngles[i] = 90;
    delay(100); // Khởi động so le tránh sụt nguồn áp
  }
  Serial.println("[SERVO] Hoan tat khoi tao Servo o goc 90 do.");
}

// ----------------------------------------------------------------------------
// KHỞI TẠO I2S FULL-DUPLEX (MIC + DAC/AMP)
// ----------------------------------------------------------------------------
void setupI2SAudio() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
      .sample_rate = AUDIO_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
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
      .bck_io_num = I2S_PIN_BCLK,
      .ws_io_num = I2S_PIN_WS,
      .data_out_num = I2S_PIN_DOUT,
      .data_in_num = I2S_PIN_DIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  Serial.println("[AUDIO] I2S Full-Duplex (PCM5100A + INMP441) da san sang.");
}

// Phát âm đơn tần số sine (Bip / Chime)
void playBeep(float freqHz, int durationMs, float volume = 0.4f) {
  int totalSamples = (AUDIO_RATE * durationMs) / 1000;
  int16_t buffer[256 * 2]; // Stereo: Left + Right
  int samplesLeft = totalSamples;
  float phase = 0.0f;
  float phaseInc = (2.0f * M_PI * freqHz) / AUDIO_RATE;

  while (samplesLeft > 0) {
    int chunk = (samplesLeft > 256) ? 256 : samplesLeft;
    for (int i = 0; i < chunk; i++) {
      int16_t sample = (int16_t)(sinf(phase) * (32767.0f * volume));
      buffer[i * 2]     = sample; // L
      buffer[i * 2 + 1] = sample; // R
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, buffer, chunk * 4, &bytesWritten, portMAX_DELAY);
    samplesLeft -= chunk;
  }
}

// Phát nhạc khởi động ngắn
void playStartupMelody() {
  playBeep(523.25f, 90, 0.3f); // C5
  delay(30);
  playBeep(659.25f, 90, 0.3f); // E5
  delay(30);
  playBeep(783.99f, 90, 0.3f); // G5
  delay(30);
  playBeep(1046.50f, 180, 0.35f); // C6
}

// Đọc mức âm thanh từ Mic INMP441
float readMicLevel() {
  int16_t micBuffer[128 * 2];
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, micBuffer, sizeof(micBuffer), &bytesRead, 10 / portTICK_PERIOD_MS);
  if (bytesRead == 0) return 0.0f;

  int samples = bytesRead / 4;
  int64_t sumSq = 0;
  for (int i = 0; i < samples; i++) {
    int16_t val = micBuffer[i * 2]; // Lấy kênh Mic (kênh Trái)
    sumSq += (int64_t)val * val;
  }
  return sqrtf((float)sumSq / samples);
}

// ----------------------------------------------------------------------------
// GIAO DIỆN MÀN HÌNH HUD & ROBO-EYES
// ----------------------------------------------------------------------------
void drawEye(int cx, int cy, int w, int h, uint16_t color, int pupOffsetX = 0, int pupOffsetY = 0) {
  tft.fillRoundRect(cx - w/2, cy - h/2, w, h, 14, color);
  // Con ngươi mắt
  int pw = w / 2;
  int ph = h / 2;
  tft.fillRoundRect(cx + pupOffsetX - pw/2, cy + pupOffsetY - ph/2, pw, ph, 8, ILI9341_BLACK);
  // Điểm sáng phản chiếu
  tft.fillCircle(cx + pupOffsetX - pw/4, cy + pupOffsetY - ph/4, 4, ILI9341_WHITE);
}

void drawHUD(const char* poseName, uint16_t eyeColor, int eyeH = 60, int pupX = 0, int pupY = 0) {
  tft.fillScreen(ILI9341_BLACK);

  // Khung tiêu đề
  tft.fillRect(0, 0, 320, 24, 0x18C3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 7);
  tft.print("XIAOZHI ROBOT DOG - WEACT N16R8 HARDWARE SUITE");

  // Vẽ 2 mắt RoboEyes
  drawEye(105, 95, 65, eyeH, eyeColor, pupX, pupY);
  drawEye(215, 95, 65, eyeH, eyeColor, pupX, pupY);

  // Tên tư thế / Trạng thái
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  int nameX = 160 - (strlen(poseName) * 6);
  if (nameX < 10) nameX = 10;
  tft.setCursor(nameX, 150);
  tft.print(poseName);

  // Bảng thông số góc 4 Servo
  tft.drawFastHLine(10, 178, 300, 0x4208);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(15, 188);
  tft.printf("FL (G6): %d*", currentAngles[0]);
  tft.setCursor(95, 188);
  tft.printf("FR (G7): %d*", currentAngles[1]);
  tft.setCursor(175, 188);
  tft.printf("RL (G4): %d*", currentAngles[2]);
  tft.setCursor(250, 188);
  tft.printf("RR (G5): %d*", currentAngles[3]);

  // Thanh trạng thái dưới đáy
  tft.fillRect(0, 218, 320, 22, 0x10A2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(8, 224);
  tft.print("MIC: INMP441 | AMP: PCM5100A | BOOT: Sweep Test");
}

// Cập nhật thanh mức âm lượng Mic thời gian thực
void updateMicVUMeter(float level) {
  int barWidth = map((int)constrain(level, 0, 8000), 0, 8000, 0, 80);
  tft.fillRect(230, 224, 82, 10, ILI9341_BLACK);
  uint16_t col = (barWidth > 50) ? ILI9341_RED : (barWidth > 25 ? ILI9341_YELLOW : ILI9341_GREEN);
  tft.fillRect(230, 224, barWidth, 10, col);
}

// ----------------------------------------------------------------------------
// ĐIỀU KHIỂN CHUYỂN ĐỘNG SERVO MƯỢT (COSINE RAMP)
// ----------------------------------------------------------------------------
void setPoseSmooth(const char* poseName, int targetFL, int targetFR, int targetRL, int targetRR,
                   int durationMs = 500, uint16_t eyeCol = ILI9341_CYAN, int eyeH = 60,
                   int pupX = 0, int pupY = 0) {
  drawHUD(poseName, eyeCol, eyeH, pupX, pupY);

  int startAngles[4] = {currentAngles[0], currentAngles[1], currentAngles[2], currentAngles[3]};
  int targets[4]     = {targetFL, targetFR, targetRL, targetRR};

  int steps = durationMs / 20;
  if (steps < 1) steps = 1;

  for (int s = 1; s <= steps; s++) {
    float t = (float)s / steps;
    float factor = (1.0f - cosf(t * M_PI)) / 2.0f; // Cosine S-curve

    for (int i = 0; i < NUM_SERVOS; i++) {
      int deg = (int)(startAngles[i] + (targets[i] - startAngles[i]) * factor);
      writeServoDuty(i, deg);
      currentAngles[i] = deg;
    }

    // Đọc mic nhẹ trong lúc chuyển động
    float micVal = readMicLevel();
    updateMicVUMeter(micVal);

    delay(20);
  }

  // Cập nhật giá trị góc chính xác cuối cùng
  for (int i = 0; i < NUM_SERVOS; i++) {
    writeServoDuty(i, targets[i]);
    currentAngles[i] = targets[i];
  }
}

// ----------------------------------------------------------------------------
// SETUP & LOOP
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  delay(1000);

  Serial.println("\n========================================================");
  Serial.println("  XIAOZHI ROBOT DOG - WEACT N16R8 ALL-IN-ONE HARDWARE TEST");
  Serial.println("========================================================");

  // Khởi động màn hình ILI9341
  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(27000000);
  tft.setRotation(1);

  // Khởi tạo Audio và Servos
  setupI2SAudio();
  setupServos();

  // Màn hình khởi động & Âm thanh Chime
  drawHUD("KHOI DONG...", ILI9341_CYAN, 60);
  playStartupMelody();
  delay(1000);
}

void loop() {
  // KIỂM TRA NÚT BẤM BOOT (Nếu nhấn -> Chạy chế độ Quét Đồng Loạt 45-135)
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    playBeep(1000.0f, 100);
    setPoseSmooth("DONG LOAT: 45 DO", 45, 45, 45, 45, 600, ILI9341_YELLOW, 60);
    delay(1500);
    setPoseSmooth("DONG LOAT: 135 DO", 135, 135, 135, 135, 600, ILI9341_MAGENTA, 60);
    delay(1500);
    setPoseSmooth("DONG LOAT: 90 DO", 90, 90, 90, 90, 500, ILI9341_CYAN, 60);
    delay(1000);
    return;
  }

  // TƯ THẾ 1: STAND (ĐỨNG CÂN BẰNG 90 ĐỘ)
  playBeep(523.25f, 60, 0.25f);
  setPoseSmooth("STAND (Dung)", 90, 90, 90, 90, 600, ILI9341_CYAN, 60);
  for (int d = 0; d < 20; d++) {
    updateMicVUMeter(readMicLevel());
    delay(100);
  }

  // TƯ THẾ 2: SIT (NGỒI XUỐNG)
  playBeep(659.25f, 60, 0.25f);
  setPoseSmooth("SIT (Ngoi xuong)", 100, 100, 120, 120, 700, ILI9341_GREEN, 50);
  for (int d = 0; d < 20; d++) {
    updateMicVUMeter(readMicLevel());
    delay(100);
  }

  // TƯ THẾ 3: WAVE (VẪY CHÂN CHÀO)
  playBeep(783.99f, 60, 0.25f);
  for (int w = 0; w < 3; w++) {
    setPoseSmooth("WAVE (Vay chao)", 100, 125, 120, 120, 200, ILI9341_YELLOW, 70, 15, -10);
    delay(80);
    setPoseSmooth("WAVE (Vay chao)", 100, 95, 120, 120, 200, ILI9341_YELLOW, 70, -15, -10);
    delay(80);
  }
  delay(500);

  // TƯ THẾ 4: CROUCH (CÚI THẤP XUỐNG)
  playBeep(440.0f, 80, 0.25f);
  setPoseSmooth("CROUCH (Cui thap)", 65, 65, 65, 65, 600, ILI9341_ORANGE, 40);
  for (int d = 0; d < 20; d++) {
    updateMicVUMeter(readMicLevel());
    delay(100);
  }

  // TRỞ LẠI STAND
  setPoseSmooth("STAND (Dung)", 90, 90, 90, 90, 500, ILI9341_CYAN, 60);
  delay(1500);
}
