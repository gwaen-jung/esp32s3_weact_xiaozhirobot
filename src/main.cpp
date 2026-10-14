/**
 * ============================================================================
 * XIAOZHI ROBOT DOG - BÀI TEST TỔNG HỢP TOÀN BỘ PHẦN CỨNG (ALL-IN-ONE HARDWARE TEST)
 * Board: WeAct Studio ESP32-S3 CoreBoard N16R8 (16MB Flash, 8MB Octal PSRAM)
 * Cổng COM Nạp + Serial Monitor: COM18 (CH343 UART)
 * ============================================================================
 *
 * SƠ ĐỒ ĐẤU NỐI (giá trị thật lấy từ platformio.ini):
 * 1. TFT LCD 2.4" SPI ILI9341 (240x320): SCLK 40, MOSI 41, CS 39, DC 38, RST 47
 *    LED -> 3.3V, VCC/GND -> 3.3V/GND
 *
 * 2. ÂM THANH FULL-DUPLEX I2S (BCLK = 16, WS = 17 dùng chung):
 *    - INMP441: SCK 16, WS 17, SD 14, L/R -> GND, VDD 3.3V
 *    - PCM5100A: BCK 16, WS 17, DIN 15, SCK(MC) -> GND (PLL nội), XSMT(SD) -> 3.3V, VCC 5V
 *    - I2S chạy 32-bit/slot (64*fs) vì INMP441 yêu cầu 64 xung SCK mỗi khung WS.
 *
 * 3. 4 SERVO (SG90 / MG90S), nguồn 5V ngoài (>= 3A) chung GND:
 *    - FL (Trước Trái) : GPIO 7  - 180°
 *    - FR (Trước Phải) : GPIO 4  - SERVO 360° (chỉ giữ điểm dừng, không điều khiển góc)
 *    - RL (Sau Trái)   : GPIO 6  - 180°
 *    - RR (Sau Phải)   : GPIO 5  - 180°
 *
 * 4. NÚT BOOT (GPIO 0): nhấn bất kỳ lúc nào -> quét đồng loạt 45/135/90.
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <driver/i2s.h>
#include <math.h>

// --- PIN MÀN HÌNH (fallback nếu không build qua platformio.ini) ---
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

// --- PIN I2S ---
#ifndef I2S_PIN_BCLK
#define I2S_PIN_BCLK 16
#define I2S_PIN_WS   17
#define I2S_PIN_DIN  14
#define I2S_PIN_DOUT 15
#endif
#define I2S_PORT   I2S_NUM_0
#define AUDIO_RATE 16000

// --- PIN SERVO ---
#ifndef SERVO_PIN_FL
#define SERVO_PIN_FL 7
#define SERVO_PIN_FR 4
#define SERVO_PIN_RL 6
#define SERVO_PIN_RR 5
#endif
#ifndef SERVO_FR_IS_360
#define SERVO_FR_IS_360 1
#endif

#ifndef PIN_BOOT_BTN
#define PIN_BOOT_BTN 0
#endif

enum { LEG_FL = 0, LEG_FR, LEG_RL, LEG_RR, NUM_SERVOS };
const int SERVO_PINS[NUM_SERVOS] = {SERVO_PIN_FL, SERVO_PIN_FR, SERVO_PIN_RL, SERVO_PIN_RR};
const char* LEG_CODES[NUM_SERVOS] = {"FL", "FR", "RL", "RR"};
int currentAngles[NUM_SERVOS] = {90, 90, 90, 90};

static const int MIN_ANGLE = 45;
static const int MAX_ANGLE = 135;
static const int FR_STOP_360 = 90;   // Điểm dừng servo 360 (chỉnh nếu servo còn nhích)

static volatile bool sweepRequested = false;
static bool i2sReady = false;
static String lastHudPose = "";

// ----------------------------------------------------------------------------
// SERVO
// ----------------------------------------------------------------------------
void writeServoDuty(int channel, int angle) {
  if (SERVO_FR_IS_360 && channel == LEG_FR) {
    angle = FR_STOP_360;  // Servo 360: góc = tốc độ -> luôn giữ điểm dừng
  } else {
    angle = constrain(angle, MIN_ANGLE, MAX_ANGLE);
  }
  // 50Hz, 14-bit (0..16383): 0.5ms = 410, 2.5ms = 2048
  ledcWrite(channel, map(angle, 0, 180, 410, 2048));
}

void setupServos() {
  Serial.println("[SERVO] Khoi tao 4 kenh PWM cho Servos...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    ledcSetup(i, 50, 14);
    ledcAttachPin(SERVO_PINS[i], i);
    writeServoDuty(i, 90);
    currentAngles[i] = 90;
    delay(100);  // Khởi động so le tránh sụt áp
  }
  Serial.println("[SERVO] Hoan tat khoi tao Servo o goc 90 do.");
}

// ----------------------------------------------------------------------------
// I2S FULL-DUPLEX (MIC + DAC/AMP), 32-bit slot
// ----------------------------------------------------------------------------
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
      .bck_io_num = I2S_PIN_BCLK,
      .ws_io_num = I2S_PIN_WS,
      .data_out_num = I2S_PIN_DOUT,
      .data_in_num = I2S_PIN_DIN
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err == ESP_OK) err = i2s_set_pin(I2S_PORT, &pin_config);
  i2sReady = (err == ESP_OK);
  if (i2sReady) {
    Serial.println("[AUDIO] I2S Full-Duplex 32-bit (PCM5100A + INMP441) da san sang.");
  } else {
    Serial.printf("[AUDIO] LOI khoi tao I2S: %s\n", esp_err_to_name(err));
  }
}

// Phát âm sine (Bip / Chime)
void playBeep(float freqHz, int durationMs, float volume = 0.4f) {
  if (!i2sReady) return;
  const int CHUNK = 256;
  static int32_t buffer[CHUNK * 2];  // Stereo L+R, 32-bit
  int samplesLeft = (AUDIO_RATE * durationMs) / 1000;
  float phase = 0.0f;
  const float phaseInc = (2.0f * M_PI * freqHz) / AUDIO_RATE;

  while (samplesLeft > 0) {
    int chunk = (samplesLeft > CHUNK) ? CHUNK : samplesLeft;
    for (int i = 0; i < chunk; i++) {
      int32_t sample = (int32_t)(sinf(phase) * (32767.0f * volume)) << 16;
      buffer[i * 2]     = sample;
      buffer[i * 2 + 1] = sample;
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, buffer, chunk * 2 * sizeof(int32_t), &bytesWritten, portMAX_DELAY);
    samplesLeft -= chunk;
  }
}

void playStartupMelody() {
  playBeep(523.25f, 90, 0.3f);   // C5
  delay(30);
  playBeep(659.25f, 90, 0.3f);   // E5
  delay(30);
  playBeep(783.99f, 90, 0.3f);   // G5
  delay(30);
  playBeep(1046.50f, 180, 0.35f); // C6
}

// Mức âm thanh Mic INMP441 (RMS, thang 16-bit). Tính cả 2 kênh, lấy kênh mạnh hơn
// để không phụ thuộc thứ tự L/R của driver.
float readMicLevel() {
  if (!i2sReady) return 0.0f;
  static int32_t micBuffer[128 * 2];
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, micBuffer, sizeof(micBuffer), &bytesRead, pdMS_TO_TICKS(10));
  int frames = bytesRead / (2 * sizeof(int32_t));
  if (frames == 0) return 0.0f;

  int64_t sumA = 0, sumB = 0;
  for (int i = 0; i < frames; i++) {
    int32_t a = micBuffer[i * 2] >> 16;      // 24-bit left-justified -> 16-bit
    int32_t b = micBuffer[i * 2 + 1] >> 16;
    sumA += (int64_t)a * a;
    sumB += (int64_t)b * b;
  }
  return sqrtf((float)max(sumA, sumB) / frames);
}

// ----------------------------------------------------------------------------
// NÚT BOOT: chờ có kiểm tra nút, để nhấn lúc nào cũng có phản hồi
// ----------------------------------------------------------------------------
void pollBootButton() {
  if (digitalRead(PIN_BOOT_BTN) == LOW) sweepRequested = true;
}

// ----------------------------------------------------------------------------
// HUD & ROBO-EYES (320x240 sau setRotation(1))
// ----------------------------------------------------------------------------
#define EYES_AREA_X 40
#define EYES_AREA_Y 50
#define EYES_AREA_W 240
#define EYES_AREA_H 95

void drawEye(int cx, int cy, int w, int h, uint16_t color, int pupOffsetX = 0, int pupOffsetY = 0) {
  tft.fillRoundRect(cx - w/2, cy - h/2, w, h, 14, color);
  int pw = w / 2;
  int ph = h / 2;
  tft.fillRoundRect(cx + pupOffsetX - pw/2, cy + pupOffsetY - ph/2, pw, ph, 8, ILI9341_BLACK);
  tft.fillCircle(cx + pupOffsetX - pw/4, cy + pupOffsetY - ph/4, 4, ILI9341_WHITE);
}

void drawEyes(uint16_t eyeColor, int eyeH, int pupX, int pupY) {
  tft.fillRect(EYES_AREA_X, EYES_AREA_Y, EYES_AREA_W, EYES_AREA_H, ILI9341_BLACK);
  drawEye(105, 95, 65, eyeH, eyeColor, pupX, pupY);
  drawEye(215, 95, 65, eyeH, eyeColor, pupX, pupY);
}

void drawAngles() {
  const int xs[NUM_SERVOS] = {12, 90, 168, 246};
  tft.fillRect(0, 184, 320, 14, ILI9341_BLACK);
  tft.setTextSize(1);
  for (int i = 0; i < NUM_SERVOS; i++) {
    tft.setCursor(xs[i], 188);
    if (SERVO_FR_IS_360 && i == LEG_FR) {
      tft.setTextColor(ILI9341_ORANGE);
      tft.printf("FR(G%d):360", SERVO_PINS[i]);
    } else {
      tft.setTextColor(ILI9341_GREEN);
      tft.printf("%s(G%d):%d", LEG_CODES[i], SERVO_PINS[i], currentAngles[i]);
    }
  }
}

void drawHUD(const char* poseName, uint16_t eyeColor, int eyeH = 60, int pupX = 0, int pupY = 0) {
  tft.fillScreen(ILI9341_BLACK);

  tft.fillRect(0, 0, 320, 24, 0x18C3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 8);
  tft.print("XIAOZHI ROBOT DOG - HARDWARE SUITE");

  drawEyes(eyeColor, eyeH, pupX, pupY);

  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  int nameX = 160 - (int)(strlen(poseName) * 6);
  if (nameX < 4) nameX = 4;
  tft.setCursor(nameX, 152);
  tft.print(poseName);

  tft.drawFastHLine(10, 178, 300, 0x4208);
  drawAngles();

  // Thanh đáy: chữ bên trái, VU meter bên phải (không chồng nhau)
  tft.fillRect(0, 218, 320, 22, 0x10A2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(1);
  tft.setCursor(8, 225);
  tft.print("BOOT: quet 45/135 | MIC:");
  tft.drawRect(163, 223, 150, 12, 0x4208);

  lastHudPose = poseName;
}

// Thanh VU meter: x=164..311, y=224..233
void updateMicVUMeter(float level) {
  const int maxW = 148;
  int barWidth = map((int)constrain(level, 0, 8000), 0, 8000, 0, maxW);
  uint16_t col = (barWidth > maxW * 2 / 3) ? ILI9341_RED
               : (barWidth > maxW / 3 ? ILI9341_YELLOW : ILI9341_GREEN);
  tft.fillRect(164, 224, barWidth, 10, col);
  tft.fillRect(164 + barWidth, 224, maxW - barWidth, 10, ILI9341_BLACK);
}

// Chờ ms mili-giây, vẫn cập nhật VU meter và theo dõi nút BOOT.
// Trả về true nếu có yêu cầu sweep (để loop() thoát sớm).
bool waitWithMic(int ms) {
  unsigned long start = millis();
  while ((int)(millis() - start) < ms) {
    updateMicVUMeter(readMicLevel());  // readMicLevel tự chờ tối đa 10ms
    pollBootButton();
    if (sweepRequested) return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
// CHUYỂN ĐỘNG MƯỢT (COSINE S-CURVE)
// ----------------------------------------------------------------------------
void setPoseSmooth(const char* poseName, int targetFL, int targetFR, int targetRL, int targetRR,
                   int durationMs = 500, uint16_t eyeCol = ILI9341_CYAN, int eyeH = 60,
                   int pupX = 0, int pupY = 0) {
  // Cùng tên tư thế (ví dụ WAVE lặp) -> chỉ vẽ lại mắt, tránh nháy cả màn hình
  if (lastHudPose == poseName) drawEyes(eyeCol, eyeH, pupX, pupY);
  else drawHUD(poseName, eyeCol, eyeH, pupX, pupY);

  int startAngles[NUM_SERVOS];
  for (int i = 0; i < NUM_SERVOS; i++) startAngles[i] = currentAngles[i];
  int targets[NUM_SERVOS] = {
      constrain(targetFL, MIN_ANGLE, MAX_ANGLE),
      SERVO_FR_IS_360 ? FR_STOP_360 : constrain(targetFR, MIN_ANGLE, MAX_ANGLE),
      constrain(targetRL, MIN_ANGLE, MAX_ANGLE),
      constrain(targetRR, MIN_ANGLE, MAX_ANGLE)};

  int steps = max(1, durationMs / 20);
  for (int s = 1; s <= steps; s++) {
    unsigned long t0 = millis();
    float t = (float)s / steps;
    float factor = (1.0f - cosf(t * M_PI)) / 2.0f;
    for (int i = 0; i < NUM_SERVOS; i++) {
      int deg = (int)(startAngles[i] + (targets[i] - startAngles[i]) * factor);
      writeServoDuty(i, deg);
      currentAngles[i] = deg;
    }
    updateMicVUMeter(readMicLevel());
    pollBootButton();
    int spent = millis() - t0;
    if (spent < 20) delay(20 - spent);  // giữ nhịp 20ms/bước bất kể thời gian vẽ
  }

  for (int i = 0; i < NUM_SERVOS; i++) {
    writeServoDuty(i, targets[i]);
    currentAngles[i] = targets[i];
  }
  drawAngles();
}

void runSweep() {
  sweepRequested = false;
  playBeep(1000.0f, 100);
  setPoseSmooth("DONG LOAT: 45", 45, 45, 45, 45, 600, ILI9341_YELLOW, 60);
  delay(1500);
  setPoseSmooth("DONG LOAT: 135", 135, 135, 135, 135, 600, ILI9341_MAGENTA, 60);
  delay(1500);
  setPoseSmooth("DONG LOAT: 90", 90, 90, 90, 90, 500, ILI9341_CYAN, 60);
  delay(1000);
  while (digitalRead(PIN_BOOT_BTN) == LOW) delay(10);  // chờ nhả nút
  sweepRequested = false;
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

  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(1);
  tft.setTextWrap(false);

  setupI2SAudio();
  setupServos();

  drawHUD("KHOI DONG...", ILI9341_CYAN, 60);
  playStartupMelody();
  delay(1000);
}

void loop() {
  pollBootButton();
  if (sweepRequested) { runSweep(); return; }

  // 1. STAND
  playBeep(523.25f, 60, 0.25f);
  setPoseSmooth("STAND (Dung)", 90, 90, 90, 90, 600, ILI9341_CYAN, 60);
  if (waitWithMic(2000)) return;

  // 2. SIT
  playBeep(659.25f, 60, 0.25f);
  setPoseSmooth("SIT (Ngoi)", 100, 90, 120, 120, 700, ILI9341_GREEN, 50);
  if (waitWithMic(2000)) return;

  // 3. WAVE: vẫy chân trước trái FL (FR là servo 360 nên không dùng để vẫy)
  playBeep(783.99f, 60, 0.25f);
  for (int w = 0; w < 3; w++) {
    setPoseSmooth("WAVE (Vay chao)", 125, 90, 120, 120, 200, ILI9341_YELLOW, 70, 15, -10);
    if (waitWithMic(80)) return;
    setPoseSmooth("WAVE (Vay chao)", 80, 90, 120, 120, 200, ILI9341_YELLOW, 70, -15, -10);
    if (waitWithMic(80)) return;
  }
  if (waitWithMic(500)) return;

  // 4. CROUCH
  playBeep(440.0f, 80, 0.25f);
  setPoseSmooth("CROUCH (Cui)", 65, 90, 65, 65, 600, ILI9341_ORANGE, 40);
  if (waitWithMic(2000)) return;

  // Trở lại STAND
  setPoseSmooth("STAND (Dung)", 90, 90, 90, 90, 500, ILI9341_CYAN, 60);
  waitWithMic(1500);
}
