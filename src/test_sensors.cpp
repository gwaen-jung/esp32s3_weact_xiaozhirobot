/**
 * ============================================================================
 * XIAOZHI ROBOT DOG - BÀI TEST CHUYÊN BIỆT CẢM BIẾN I2C (LASER TOF & MPU6050)
 * Môi trường: [env:test_sensors] (PlatformIO)
 * MCU: WeAct Studio ESP32-S3 CoreBoard N16R8
 * Display: 2.4" SPI TFT LCD (ILI9341, 240x320)
 * ============================================================================
 *
 * SƠ ĐỒ ĐẤU NỐI I2C:
 *   - SDA: GPIO 8, SCL: GPIO 9
 *   - VCC: 3.3V (KHÔNG CẮM 5V!), GND chung với ESP32
 *
 * ĐỊA CHỈ I2C MỤC TIÊU:
 *   - 0x29: Laser ToF VL53L0X (thư viện Adafruit_VL53L0X; VL53L1X KHÔNG được hỗ trợ)
 *   - 0x68 / 0x69: MPU6050 (AD0 = GND / AD0 = 3.3V)
 *
 * TÍNH NĂNG:
 *   1. I2C Scanner phát hiện đứt dây/chập bus.
 *   2. Đo khoảng cách Laser chế độ liên tục (không chặn loop).
 *   3. Cử chỉ "ĐƯA TAY SÁT" khi < 5 cm (VL53L0X đo kém tin cậy dưới ~3 cm).
 *   4. Pitch/Roll, gia tốc tổng, cảnh báo "BỊ NHẤC / NGHIÊNG".
 *   5. HUD ILI9341: khung tĩnh vẽ 1 lần, vùng số liệu vẽ qua canvas -> không nháy.
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_VL53L0X.h>
#include <math.h>

// --- PIN MÀN HÌNH ---
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

// --- PIN I2C ---
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
#define ADDR_TOF       0x29
#define ADDR_MPU_LOW   0x68
#define ADDR_MPU_HIGH  0x69

#define TOUCH_THRESHOLD_MM 50   // < 5 cm

// Trạng thái phần cứng
static bool tofFound = false;
static bool mpuFound = false;
static uint8_t mpuAddr = ADDR_MPU_LOW;
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
static Adafruit_VL53L0X lox = Adafruit_VL53L0X();
static bool loxInitialized = false;
static uint16_t tofDistanceMm = 0;   // 0 = ngoài tầm / không có số đo
static bool tofTouchDetected = false;

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
    if (Wire.endTransmission() != 0) continue;

    count++;
    char hexBuf[8];
    snprintf(hexBuf, sizeof(hexBuf), "0x%02X ", address);
    detectedDevicesStr += hexBuf;

    if (address == ADDR_TOF) {
      tofFound = true;
      Serial.println("  -> [0x29] TIM THAY: Laser ToF (VL53L0X/VL53L1X)");
    } else if (address == ADDR_MPU_LOW || address == ADDR_MPU_HIGH) {
      if (!mpuFound) mpuAddr = address;  // ưu tiên địa chỉ tìm thấy đầu tiên
      mpuFound = true;
      Serial.printf("  -> [0x%02X] TIM THAY: MPU-6050 (AD0 = %s)\n", address,
                    address == ADDR_MPU_LOW ? "GND" : "3.3V");
    } else {
      Serial.printf("  -> [0x%02X] TIM THAY: Thiet bi I2C khac\n", address);
    }
  }

  if (count == 0) {
    detectedDevicesStr = "KHONG CO (kiem tra day!)";
    Serial.println("  [CANH BAO] Khong tim thay thiet bi I2C nao! Kiem tra 3.3V, GND, SDA, SCL!");
  } else {
    Serial.printf("[I2C SCANNER] Tong cong tim thay %d thiet bi.\n", count);
  }
  Serial.println("---------------------------------------------------------\n");
}

// ----------------------------------------------------------------------------
// DRIVER MPU6050 (DIRECT I2C WIRE)
// ----------------------------------------------------------------------------
static bool mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool initMPU6050() {
  if (!mpuFound) return false;

  Wire.beginTransmission(mpuAddr);
  Wire.write(0x75);  // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddr, (uint8_t)1) != 1) return false;
  uint8_t who = Wire.read();
  Serial.printf("[MPU6050] WHO_AM_I = 0x%02X (MPU6050 = 0x68, MPU6500 = 0x70)\n", who);

  bool ok = mpuWriteReg(0x6B, 0x00);  // PWR_MGMT_1: wake up
  delay(10);
  ok &= mpuWriteReg(0x1A, 0x03);      // DLPF ~44Hz
  ok &= mpuWriteReg(0x1C, 0x00);      // Accel +/-2g
  ok &= mpuWriteReg(0x1B, 0x00);      // Gyro +/-250 deg/s

  Serial.println(ok ? "[MPU6050] Khoi tao thanh cong!" : "[MPU6050] Loi ghi thanh ghi!");
  return ok;
}

void readMPU6050() {
  if (!mpuFound) return;

  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom(mpuAddr, (uint8_t)14) != 14) return;

  uint8_t raw[14];
  for (int i = 0; i < 14; i++) raw[i] = Wire.read();
  int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
  int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
  int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
  int16_t rawTemp = (int16_t)((raw[6] << 8) | raw[7]);
  // raw[8..13] = gyro, chưa dùng trong bài test này

  float fax = ax / 16384.0f;
  float fay = ay / 16384.0f;
  float faz = az / 16384.0f;

  mpuTempC = (rawTemp / 340.0f) + 36.53f;
  mpuAccelG = sqrtf(fax * fax + fay * fay + faz * faz);

  float roll = atan2f(fay, faz) * 180.0f / M_PI;
  float pitch = atan2f(-fax, sqrtf(fay * fay + faz * faz)) * 180.0f / M_PI;
  mpuRoll = roll - rollOffset;
  mpuPitch = pitch - pitchOffset;

  mpuIsPickedUp = (fabsf(mpuAccelG - 1.0f) > 0.35f) || (fabsf(mpuPitch) > 40.0f) || (fabsf(mpuRoll) > 40.0f);
}

// ----------------------------------------------------------------------------
// DRIVER LASER TOF (ADAFRUIT VL53L0X, CHẾ ĐỘ LIÊN TỤC)
// ----------------------------------------------------------------------------
bool initVL53L0X() {
  if (loxInitialized) {
    lox.stopRangeContinuous();
    loxInitialized = false;
  }
  if (!tofFound) return false;

  Serial.println("[TOF] Dang khoi tao Adafruit VL53L0X...");
  if (lox.begin(ADDR_TOF, false, &Wire) && lox.startRangeContinuous(50)) {
    loxInitialized = true;
    Serial.println("[TOF] VL53L0X khoi tao thanh cong (continuous 50ms)!");
    return true;
  }
  Serial.println("[TOF] Khoi tao VL53L0X that bai! (Neu module la VL53L1X can thu vien khac)");
  return false;
}

// Chỉ đọc khi có kết quả mới -> không chặn loop
void pollVL53L0X() {
  if (!loxInitialized || !lox.isRangeComplete()) return;
  uint16_t r = lox.readRangeResult();  // 0xFFFF khi lỗi / ngoài tầm
  tofDistanceMm = (r > 0 && r < 2000) ? r : 0;
  tofTouchDetected = (tofDistanceMm > 0 && tofDistanceMm <= TOUCH_THRESHOLD_MM);
}

// ----------------------------------------------------------------------------
// GIAO DIỆN HUD (320x240 sau setRotation(1))
//  - drawStaticHUD(): vẽ khung tĩnh, chỉ gọi khi khởi động / quét lại bus
//  - updateDynamicHUD(): vẽ số liệu vào canvas rồi đẩy lên màn -> không nháy
// ----------------------------------------------------------------------------
#define CARD_Y   58
#define CARD_W   145
#define CARD_H   100
#define LEFT_X   10
#define RIGHT_X  165
#define BODY_W   (CARD_W - 4)
#define BODY_H   (CARD_H - 22)

static GFXcanvas16 leftBody(BODY_W, BODY_H);
static GFXcanvas16 rightBody(BODY_W, BODY_H);
static GFXcanvas16 bottomBar(320, 20);
static int lastTofHdr = -1, lastMpuHdr = -1;  // cache trạng thái header thẻ

static void drawCardHeader(int x, const char* title, bool alert, uint16_t alertBorder, uint16_t alertFill) {
  tft.drawRoundRect(x, CARD_Y, CARD_W, CARD_H, 6, alert ? alertBorder : 0x0410);
  tft.fillRect(x + 1, CARD_Y + 1, CARD_W - 2, 18, alert ? alertFill : 0x10A2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(x + 6, CARD_Y + 6);
  tft.print(title);
}

void drawStaticHUD() {
  tft.fillScreen(ILI9341_BLACK);

  // 1. Tiêu đề
  tft.fillRect(0, 0, 320, 26, 0x18C3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 9);
  tft.print("XIAOZHI ROBOT DOG - I2C SENSOR SUITE");

  // 2. Badge trạng thái
  char buf[32];
  uint16_t tofCol = loxInitialized ? ILI9341_GREEN : (tofFound ? ILI9341_ORANGE : ILI9341_RED);
  tft.fillRoundRect(LEFT_X, 32, CARD_W, 20, 4, tofCol);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(LEFT_X + 6, 38);
  tft.printf("TOF [0x29]: %s", loxInitialized ? "VL53L0X" : (tofFound ? "INIT LOI" : "KHONG CO"));

  uint16_t mpuCol = mpuFound ? ILI9341_GREEN : ILI9341_RED;
  tft.fillRoundRect(RIGHT_X, 32, CARD_W, 20, 4, mpuCol);
  tft.setCursor(RIGHT_X + 6, 38);
  if (mpuFound) snprintf(buf, sizeof(buf), "MPU [0x%02X]: OK", mpuAddr);
  else snprintf(buf, sizeof(buf), "MPU: KHONG CO");
  tft.print(buf);

  // 3. Khung danh sách thiết bị + hướng dẫn
  tft.drawRoundRect(10, 164, 300, 50, 4, 0x2124);
  tft.fillRect(11, 165, 298, 48, 0x0841);
  tft.setCursor(16, 170);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("I2C: ");
  tft.setTextColor(ILI9341_YELLOW);
  tft.print(detectedDevicesStr);
  tft.setTextColor(0x7BEF);
  tft.setCursor(16, 184);
  tft.print("SERIAL: 'scan' quet lai | 'zero' calib goc");
  tft.setCursor(16, 198);
  tft.print("BOOT: nhan de quet lai bus I2C");

  // Ép updateDynamicHUD vẽ lại header thẻ
  lastTofHdr = -1;
  lastMpuHdr = -1;
}

void updateDynamicHUD() {
  // Header thẻ chỉ vẽ lại khi đổi trạng thái
  if (lastTofHdr != (int)tofTouchDetected) {
    drawCardHeader(LEFT_X, "LASER TOF DISTANCE", tofTouchDetected, 0x07E0, 0x03E0);
    lastTofHdr = tofTouchDetected;
  }
  if (lastMpuHdr != (int)mpuIsPickedUp) {
    drawCardHeader(RIGHT_X, "MPU6050 POSTURE", mpuIsPickedUp, 0xFD20, 0xB240);
    lastMpuHdr = mpuIsPickedUp;
  }

  // --- Thẻ trái: ToF ---
  GFXcanvas16& L = leftBody;
  L.fillScreen(ILI9341_BLACK);
  L.setTextWrap(false);
  if (loxInitialized) {
    if (tofDistanceMm > 0) {
      L.setTextSize(2);
      L.setTextColor(tofTouchDetected ? ILI9341_GREEN : ILI9341_YELLOW);
      L.setCursor(4, 6);
      L.printf("%.1f cm", tofDistanceMm / 10.0f);
      L.setTextSize(1);
      L.setTextColor(ILI9341_CYAN);
      L.setCursor(4, 28);
      L.printf("(%u mm)", tofDistanceMm);
    } else {
      L.setTextSize(2);
      L.setTextColor(0x7BEF);
      L.setCursor(4, 6);
      L.print("> 200 cm");
      L.setTextSize(1);
      L.setCursor(4, 28);
      L.print("(ngoai tam)");
    }
    int barW = BODY_W - 8;
    L.drawRect(4, 44, barW, 8, ILI9341_WHITE);
    int fillW = map(constrain((int)tofDistanceMm, 0, 1000), 0, 1000, 0, barW - 2);
    L.fillRect(5, 45, fillW, 6, tofTouchDetected ? ILI9341_GREEN : ILI9341_CYAN);
    L.setCursor(4, 60);
    L.setTextColor(tofTouchDetected ? ILI9341_GREEN : ILI9341_LIGHTGREY);
    L.print(tofTouchDetected ? "** TAY SAT < 5cm **" : "Dua tay sat < 5cm");
  } else {
    L.setTextSize(1);
    L.setTextColor(ILI9341_RED);
    L.setCursor(4, 20);
    L.print(tofFound ? "Co 0x29 nhung init loi" : "Mat ket noi I2C!");
    L.setCursor(4, 36);
    L.print(tofFound ? "VL53L1X? doi thu vien" : "Kiem tra SDA/SCL");
  }
  tft.drawRGBBitmap(LEFT_X + 2, CARD_Y + 20, L.getBuffer(), BODY_W, BODY_H);

  // --- Thẻ phải: MPU ---
  GFXcanvas16& R = rightBody;
  R.fillScreen(ILI9341_BLACK);
  R.setTextWrap(false);
  R.setTextSize(1);
  if (mpuFound) {
    R.setTextColor(ILI9341_YELLOW);
    R.setCursor(4, 6);  R.printf("PITCH: %+5.1f DEG", mpuPitch);
    R.setCursor(4, 18); R.printf("ROLL : %+5.1f DEG", mpuRoll);
    R.setTextColor(ILI9341_CYAN);
    R.setCursor(4, 30); R.printf("ACCEL: %4.2f g", mpuAccelG);
    R.setCursor(4, 42); R.printf("TEMP : %4.1f *C", mpuTempC);
    R.setCursor(4, 60);
    R.setTextColor(mpuIsPickedUp ? ILI9341_ORANGE : ILI9341_GREEN);
    R.print(mpuIsPickedUp ? "! BI NHAC / NGHIENG !" : "THANG BANG ON DINH");
  } else {
    R.setTextColor(ILI9341_RED);
    R.setCursor(4, 20); R.print("Mat ket noi I2C!");
    R.setCursor(4, 36); R.print("Kiem tra day, AD0");
  }
  tft.drawRGBBitmap(RIGHT_X + 2, CARD_Y + 20, R.getBuffer(), BODY_W, BODY_H);

  // --- Thanh đáy ---
  GFXcanvas16& B = bottomBar;
  B.fillScreen(tofTouchDetected ? 0x03E0 : (mpuIsPickedUp ? 0xB240 : 0x10A2));
  B.setTextWrap(false);
  B.setTextSize(1);
  B.setTextColor(ILI9341_WHITE);
  B.setCursor(8, 6);
  if (tofTouchDetected)   B.print(">>> CU CHI: DUA TAY SAT -> WAKE UP!");
  else if (mpuIsPickedUp) B.print(">>> CANH BAO: ROBOT BI NHAC / NGHIENG!");
  else                    B.print(">>> On dinh (SDA=8, SCL=9, 3.3V)");
  tft.drawRGBBitmap(0, 220, B.getBuffer(), 320, 20);
}

void rescanAll() {
  scanI2CBus();
  if (mpuFound) initMPU6050();
  initVL53L0X();  // tự dừng phiên cũ + khởi tạo lại nếu tìm thấy
  if (!tofFound) { tofDistanceMm = 0; tofTouchDetected = false; }
  drawStaticHUD();
  updateDynamicHUD();
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

  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(1);
  tft.setTextWrap(false);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  Wire.setTimeOut(25);
  delay(100);

  if (!leftBody.getBuffer() || !rightBody.getBuffer() || !bottomBar.getBuffer()) {
    Serial.println("[HUD] LOI: khong du RAM cho canvas!");
  }

  rescanAll();
}

// ----------------------------------------------------------------------------
// LOOP CHÍNH
// ----------------------------------------------------------------------------
static unsigned long lastSensorRead = 0;
static unsigned long lastHudUpdate = 0;
static unsigned long lastSerialLog = 0;
static String serialInputBuffer = "";

void loop() {
  unsigned long now = millis();

  // 1. Đọc cảm biến (ToF không chặn, MPU ~1ms)
  pollVL53L0X();
  if (now - lastSensorRead >= 20) {
    lastSensorRead = now;
    readMPU6050();
  }

  // 2. Cập nhật HUD ~10 FPS
  if (now - lastHudUpdate >= 100) {
    lastHudUpdate = now;
    updateDynamicHUD();
  }

  // 3. Log Serial mỗi 400ms
  if (now - lastSerialLog >= 400) {
    lastSerialLog = now;
    Serial.printf("[SENSOR DATA] TOF: %4u mm (%.1f cm) %s | MPU: Pitch=%+5.1f* Roll=%+5.1f* Acc=%4.2fg %s\n",
                  tofDistanceMm, tofDistanceMm / 10.0f,
                  tofTouchDetected ? "[TOUCH!]" : "        ",
                  mpuPitch, mpuRoll, mpuAccelG,
                  mpuIsPickedUp ? "[PICKED UP!]" : "");
  }

  // 4. Lệnh Serial
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        serialInputBuffer.trim();
        serialInputBuffer.toLowerCase();
        if (serialInputBuffer == "scan") {
          rescanAll();
        } else if (serialInputBuffer == "zero") {
          pitchOffset += mpuPitch;
          rollOffset += mpuRoll;
          Serial.println("[MPU6050] Da hieu chuan diem 0 (Zero Calibrated)!");
        } else {
          Serial.println("[CMD] Go 'scan' de quet lai I2C | 'zero' de calib goc.");
        }
        serialInputBuffer = "";
      }
    } else if (serialInputBuffer.length() < 64) {
      serialInputBuffer += c;
    }
  }

  // 5. Nút BOOT -> quét lại bus
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    delay(50);
    if (digitalRead(PIN_BOOT_BTN) == LOW) {
      Serial.println("[BUTTON] Nut BOOT duoc nhan -> Quet lai bus I2C...");
      rescanAll();
      while (digitalRead(PIN_BOOT_BTN) == LOW) delay(10);
    }
  }

  delay(2);
}
