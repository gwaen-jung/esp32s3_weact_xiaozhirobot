/**
 * =========================================================================
 * BỘ TEST OLED SSD1306 CHO MẠCH ESP32-S3 CLONE
 * Pinout người dùng cung cấp:
 *   SCL = GPIO 1
 *   SDA = GPIO 2
 * =========================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define PIN_SCL 1
#define PIN_SDA 2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledFound = false;
uint8_t detectedAddr = 0;
bool pinsReversed = false;

// Hàm quét I2C bus
uint8_t scanI2C(int sda, int scl) {
  Serial.printf("\n--- Dang quet I2C voi SDA = GPIO %d, SCL = GPIO %d ---\n", sda, scl);
  Wire.end();
  delay(50);
  Wire.begin(sda, scl, 100000); // 100kHz standard mode
  Wire.setTimeOut(50); // timeout 50ms tránh treo

  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  [TIM THAY THIET BI I2C] Tai dia chi: 0x%02X\n", addr);
      if (addr == 0x3C || addr == 0x3D) {
        found = addr;
      } else if (found == 0) {
        found = addr;
      }
    }
  }

  if (found == 0) {
    Serial.println("  [!] Khong tim thay thiet bi nao tren bus nay.");
  }
  return found;
}

// Vẽ đôi mắt robot ngầu
void drawRobotEyes(int eyeOffsetX, int eyeHeight) {
  display.clearDisplay();
  
  // Khung viền status
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.printf("S3 CLONE | 0x%02X", detectedAddr);
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // Mắt trái: x=24, y=20
  int leftEyeX = 24 + eyeOffsetX;
  int rightEyeX = 74 + eyeOffsetX;
  int eyeY = 24 + (28 - eyeHeight) / 2;

  display.fillRoundRect(leftEyeX, eyeY, 30, eyeHeight, 6, SSD1306_WHITE);
  display.fillRoundRect(rightEyeX, eyeY, 30, eyeHeight, 6, SSD1306_WHITE);

  // Điểm sáng trong mắt
  if (eyeHeight > 10) {
    display.fillCircle(leftEyeX + 8, eyeY + 8, 3, SSD1306_BLACK);
    display.fillCircle(rightEyeX + 8, eyeY + 8, 3, SSD1306_BLACK);
  }

  // Footer
  display.setCursor(4, 56);
  display.printf("SCL:IO%d  SDA:IO%d", pinsReversed ? 2 : 1, pinsReversed ? 1 : 2);
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1500); // Đợi serial ổn định sau khi cắm/reset

  Serial.println("\n\n========================================================");
  Serial.println("   XIAOZHI ROBOT - TEST OLED CHO ESP32-S3 CLONE");
  Serial.println("   Cau hinh: DIO Flash, No-PSRAM, Clock standard");
  Serial.println("========================================================");

  // 1. Kiểm tra trạng thái điện áp 2 chân GPIO 1 & 2
  Serial.println("\n[BUOC 1] Kiem tra dien ap GPIO 1 (SCL) va GPIO 2 (SDA)...");
  pinMode(PIN_SCL, INPUT_PULLUP);
  pinMode(PIN_SDA, INPUT_PULLUP);
  delay(10);
  int scl_lvl = digitalRead(PIN_SCL);
  int sda_lvl = digitalRead(PIN_SDA);
  Serial.printf("  GPIO 1 (SCL): %s\n", scl_lvl ? "HIGH (3.3V OK)" : "LOW (CHẬM GND HOẶC NGUỒN CHƯA CÓ!)");
  Serial.printf("  GPIO 2 (SDA): %s\n", sda_lvl ? "HIGH (3.3V OK)" : "LOW (CHẬM GND HOẶC NGUỒN CHƯA CÓ!)");

  // 2. Thử quét I2C với SDA=2, SCL=1
  detectedAddr = scanI2C(PIN_SDA, PIN_SCL);
  pinsReversed = false;

  // Nếu không thấy, thử đảo chiều SDA=1, SCL=2 (phòng khi vẽ mạch bị chéo chân)
  if (detectedAddr == 0) {
    Serial.println("\n[BUOC 2] Khong thay o SDA=2/SCL=1 -> Thu quet dao chan SDA=1, SCL=2...");
    detectedAddr = scanI2C(PIN_SCL, PIN_SDA);
    if (detectedAddr != 0) {
      pinsReversed = true;
      Serial.printf("  ===> PHAT HIEN OLED BI DAO CHAN: SDA la GPIO 1, SCL la GPIO 2!\n");
    }
  }

  // 3. Khởi động màn hình OLED nếu tìm thấy địa chỉ
  if (detectedAddr != 0) {
    Serial.printf("\n[BUOC 3] Dang khoi dong Adafruit_SSD1306 tai dia chi 0x%02X...\n", detectedAddr);
    
    int sda = pinsReversed ? PIN_SCL : PIN_SDA;
    int scl = pinsReversed ? PIN_SDA : PIN_SCL;
    Wire.end();
    Wire.begin(sda, scl, 400000);

    if (display.begin(SSD1306_SWITCHCAPVCC, detectedAddr)) {
      oledFound = true;
      Serial.println("  ===> KHOI DONG OLED THANH CONG! <===");

      // Sáng toàn màn hình 1.5 giây để kiểm tra điểm chết
      Serial.println("  -> Bat sang toan bo man hinh (Test Panel White)...");
      display.clearDisplay();
      display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
      display.display();
      delay(1500);

      // Hiển thị thông tin
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 4);
      display.println("ESP32-S3 CLONE");
      display.setCursor(10, 18);
      display.println("XIAOZHI PCB OK!");
      display.setCursor(10, 32);
      display.printf("OLED: 0x%02X (FOUND)\n", detectedAddr);
      display.setCursor(10, 46);
      display.printf("SCL:%d  SDA:%d\n", scl, sda);
      display.display();
      delay(2000);
    } else {
      Serial.println("  [LOI] Adafruit_SSD1306.begin that bai!");
    }
  } else {
    Serial.println("\n[!] CANH BAO: Khong tim thay bat ky thiet bi I2C nao tren GPIO 1 va GPIO 2!");
    Serial.println("    Kiem tra:");
    Serial.println("    1. Man hinh OLED da duoc cap VCC (3.3V) va GND chua?");
    Serial.println("    2. Chan VCC va GND co bi nguoc khong? (Man OLED cho Shopee thuong co loai GND-VCC, co loai VCC-GND)");
    Serial.println("    3. Tren mach co dien tro keo len (Pull-up 4.7k - 10k) cho SCL/SDA len 3.3V khong?");
  }
}

int frame = 0;
int eyePos = 0;
int dir = 1;

void loop() {
  frame++;

  if (oledFound) {
    // Hiệu ứng mắt robot cử động
    eyePos += dir * 2;
    if (eyePos > 6) dir = -1;
    if (eyePos < -6) dir = 1;

    // Chớp mắt mỗi 20 frame
    int eyeH = (frame % 25 == 0) ? 2 : 26;
    drawRobotEyes(eyePos, eyeH);
  }

  // Báo log định kỳ ra Serial
  if (frame % 5 == 0) {
    Serial.printf("[RUNNING #%d] OLED Status: %s | I2C Addr: 0x%02X | Pins: %s\n",
                  frame,
                  oledFound ? "HOAT DONG TOT" : "CHUA KET NOI",
                  detectedAddr,
                  pinsReversed ? "SDA=IO1, SCL=IO2 (Dao)" : "SCL=IO1, SDA=IO2 (Chuan)");
  }

  delay(100);
}
