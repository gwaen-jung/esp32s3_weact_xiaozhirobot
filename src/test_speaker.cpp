/**
 * ============================================================================
 * XIAOZHI CUSTOM PCB - CHƯƠNG TRÌNH TEST RIÊNG BIỆT CHO LOA MAX98357A
 * ============================================================================
 * Phần cứng:
 *  - Bo mạch: ESP32-S3 Clone
 *  - Module Loa MAX98357A (I2S DAC / Class-D Amp):
 *      + BCLK -> GPIO 11
 *      + LRC  -> GPIO 12
 *      + DIN  -> GPIO 13
 *      + GND  -> GND
 *      + VIN  -> 5V (hoặc 3.3V)
 *      + SD   -> ĐIỀU KIỆN TIÊN QUYẾT: NỐI TẮT JP1 LÊN VCC/3.3V!
 *  - Màn hình OLED SSD1306:
 *      + SDA  -> GPIO 1
 *      + SCL  -> GPIO 2
 *  - TẤT CẢ MOTOR (GPIO 4, 5, 6, 7) ĐỀU TẮT HOÀN TOÀN để tránh ồn & nhiễu.
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <math.h>

// --- PIN CẤU HÌNH ---
#ifndef PIN_OLED_SDA
#define PIN_OLED_SDA  2
#endif
#ifndef PIN_OLED_SCL
#define PIN_OLED_SCL  1
#endif

#ifndef PIN_I2S_BCLK
#define PIN_I2S_BCLK  11
#endif
#ifndef PIN_I2S_WS
#define PIN_I2S_WS    12
#endif
#ifndef PIN_I2S_DOUT
#define PIN_I2S_DOUT  13
#endif

#define PIN_MOTOR_1   4
#define PIN_MOTOR_2   5
#define PIN_MOTOR_3   6
#define PIN_MOTOR_4   7
#define PIN_LED       48
#define PIN_BOOT      0

// --- I2S PARAMETERS ---
#define I2S_PORT      I2S_NUM_0
#define SAMPLE_RATE   44100 // 44.1 kHz - Chuẩn CD Audio cho MAX98357A
#define CHUNK_SIZE    256

// --- OLED SETUP ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static bool oledOk = false;

static bool i2sOk = false;
static float currentTestFreq = 0.0f;
static const char* currentTestName = "KHOI DONG";
static int waveAnimationTick = 0;

// ============================================================================
// 1. KHỞI TẠO OLED SSD1306 (Tự động dò SDA=2/SCL=1 hoặc SDA=1/SCL=2)
// ============================================================================
void initOLED() {
  int pinPairs[][2] = {{2, 1}, {1, 2}};
  for (int i = 0; i < 2; i++) {
    int sda = pinPairs[i][0];
    int scl = pinPairs[i][1];
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    Wire.end();
    delay(15);
    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(30);

    for (uint8_t addr : {0x3C, 0x3D}) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        if (display.begin(SSD1306_SWITCHCAPVCC, addr, false, false)) {
          oledOk = true;
          display.ssd1306_command(SSD1306_DISPLAYON);
          display.ssd1306_command(SSD1306_SETCONTRAST);
          display.ssd1306_command(0xFF);
          display.clearDisplay();
          display.display();
          Serial.printf("[OLED] Khoi dong thanh cong tren SDA=%d, SCL=%d, Addr=0x%02X!\n", sda, scl, addr);
          return;
        }
      }
    }
  }
  oledOk = false;
  Serial.println("[OLED] Khong tim thay OLED tren I2C.");
}

void updateOLEDUI(const char* title, const char* desc, float freq, int percent) {
  if (!oledOk) return;

  display.clearDisplay();

  // Header đảo màu
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(8, 2);
  display.print("TEST LOA MAX98357");

  display.setTextColor(SSD1306_WHITE);

  // Tên bài test đang chạy
  display.setCursor(2, 16);
  display.print("Bai test:");
  display.setCursor(2, 27);
  display.setTextSize(1);
  display.printf(">> %s", title);

  // Tần số & Âm lượng
  display.setCursor(2, 39);
  if (freq > 0) {
    display.printf("Tan so: %.0f Hz", freq);
  } else {
    display.print("Nghi giua cac not");
  }

  // Khung mô phỏng sóng âm thanh động
  waveAnimationTick++;
  for (int x = 0; x < 128; x += 3) {
    int h = 0;
    if (freq > 0) {
      float a = sinf((x + waveAnimationTick * 4) * 0.2f);
      h = (int)(abs(a) * 9.0f) + 1;
    }
    display.drawFastVLine(x, 58 - h, h, SSD1306_WHITE);
  }

  // Nhắc nhở JP1 ở dưới đáy
  display.drawFastHLine(0, 60, 128, SSD1306_WHITE);

  display.display();
}

// ============================================================================
// 2. KHỞI TẠO I2S AUDIO CHO MAX98357A (16-BIT STEREO, 44.1 KHZ)
// ============================================================================
void initI2S() {
  Serial.println("\n[I2S] Dang khoi tao I2S Master TX (16-bit, 44.1kHz)...");
  Serial.printf("  - BCLK: GPIO %d\n", PIN_I2S_BCLK);
  Serial.printf("  - LRC : GPIO %d\n", PIN_I2S_WS);
  Serial.printf("  - DIN : GPIO %d\n", PIN_I2S_DOUT);

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = CHUNK_SIZE;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true; // Chống tiếng "bụp" và rít khi dừng
  cfg.fixed_mclk = 0;

  i2s_driver_uninstall(I2S_PORT);
  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S ERROR] i2s_driver_install that bai: %d\n", err);
    i2sOk = false;
    return;
  }

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK) {
    Serial.printf("[I2S ERROR] i2s_set_pin that bai: %d\n", err);
    i2sOk = false;
    return;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  i2sOk = true;
  Serial.println("[I2S OK] Driver I2S da san sang truyen du lieu ra MAX98357A!");
}

// ============================================================================
// 3. CÁC HÀM PHÁT ÂM THANH MẠNH MẼ (SÓNG VUÔNG & SÓNG SIN 100% VOLUME)
// ============================================================================

// Phát sóng vuông (Square wave) - Tiếng còi điện tử vang, đanh, dễ nghe nhất
void playSquareWave(float freqHz, int durationMs, float volume = 1.0f) {
  if (!i2sOk) return;

  currentTestFreq = freqHz;
  static int16_t txBuf[CHUNK_SIZE * 2]; // 16-bit Stereo L/R
  int totalSamples = (SAMPLE_RATE * durationMs) / 1000;
  float phase = 0.0f;
  const float phaseInc = (2.0f * M_PI * freqHz) / SAMPLE_RATE;
  int16_t amplitude = (int16_t)(32000.0f * volume);

  while (totalSamples > 0) {
    int chunk = (totalSamples > CHUNK_SIZE) ? CHUNK_SIZE : totalSamples;
    for (int i = 0; i < chunk; i++) {
      int16_t sample = (phase < M_PI) ? amplitude : -amplitude;
      txBuf[i * 2]     = sample; // Kênh Trái (L)
      txBuf[i * 2 + 1] = sample; // Kênh Phải (R)
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t written = 0;
    i2s_write(I2S_PORT, txBuf, chunk * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(100));
    totalSamples -= chunk;
  }
}

// Phát sóng sin (Sine wave) - Âm thanh mượt mà, ấm áp
void playSineWave(float freqHz, int durationMs, float volume = 1.0f) {
  if (!i2sOk) return;

  currentTestFreq = freqHz;
  static int16_t txBuf[CHUNK_SIZE * 2];
  int totalSamples = (SAMPLE_RATE * durationMs) / 1000;
  float phase = 0.0f;
  const float phaseInc = (2.0f * M_PI * freqHz) / SAMPLE_RATE;
  float amplitude = 32000.0f * volume;

  while (totalSamples > 0) {
    int chunk = (totalSamples > CHUNK_SIZE) ? CHUNK_SIZE : totalSamples;
    for (int i = 0; i < chunk; i++) {
      int16_t sample = (int16_t)(sinf(phase) * amplitude);
      txBuf[i * 2]     = sample;
      txBuf[i * 2 + 1] = sample;
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t written = 0;
    i2s_write(I2S_PORT, txBuf, chunk * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(100));
    totalSamples -= chunk;
  }
}

void silence(int durationMs) {
  currentTestFreq = 0.0f;
  static int16_t zeroBuf[CHUNK_SIZE * 2] = {0};
  int totalSamples = (SAMPLE_RATE * durationMs) / 1000;

  while (totalSamples > 0) {
    int chunk = (totalSamples > CHUNK_SIZE) ? CHUNK_SIZE : totalSamples;
    size_t written = 0;
    i2s_write(I2S_PORT, zeroBuf, chunk * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(100));
    totalSamples -= chunk;
  }
}

// ============================================================================
// 4. CÁC BÀI TEST ÂM THANH TUẦN HOÀN
// ============================================================================

// Bài 1: Tiếng còi bíp 1000Hz (Cực kỳ đanh và rõ)
void testBeeper1kHz() {
  Serial.println("\n[TEST 1] Phat 3 tieng coi BEEP 1000Hz (Square Wave 100%)...");
  currentTestName = "1. COI BEEP 1KHZ";

  for (int i = 0; i < 3; i++) {
    updateOLEDUI("1. COI BEEP 1KHZ", "Song vuong 1kHz", 1000, 100);
    neopixelWrite(PIN_LED, 40, 40, 0); // Vàng
    playSquareWave(1000.0f, 250, 1.0f);
    neopixelWrite(PIN_LED, 0, 0, 0);
    silence(150);
  }
}

// Bài 2: Âm giai Đồ - Rê - Mi - Pha - Son - La - Si - Đố
void testMusicalScale() {
  Serial.println("\n[TEST 2] Phat gam am nhac Do-Re-Mi-Fa-Sol-La-Si-Do...");
  currentTestName = "2. DO RE MI FA SOL";

  float scale[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
  const char* names[] = {"DO (C4)", "RE (D4)", "MI (E4)", "FA (F4)", "SOL (G4)", "LA (A4)", "SI (B4)", "DO (C5)"};

  for (int i = 0; i < 8; i++) {
    updateOLEDUI(names[i], "Sóng sin chuẩn", scale[i], 100);
    neopixelWrite(PIN_LED, 0, 40, 20); // Cyan
    playSineWave(scale[i], 220, 1.0f);
    silence(40);
  }
}

// Bài 3: Còi báo động xe cảnh sát (Siren Hi-Lo)
void testPoliceSiren() {
  Serial.println("\n[TEST 3] Phat coi bao dong Siren 700Hz <-> 1400Hz...");
  currentTestName = "3. COI BAO DONG";

  for (int i = 0; i < 4; i++) {
    updateOLEDUI("COI BAO DONG", "Cao do 1400Hz", 1400, 100);
    neopixelWrite(PIN_LED, 40, 0, 0); // Đỏ
    playSquareWave(1400.0f, 200, 1.0f);

    updateOLEDUI("COI BAO DONG", "Cao do 700Hz", 700, 100);
    neopixelWrite(PIN_LED, 0, 0, 40); // Xanh dương
    playSquareWave(700.0f, 200, 1.0f);
  }
  neopixelWrite(PIN_LED, 0, 0, 0);
}

// Bài 4: Đoạn nhạc Super Mario kinh điển
void testSuperMario() {
  Serial.println("\n[TEST 4] Phat nhac Super Mario Theme kinh dien...");
  currentTestName = "4. SUPER MARIO";

  struct Note { float f; int d; };
  Note mario[] = {
    {659.25f, 130}, {0, 30},
    {659.25f, 130}, {0, 90},
    {659.25f, 130}, {0, 90},
    {523.25f, 130}, {0, 30},
    {659.25f, 150}, {0, 90},
    {783.99f, 220}, {0, 180},
    {392.00f, 250}, {0, 120}
  };

  for (auto& n : mario) {
    if (n.f > 0) {
      updateOLEDUI("SUPER MARIO", "Mario Theme", n.f, 100);
      neopixelWrite(PIN_LED, 0, 40, 0); // Xanh lá
      playSquareWave(n.f, n.d, 0.95f);
    } else {
      silence(n.d);
    }
  }
  neopixelWrite(PIN_LED, 0, 0, 0);
}

// Bài 5: Quét dải tần số liên tục từ Trầm đến Bổng (200Hz -> 2500Hz)
void testFrequencySweep() {
  Serial.println("\n[TEST 5] Dang quet tan so Sweep (200Hz -> 2500Hz)...");
  currentTestName = "5. SWEEP 200-2500";
  updateOLEDUI("QUET TAN SO", "200Hz -> 2500Hz", 1000, 100);

  for (float f = 200.0f; f <= 2500.0f; f += 25.0f) {
    playSineWave(f, 16, 1.0f);
  }
  silence(200);
}

// ============================================================================
// 5. SETUP & MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Đảm bảo tất cả chân motor bị ngắt hoàn toàn
  pinMode(PIN_MOTOR_1, OUTPUT); digitalWrite(PIN_MOTOR_1, LOW);
  pinMode(PIN_MOTOR_2, OUTPUT); digitalWrite(PIN_MOTOR_2, LOW);
  pinMode(PIN_MOTOR_3, OUTPUT); digitalWrite(PIN_MOTOR_3, LOW);
  pinMode(PIN_MOTOR_4, OUTPUT); digitalWrite(PIN_MOTOR_4, LOW);

  pinMode(PIN_LED, OUTPUT);
  neopixelWrite(PIN_LED, 0, 0, 40); // Xanh dương báo boot

  delay(500);
  Serial.println("\n\n==========================================================");
  Serial.println("  CHUONG TRINH TEST CHUYEN BIET CHO LOA MAX98357A (ESP32-S3)");
  Serial.println("  BCLK=GPIO11, LRC=GPIO12, DIN=GPIO13                      ");
  Serial.println("  * LUU Y PHAN CUNG: Vui long NOI TAT JP1 de mo loa (Unmute)");
  Serial.println("==========================================================");

  // 1. Khởi động OLED (SDA=1, SCL=2)
  initOLED();
  updateOLEDUI("KHOI DONG", "Dang khoi tao I2S...", 0, 0);

  // 2. Khởi tạo driver I2S chuẩn cho MAX98357A
  initI2S();

  delay(600);
  neopixelWrite(PIN_LED, 0, 40, 0); // Xanh lá
  Serial.println("[BAT DAU] Chuong trinh test loa bat dau phat lien tuc!");
}

void loop() {
  // Lần lượt chạy 5 bài test âm thanh đa dạng:
  testBeeper1kHz();      // 1. Tiếng còi bíp đanh rõ
  silence(500);

  testMusicalScale();    // 2. Âm giai Đồ Rê Mi
  silence(500);

  testPoliceSiren();     // 3. Còi cảnh sát
  silence(500);

  testSuperMario();      // 4. Nhạc Mario
  silence(500);

  testFrequencySweep();  // 5. Quét tần số bass - treble
  silence(1500);

  // Nhắc nhở qua Serial sau mỗi vòng lặp
  Serial.println("\n[NHAC NHO] Neu ban van chua nghe thay tieng loa:");
  Serial.println("  1. Kiem tra xem 2 chan cua JP1 canh MAX98357 da duoc NOI TAT chua.");
  Serial.println("  2. Kiem tra nguon cap VIN (5V/3.3V) va GND vao module MAX98357.");
  Serial.println("  3. Kiem tra oc siet day loa vao coc (+) va (-).");
}
