/**
 * ============================================================================
 * XIAOZHI ROBOT DOG - BÀI TEST CHUYÊN BIỆT CẢM BIẾN I2C (LASER TOF & MPU6050)
 * Môi trường: [env:test_sensors] (PlatformIO)
 * MCU: WeAct Studio ESP32-S3 CoreBoard N16R8
 * Display: 2.4" SPI TFT LCD (ILI9341, 240x320)
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI I2C:
 *   - SDA: GPIO 8
 *   - SCL: GPIO 9
 *   - VCC: 3.3V (BẮT BUỘC 3.3V, KHÔNG CẮM 5V!)
 *   - GND: GND chung với ESP32
 * 
 * ĐỊA CHỈ I2C MỤC TIÊU:
 *   - 0x29: Cảm biến khoảng cách Laser ToF (VL53L0X / VL53L1X)
 *   - 0x68: Cảm biến con quay hồi chuyển 6 trục (MPU6050, AD0 = GND)
 * 
 * TÍNH NĂNG BÀI TEST:
 *   1. Quét toàn bộ bus I2C (I2C Scanner) phát hiện nhanh lỗi đứt dây/chập bus.
 *   2. Đọc khoảng cách Laser thời gian thực (mm và cm).
 *   3. Phát hiện cử chỉ "CHẠM MŨI / VUỐT ĐẦU" khi đưa tay sát < 1.5 cm (< 15 mm).
 *   4. Đọc góc nghiêng Pitch & Roll (°), gia tốc tổng (g) và cảnh báo "BỊ BẾ LÊN".
 *   5. Giao diện HUD trực quan trên màn hình LCD ILI9341 & Serial Monitor 115200.
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_VL53L0X.h>
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

static SPIClass tftSPI(FSPI);
static Adafruit_ILI9341 tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// --- CẤU HÌNH PIN I2C ---
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 8
#endif

#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 9
#endif

#ifndef PIN_BOOT_BTN
#define PIN_BOOT_BTN 0
#endif

// --- ĐỊA CHỈ I2C ---
#define ADDR_TOF     0x29
#define ADDR_MPU     0x68

// Trạng thái phần cứng
static bool tofFound = false;
static bool mpuFound = false;
static String detectedDevicesStr = "Scanning...";

// Dữ liệu MPU6050
static float mpuPitch = 0.0f;
static float mpuRoll = 0.0f;
static float mpuAccelG = 1.0f;
static float mpuTempC = 0.0f;
static bool mpuIsPickedUp = false;
static float pitchOffset = 0.0f;
static float rollOffset = 0.0f;

// Dữ liệu Laser ToF
static uint16_t tofDistanceMm = 0;
static bool tofTouchDetected = false;
static String tofModelStr = "None";

// ----------------------------------------------------------------------------
// I2C SCANNER
// ----------------------------------------------------------------------------
void scanI2CBus() {
  Serial.println("\n---------------------------------------------------------");
  Serial.printf("[I2C SCANNER] Quet bus I2C tren SDA=GPIO%d, SCL=GPIO%d...\n", I2C_SDA_PIN, I2C_SCL_PIN);

  detectedDevicesStr = "";
  int count = 0;
  tofFound = false;
  mpuFound = false;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      count++;
      char hexBuf[16];
      snprintf(hexBuf, sizeof(hexBuf), "0x%02X ", address);
      detectedDevicesStr += hexBuf;

      if (address == ADDR_TOF) {
        tofFound = true;
        Serial.printf("  -> [0x29] TIM THAY: Cảm biến khoảng cách Laser ToF (VL53L0X/VL53L1X)\n");
      } else if (address == ADDR_MPU) {
        mpuFound = true;
        Serial.printf("  -> [0x68] TIM THAY: Cảm biến góc nghiêng/gia tốc MPU-6050\n");
      } else if (address == 0x69) {
        mpuFound = true;
        Serial.printf("  -> [0x69] TIM THAY: MPU-6050 (chân AD0 đang nối HIGH/3.3V)\n");
      } else {
        Serial.printf("  -> [0x%02X] TIM THAY: Thiết bị I2C khác\n", address);
      }
    }
  }

  if (count == 0) {
    detectedDevicesStr = "KHONG CO THIET BI (Kiem tra day!)";
    Serial.println("  [CANH BAO] Khong tim thay thiet bi I2C nao! Kiem tra lai nguon 3.3V, GND, SDA va SCL!");
  } else {
    Serial.printf("[I2C SCANNER] Tong cong tim thay %d thiet bi.\n", count);
  }
  Serial.println("---------------------------------------------------------\n");
}

// ----------------------------------------------------------------------------
// DRIVER MPU6050 (DIRECT I2C WIRE)
// ----------------------------------------------------------------------------
bool initMPU6050() {
  if (!mpuFound) return false;

  uint8_t addr = ADDR_MPU;
  // Kiểm tra WHO_AM_I (reg 0x75)
  Wire.beginTransmission(addr);
  Wire.write(0x75);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)addr, (uint8_t)1);
  if (!Wire.available()) return false;
  uint8_t who = Wire.read();
  Serial.printf("[MPU6050] WHO_AM_I = 0x%02X (Chuan = 0x68)\n", who);

  // Đánh thức MPU (PWR_MGMT_1 reg 0x6B = 0x00)
  Wire.beginTransmission(addr);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(10);

  // Bộ lọc DLPF = 3 (~44Hz)
  Wire.beginTransmission(addr);
  Wire.write(0x1A);
  Wire.write(0x03);
  Wire.endTransmission();

  // Dải đo Accel = +/- 2g
  Wire.beginTransmission(addr);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission();

  // Dải đo Gyro = +/- 250 deg/s
  Wire.beginTransmission(addr);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission();

  Serial.println("[MPU6050] Khoi tao thanh cong!");
  return true;
}

void readMPU6050() {
  if (!mpuFound) return;

  uint8_t addr = ADDR_MPU;
  Wire.beginTransmission(addr);
  Wire.write(0x3B); // Đọc từ ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return;

  if (Wire.requestFrom((uint8_t)addr, (uint8_t)14) == 14) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    int16_t rawTemp = (Wire.read() << 8) | Wire.read();
    int16_t gx = (Wire.read() << 8) | Wire.read();
    int16_t gy = (Wire.read() << 8) | Wire.read();
    int16_t gz = (Wire.read() << 8) | Wire.read();

    float fax = (float)ax / 16384.0f;
    float fay = (float)ay / 16384.0f;
    float faz = (float)az / 16384.0f;

    mpuTempC = (rawTemp / 340.0f) + 36.53f;
    mpuAccelG = sqrtf(fax * fax + fay * fay + faz * faz);

    // Tính góc Pitch & Roll
    float roll = atan2f(fay, faz) * 180.0f / M_PI;
    float pitch = atan2f(-fax, sqrtf(fay * fay + faz * faz)) * 180.0f / M_PI;

    mpuRoll = roll - rollOffset;
    mpuPitch = pitch - pitchOffset;

    // Phát hiện trạng thái bị bế lên / nhấc bổng / lật
    mpuIsPickedUp = (fabsf(mpuAccelG - 1.0f) > 0.35f) || (fabsf(mpuPitch) > 40.0f) || (fabsf(mpuRoll) > 40.0f);
  }
}

// ----------------------------------------------------------------------------
// DRIVER LASER TOF (ADAFRUIT VL53L0X)
// ----------------------------------------------------------------------------
static Adafruit_VL53L0X lox = Adafruit_VL53L0X();
static bool loxInitialized = false;

bool initVL53L0X() {
  if (!tofFound) return false;

  Serial.println("[TOF] Dang khoi tao Adafruit VL53L0X...");
  if (lox.begin(ADDR_TOF, false, &Wire)) {
    tofModelStr = "VL53L0X";
    loxInitialized = true;
    Serial.println("[TOF] Adafruit VL53L0X khoi tao thanh cong!");
    return true;
  } else {
    Serial.println("[TOF] Khoi tao Adafruit VL53L0X that bai!");
    return false;
  }
}

uint16_t readVL53L0XDistance() {
  if (!tofFound || !loxInitialized) return 0;

  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false); // false = khong in debug

  if (measure.RangeStatus != 4) { // 4 = out of range
    if (measure.RangeMilliMeter > 0 && measure.RangeMilliMeter < 2500) {
      return measure.RangeMilliMeter;
    }
  }
  return 0;
}

// ----------------------------------------------------------------------------
// GIAO DIỆN MÀN HÌNH HUD (ILI9341 240x320)
// ----------------------------------------------------------------------------
void drawHUD() {
  tft.fillScreen(ILI9341_BLACK);

  // 1. Tiêu đề
  tft.fillRect(0, 0, 320, 26, 0x18C3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 8);
  tft.print("XIAOZHI ROBOT DOG - I2C SENSOR SUITE");

  // 2. Hai badge trạng thái I2C
  int badgeY = 32;
  // Badge ToF
  uint16_t tofCol = tofFound ? ILI9341_GREEN : ILI9341_RED;
  tft.fillRoundRect(10, badgeY, 145, 20, 4, tofCol);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(16, badgeY + 6);
  tft.printf("TOF [0x29]: %s", tofFound ? tofModelStr.c_str() : "NOT FOUND");

  // Badge MPU6050
  uint16_t mpuCol = mpuFound ? ILI9341_GREEN : ILI9341_RED;
  tft.fillRoundRect(165, badgeY, 145, 20, 4, mpuCol);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(171, badgeY + 6);
  tft.printf("MPU [0x68]: %s", mpuFound ? "CONNECTED" : "NOT FOUND");

  // 3. THẺ TRÁI: CẢM BIẾN KHOẢNG CÁCH LASER TOF
  int cardY = 58;
  int cardW = 145;
  int cardH = 100;
  tft.drawRoundRect(10, cardY, cardW, cardH, 6, tofTouchDetected ? 0x07E0 : 0x0410);
  tft.fillRect(11, cardY + 1, cardW - 2, 18, tofTouchDetected ? 0x03E0 : 0x10A2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(16, cardY + 5);
  tft.print("LASER TOF DISTANCE");

  if (tofFound) {
    if (tofDistanceMm > 0) {
      tft.setTextSize(2);
      tft.setTextColor(tofTouchDetected ? ILI9341_GREEN : ILI9341_YELLOW);
      tft.setCursor(16, cardY + 26);
      tft.printf("%.1f cm", tofDistanceMm / 10.0f);

      tft.setTextSize(1);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(16, cardY + 48);
      tft.printf("(%d mm)", tofDistanceMm);
    } else {
      tft.setTextSize(2);
      tft.setTextColor(0x7BEF);
      tft.setCursor(16, cardY + 28);
      tft.print("> 200 cm");
      tft.setTextSize(1);
      tft.setCursor(16, cardY + 50);
      tft.print("(Thong thoang)");
    }

    // Thanh đo khoảng cách
    int barX = 16;
    int barY = cardY + 64;
    int barW = cardW - 12;
    tft.drawRect(barX, barY, barW, 8, ILI9341_WHITE);
    int fillW = map(constrain((int)tofDistanceMm, 0, 1000), 0, 1000, 0, barW - 2);
    tft.fillRect(barX + 1, barY + 1, fillW, 6, tofTouchDetected ? ILI9341_GREEN : ILI9341_CYAN);
    tft.fillRect(barX + 1 + fillW, barY + 1, barW - 2 - fillW, 6, ILI9341_BLACK);

    // Cảnh báo cử chỉ chạm sát
    tft.setTextSize(1);
    tft.setCursor(16, cardY + 78);
    if (tofTouchDetected) {
      tft.setTextColor(ILI9341_GREEN);
      tft.print("*** DUA TAY SAT (<5cm) ***");
    } else {
      tft.setTextColor(ILI9341_LIGHTGREY);
      tft.print("Dua tay sat < 5cm");
    }
  } else {
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(16, cardY + 40);
    tft.print("Mat ket noi I2C!");
    tft.setCursor(16, cardY + 56);
    tft.print("Kiem tra day SDA/SCL");
  }

  // 4. THẺ PHẢI: CẢM BIẾN CON QUAY HỒI CHUYỂN MPU-6050
  tft.drawRoundRect(165, cardY, cardW, cardH, 6, mpuIsPickedUp ? 0xFD20 : 0x0410);
  tft.fillRect(166, cardY + 1, cardW - 2, 18, mpuIsPickedUp ? 0xB240 : 0x10A2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(171, cardY + 5);
  tft.print("MPU6050 POSTURE");

  if (mpuFound) {
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(171, cardY + 26);
    tft.printf("PITCH: %+5.1f DEG", mpuPitch);

    tft.setCursor(171, cardY + 38);
    tft.printf("ROLL : %+5.1f DEG", mpuRoll);

    tft.setCursor(171, cardY + 50);
    tft.setTextColor(ILI9341_CYAN);
    tft.printf("ACCEL: %4.2f g", mpuAccelG);

    tft.setCursor(171, cardY + 62);
    tft.printf("TEMP : %4.1f *C", mpuTempC);

    // Trạng thái vận động
    tft.setCursor(171, cardY + 78);
    if (mpuIsPickedUp) {
      tft.setTextColor(ILI9341_ORANGE);
      tft.print("! BI BE LEN / NGHIENG !");
    } else {
      tft.setTextColor(ILI9341_GREEN);
      tft.print("THANH BANG ON DINH");
    }
  } else {
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(171, cardY + 40);
    tft.print("Mat ket noi I2C!");
    tft.setCursor(171, cardY + 56);
    tft.print("Kiem tra AD0->GND");
  }

  // 5. Khung danh sách thiết bị I2C quét được
  tft.drawRoundRect(10, 164, 300, 48, 4, 0x2124);
  tft.fillRect(11, 165, 298, 46, 0x0841);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(16, 170);
  tft.print("I2C BUS DETECTED: ");
  tft.setTextColor(ILI9341_YELLOW);
  tft.print(detectedDevicesStr.c_str());

  tft.setTextColor(0x7BEF);
  tft.setCursor(16, 184);
  tft.print("LENH SERIAL: 'scan' (Quet lai) | 'zero' (Calib goc)");
  tft.setCursor(16, 196);
  tft.print("NUT BOOT: Nhan de quet lai bus I2C bat ky luc nao");

  // 6. Thanh đáy thông báo cử chỉ
  tft.fillRect(0, 220, 320, 20, tofTouchDetected ? 0x03E0 : 0x10A2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 226);
  if (tofTouchDetected) {
    tft.print(">>> CU CHI: DUA TAY SAT (<5cm) -> WAKE UP!");
  } else if (mpuIsPickedUp) {
    tft.print(">>> CANH BAO: ROBOT BI BE LEN / LAT NGHIENG!");
  } else {
    tft.print(">>> He thong hoat dong on dinh (SDA=8, SCL=9, 3.3V)");
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
  Serial.println("  XIAOZHI ROBOT DOG - I2C SENSORS VALIDATION SUITE      ");
  Serial.println("========================================================");

  // Khởi tạo màn hình ILI9341
  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(27000000);
  tft.setRotation(1);

  // Khởi tạo bus I2C trên GPIO 8 (SDA) và GPIO 9 (SCL) ở tần số 100kHz
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  Wire.setTimeOut(25);
  delay(100);

  // Quét bus I2C lần đầu
  scanI2CBus();

  // Khởi tạo cảm biến nếu tìm thấy
  if (mpuFound) initMPU6050();
  if (tofFound) initVL53L0X();

  drawHUD();
}

// ----------------------------------------------------------------------------
// LOOP CHÍNH
// ----------------------------------------------------------------------------
static unsigned long lastSensorRead = 0;
static unsigned long lastSerialLog = 0;
static String serialInputBuffer = "";

void loop() {
  unsigned long now = millis();

  // 1. Đọc cảm biến định kỳ mỗi 80ms
  if (now - lastSensorRead >= 80) {
    lastSensorRead = now;

    // Đọc MPU6050
    if (mpuFound) {
      readMPU6050();
    }

    // Đọc Laser ToF
    if (tofFound) {
      tofDistanceMm = readVL53L0XDistance();
      tofTouchDetected = (tofDistanceMm > 0 && tofDistanceMm <= 50); // < 5.0cm (50mm)
    }

    // Cập nhật màn hình HUD
    drawHUD();
  }

  // 2. In log Serial định kỳ mỗi 400ms
  if (now - lastSerialLog >= 400) {
    lastSerialLog = now;
    Serial.printf("[SENSOR DATA] TOF: %4d mm (%.1f cm) %s | MPU: Pitch=%+5.1f* Roll=%+5.1f* Acc=%4.2fg %s\n",
                  tofDistanceMm,
                  tofDistanceMm / 10.0f,
                  tofTouchDetected ? "[TOUCH!]" : "       ",
                  mpuPitch,
                  mpuRoll,
                  mpuAccelG,
                  mpuIsPickedUp ? "[PICKED UP!]" : "           ");
  }

  // 3. Đọc lệnh từ Serial Monitor
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        serialInputBuffer.trim();
        serialInputBuffer.toLowerCase();

        if (serialInputBuffer == "scan") {
          scanI2CBus();
          if (mpuFound) initMPU6050();
          if (tofFound) initVL53L0X();
          drawHUD();
        } else if (serialInputBuffer == "zero") {
          pitchOffset += mpuPitch;
          rollOffset += mpuRoll;
          Serial.println("[MPU6050] Da hieu chuan diem 0 (Zero Calibrated)!");
        } else {
          Serial.println("[CMD] Go 'scan' de quet lai I2C | 'zero' de calib goc.");
        }
        serialInputBuffer = "";
      }
    } else {
      serialInputBuffer += c;
    }
  }

  // 4. Nhấn nút BOOT để quét lại bus I2C
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    delay(50);
    if (digitalRead(PIN_BOOT_BTN) == LOW) {
      Serial.println("[BUTTON] Nut BOOT duoc nhan -> Quet lai bus I2C...");
      scanI2CBus();
      if (mpuFound) initMPU6050();
      if (tofFound) initVL53L0X();
      drawHUD();
      while (digitalRead(PIN_BOOT_BTN) == LOW) delay(10);
    }
  }

  delay(10);
}
