/**
 * ============================================================================
 * TEST MICROPHONE INMP441 TRÊN WEACT ESP32-S3 (GPIO 4, 5, 6)
 * ============================================================================
 * Sơ đồ đấu dây (theo sơ đồ cậu gửi):
 *  - VCC -> 3V3
 *  - GND -> GND (Chân L/R nối GND = Kênh LEFT, hoặc nối 3V3 = Kênh RIGHT)
 *  - WS  -> GPIO 4
 *  - SCK -> GPIO 5 (BCLK)
 *  - SD  -> GPIO 6 (DOUT của Mic -> DIN của ESP32)
 *
 * Tính năng nổi bật:
 *  - Tự động đọc Stereo (Left + Right) và phát hiện kênh hoạt động (`LEFT(GND)` / `RIGHT(3V3)`).
 *  - Bộ lọc IIR High-Pass 20Hz chặn DC Offset chuẩn ReShape Lab.
 *  - Tự động hiệu chuẩn nền nhiễu phòng (Dynamic Noise Floor Calibration) giúp
 *    phân biệt cực nhạy giữa Im lặng (SILENT), Giọng nói (VOICE), và Tiếng vỗ tay/gõ (LOUD!).
 *  - Hiển thị thanh VU Meter trực quan trên Serial (COM21 - 115200 baud).
 *  - Nháy LED RGB WS2812 trên GPIO 48 theo cường độ âm thanh thực tế:
 *      + Im lặng (Nền phòng): Xanh dương dịu
 *      + Có tiếng nói (VOICE): Xanh lá cây sáng theo giọng
 *      + Tiếng to / Vỗ tay / Gõ mic (LOUD!): Đỏ cam rực rỡ
 * ============================================================================
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

#ifndef PIN_INMP441_WS
#define PIN_INMP441_WS   4
#endif

#ifndef PIN_INMP441_SCK
#define PIN_INMP441_SCK  5
#endif

#ifndef PIN_INMP441_SD
#define PIN_INMP441_SD   6
#endif

#define PIN_RGB_LED      48
#define I2S_PORT         I2S_NUM_0
#define SAMPLE_RATE      16000
#define DMA_BUF_LEN      256
#define DMA_BUF_COUNT    4

static int32_t rawBuffer[DMA_BUF_LEN * 2]; // Stereo: Left + Right
static float dcX_L = 0.0f, dcY_L = 0.0f;
static float dcX_R = 0.0f, dcY_R = 0.0f;
static float smoothRms = 0.0f;
static float noiseFloor = 300.0f;
static bool calibrated = false;
static uint32_t startTime = 0;

void initI2SMic() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_INMP441_SCK,
    .ws_io_num = PIN_INMP441_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_INMP441_SD
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] i2s_driver_install failed: %d\n", err);
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] i2s_set_pin failed: %d\n", err);
    return;
  }

  i2s_zero_dma_buffer(I2S_PORT);

  // Xả bỏ 10 frame đầu tiên để tụ IIR DC Blocker hội tụ ổn định (tránh gai xung lúc mới bật mic)
  size_t dummyBytes = 0;
  for (int k = 0; k < 10; k++) {
    i2s_read(I2S_PORT, rawBuffer, sizeof(rawBuffer), &dummyBytes, pdMS_TO_TICKS(50));
    int frames = dummyBytes / (sizeof(int32_t) * 2);
    for (int i = 0; i < frames; i++) {
      // Dịch >> 16 chuẩn 16-bit PCM để dải động rộng, không bị bão hòa nền
      int32_t sL = rawBuffer[i * 2] >> 16;
      int32_t sR = rawBuffer[i * 2 + 1] >> 16;
      dcY_L = (float)sL - dcX_L + 0.995f * dcY_L; dcX_L = (float)sL;
      dcY_R = (float)sR - dcX_R + 0.995f * dcY_R; dcX_R = (float)sR;
    }
  }

  Serial.println("[OK] I2S INMP441 Initialized & DC Blocker Settled!");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Đưa các chân không dùng về INPUT an toàn
  const int safe_unused[] = {
    1, 2, 3, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 47
  };
  for (int p : safe_unused) {
    pinMode(p, INPUT);
  }

  pinMode(PIN_INMP441_SD, INPUT_PULLDOWN);
  neopixelWrite(PIN_RGB_LED, 0, 0, 20);

  Serial.println("\n================================================================");
  Serial.println("   TRIAD / RESHAPE LAB - TEST MICROPHONE INMP441 (ESP32-S3)");
  Serial.println("================================================================");
  Serial.printf("  Pinout: WS = GPIO %d | SCK (BCLK) = GPIO %d | SD = GPIO %d\n",
                PIN_INMP441_WS, PIN_INMP441_SCK, PIN_INMP441_SD);
  Serial.println("  LED GPIO 48: Xanh Duong = Im lang | Xanh La = Giong noi | Do = To!");
  Serial.println("================================================================\n");

  initI2SMic();
  startTime = millis();
}

void loop() {
  size_t bytesRead = 0;
  esp_err_t res = i2s_read(I2S_PORT, rawBuffer, sizeof(rawBuffer), &bytesRead, pdMS_TO_TICKS(100));

  if (res != ESP_OK || bytesRead == 0) {
    Serial.println("[WARN] Khong doc duoc DMA I2S (bytesRead = 0)!");
    delay(200);
    return;
  }

  int frames = bytesRead / (sizeof(int32_t) * 2);
  double sumSqL = 0.0, sumSqR = 0.0, sumSqBest = 0.0;
  int32_t peakBest = 0;
  int nonZeroCount = 0;

  for (int i = 0; i < frames; i++) {
    int32_t rawL = rawBuffer[i * 2];
    int32_t rawR = rawBuffer[i * 2 + 1];

    if (rawL != 0 || rawR != 0) {
      nonZeroCount++;
    }

    // Dịch >> 16 chuẩn 16-bit PCM từ 32-bit I2S frame
    int32_t sL = rawL >> 16;
    int32_t sR = rawR >> 16;

    // Bộ lọc IIR High-Pass 20Hz chặn DC Offset
    float filtL = (float)sL - dcX_L + 0.995f * dcY_L;
    dcX_L = (float)sL;
    dcY_L = filtL;

    float filtR = (float)sR - dcX_R + 0.995f * dcY_R;
    dcX_R = (float)sR;
    dcY_R = filtR;

    sumSqL += (double)filtL * filtL;
    sumSqR += (double)filtR * filtR;

    float bestSample = (fabsf(filtL) > fabsf(filtR)) ? filtL : filtR;
    int32_t cleanSample = constrain((int32_t)bestSample, -32767, 32767);

    sumSqBest += (double)cleanSample * cleanSample;
    int32_t absVal = abs(cleanSample);
    if (absVal > peakBest) {
      peakBest = absVal;
    }
  }

  float rmsL = sqrtf((float)(sumSqL / frames));
  float rmsR = sqrtf((float)(sumSqR / frames));
  float rmsBest = sqrtf((float)(sumSqBest / frames));

  if (!calibrated) {
    smoothRms = rmsBest;
    noiseFloor = rmsBest;
    calibrated = true;
  } else {
    smoothRms = 0.60f * smoothRms + 0.40f * rmsBest;
    // Bám nền nhiễu phòng (Adaptive Noise Floor): hạ nhanh khi yên tĩnh, tăng cực chậm khi ồn
    if (rmsBest < noiseFloor) {
      noiseFloor = 0.90f * noiseFloor + 0.10f * rmsBest;
    } else {
      noiseFloor = 0.995f * noiseFloor + 0.005f * rmsBest;
    }
    noiseFloor = constrain(noiseFloor, 40.0f, 1500.0f);
  }

  // Biên độ tín hiệu thực sự vượt trên nền nhiễu phòng (Net Signal)
  float netRms = fmaxf(0.0f, smoothRms - noiseFloor);
  float snrRatio = smoothRms / fmaxf(noiseFloor, 1.0f);
  float dbfs = (rmsBest > 1.0f) ? (20.0f * log10f(rmsBest / 32767.0f)) : -96.0f;

  // Xác định kênh đang hoạt động
  const char* activeCh = "NONE (0)";
  if (nonZeroCount > 0) {
    if (rmsL > rmsR * 2.0f) activeCh = "LEFT(GND)";
    else if (rmsR > rmsL * 2.0f) activeCh = "RIGHT(3V3)";
    else activeCh = "STEREO";
  }

  // Ngưỡng phát hiện tiếng nói thông minh theo nền phòng
  bool isLoud  = (netRms > 1200.0f) || (peakBest > 6000);
  bool isVoice = !isLoud && ((netRms > 180.0f && snrRatio > 1.35f) || (peakBest > (int32_t)(noiseFloor * 3.2f)));

  // Điều khiển LED RGB GPIO 48
  if (nonZeroCount == 0) {
    neopixelWrite(PIN_RGB_LED, 30, 0, 30); // Tím: Mất dữ liệu SD
  } else if (isLoud) {
    neopixelWrite(PIN_RGB_LED, 200, 15, 0); // Đỏ cam rực: Tiếng to / Vỗ tay
  } else if (isVoice) {
    int g = constrain((int)(netRms / 8.0f), 25, 200);
    neopixelWrite(PIN_RGB_LED, 0, g, 0); // Xanh lá: Đang nói chuyện
  } else {
    neopixelWrite(PIN_RGB_LED, 0, 0, 15); // Xanh dương dịu: Im lặng
  }

  // In ra Serial mỗi 80ms
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 80) {
    lastPrint = millis();

    // Vẽ thanh VU Meter 30 cột dựa trên Net Signal vượt nền nhiễu + mức tuyệt đối
    int barLen = 0;
    if (nonZeroCount > 0) {
      float level = netRms + (smoothRms * 0.15f);
      if (level > 15.0f) {
        float norm = log10f(level / 15.0f) / log10f(8000.0f / 15.0f);
        barLen = constrain((int)(norm * 30.0f), 1, 30);
      }
    }

    char vuBar[32];
    for (int b = 0; b < 30; b++) {
      if (b < barLen) {
        vuBar[b] = (b >= 22) ? '#' : (b >= 10 ? '=' : '-');
      } else {
        vuBar[b] = ' ';
      }
    }
    vuBar[30] = '\0';

    const char* statusTag = "SILENT ";
    if (nonZeroCount == 0) statusTag = "NO_DATA";
    else if (isLoud)       statusTag = "LOUD!  ";
    else if (isVoice)      statusTag = "VOICE  ";

    Serial.printf("[%s] %s | RMS:%5.0f (Floor:%4.0f) | Peak:%5d | %5.1fdB | [%s]\n",
                  statusTag, activeCh, smoothRms, noiseFloor, peakBest, dbfs, vuBar);
  }
}
