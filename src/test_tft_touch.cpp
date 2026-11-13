/**
 * ============================================================================
 * TEST TFT 2.8" SPI (ILI9341) + TOUCH XPT2046 TRÊN WEACT ESP32-S3
 * ============================================================================
 * Sơ đồ đấu dây chuẩn V03 (Chung SPI bus FSPI):
 *  - TFT_SCK  / T_CLK  -> GPIO 40
 *  - TFT_MOSI / T_DIN  -> GPIO 41
 *  - TFT_MISO / T_DO   -> GPIO 42
 *  - TFT_CS            -> GPIO 39
 *  - TFT_DC            -> GPIO 38
 *  - TFT_RST           -> GPIO 47
 *  - T_CS (Touch CS)   -> GPIO 21
 *  - T_IRQ (Touch IRQ) -> GPIO 18
 *  - VCC / LED         -> 3V3
 *  - GND               -> GND
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS    39
#define TFT_DC    38
#define TFT_RST   47
#define TFT_MOSI  41
#define TFT_SCLK  40
#define TFT_MISO  42

#define TOUCH_CS  21
#define TOUCH_IRQ 18

static SPIClass tftSPI(FSPI);
static Adafruit_ILI9341 tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// Driver đọc XPT2046 trực tiếp qua SPI (không cần thư viện ngoài)
class SimpleXPT2046 {
private:
  int csPin;
  int irqPin;
  SPIClass* spi;

  uint16_t transferCmd(uint8_t cmd) {
    spi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(csPin, LOW);
    spi->transfer(cmd);
    uint16_t msb = spi->transfer(0x00);
    uint16_t lsb = spi->transfer(0x00);
    digitalWrite(csPin, HIGH);
    spi->endTransaction();
    return ((msb << 8) | lsb) >> 3;
  }

public:
  SimpleXPT2046(int cs, int irq, SPIClass* s) : csPin(cs), irqPin(irq), spi(s) {}

  void begin() {
    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, HIGH);
    pinMode(irqPin, INPUT_PULLUP);
  }

  bool isTouched() {
    return (digitalRead(irqPin) == LOW);
  }

  // Đọc trực tiếp Z1, Z2, X, Y qua SPI (luôn kết thúc bằng PD=00 để giữ T_IRQ hoạt động,
  // đồng thời hoạt động được ngay cả khi không cắm dây T_IRQ)
  bool readRaw(uint16_t &rawX, uint16_t &rawY, uint16_t &rawZ, uint16_t *outZ1 = nullptr, uint16_t *outZ2 = nullptr) {
    digitalWrite(TFT_CS, HIGH); // Đảm bảo nhả CS của màn hình ILI9341 trước khi đọc Touch

    // Lấy áp suất chạm (Z1, Z2) - dùng 0xB1, 0xC1
    uint16_t z1 = transferCmd(0xB1);
    uint16_t z2 = transferCmd(0xC1);
    if (outZ1) *outZ1 = z1;
    if (outZ2) *outZ2 = z2;

    // Đọc X và Y trung bình 4 lần, lệnh cuối dùng PD=00 (0x90, 0xD0) để bật lại PENIRQ
    uint32_t xSum = 0, ySum = 0;
    for (int i = 0; i < 4; i++) {
      xSum += transferCmd(0x91); // Đọc X (ADC ON)
      ySum += transferCmd(0xD1); // Đọc Y (ADC ON)
    }
    // Lệnh cuối cùng đưa về PD=00 để kích hoạt lại chân T_IRQ
    transferCmd(0xD0);

    rawX = xSum / 4;
    rawY = ySum / 4;

    // Tính áp suất chạm Z: khi chạm thực tế, z1 > 50 và (4095 - z2 + z1) tăng cao
    int32_t z = (int32_t)z1 + 4095 - (int32_t)z2;
    if (z < 0) z = 0;
    rawZ = (uint16_t)z;

    // Điều kiện chạm hợp lệ: z1 phải > 40 (có dòng qua tấm điện trở) và z1 < 4090 (không bị treo mức cao)
    if (z1 > 40 && z1 < 4090 && rawZ > 350 && rawX > 100 && rawX < 4000 && rawY > 100 && rawY < 4000) {
      return true;
    }
    return false;
  }
};

static SimpleXPT2046 touch(TOUCH_CS, TOUCH_IRQ, &tftSPI);

void setup() {
  Serial.begin(115200);
  delay(400);

  Serial.println("\n========================================================");
  Serial.println("  TRIAD / RESHAPE LAB - TEST TFT 2.8\" + TOUCH XPT2046");
  Serial.println("========================================================");

  // Khởi động SPI bus chung
  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);

  // Khởi động màn hình ILI9341
  tft.begin();
  tft.setRotation(1); // Màn hình ngang (320x240)
  tft.fillScreen(ILI9341_BLACK);

  // Khởi động cảm ứng XPT2046
  touch.begin();

  // Vẽ giao diện ban đầu
  tft.fillRect(0, 0, 320, 26, 0x18E3);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 9);
  tft.print("RESHAPE LAB: ROBOT AI V03 - TOUCH TEST");

  tft.drawRect(5, 32, 310, 175, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 90);
  tft.print("CHAM TAY VAO MAN HINH!");
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(20, 120);
  tft.print("Diem cham se ve truc tiep tren man hinh...");

  // Thanh trạng thái dưới cùng
  tft.fillRect(0, 215, 320, 25, 0x0841);
  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(10, 222);
  tft.print("Status: TOUCH WAITING...");

  Serial.println("[OK] TFT 2.8\" ILI9341 & XPT2046 Initialized!");
}

void loop() {
  uint16_t rawX, rawY, rawZ, z1, z2;
  static uint32_t lastTouchTime = 0;
  static uint32_t lastDiagTime = 0;
  static bool wasTouched = false;

  if (touch.readRaw(rawX, rawY, rawZ, &z1, &z2)) {
    // Ánh xạ tọa độ raw (khoảng 300 - 3800) sang màn hình 320x240 ngang
    int screenX = map(rawX, 350, 3850, 0, 320);
    int screenY = map(rawY, 350, 3850, 0, 240);
    screenX = constrain(screenX, 0, 319);
    screenY = constrain(screenY, 0, 239);

    // Xóa chữ thông báo ở lần chạm đầu tiên
    if (!wasTouched) {
      tft.fillRect(6, 33, 308, 173, ILI9341_BLACK);
      wasTouched = true;
    }

    // Vẽ điểm chạm trên màn hình
    tft.fillCircle(screenX, screenY, 3, ILI9341_RED);

    // Cập nhật thanh trạng thái
    tft.fillRect(0, 215, 320, 25, 0x0841);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(10, 222);
    tft.printf("TOUCH! X:%3d Y:%3d  Raw(%4d,%4d,Z:%4d)", screenX, screenY, rawX, rawY, rawZ);

    Serial.printf("[TOUCH] Screen(%3d, %3d) | Raw X:%4d Y:%4d Z:%4d (Z1:%4d Z2:%4d IRQ:%d)\n",
                  screenX, screenY, rawX, rawY, rawZ, z1, z2, digitalRead(TOUCH_IRQ));
    lastTouchTime = millis();
    delay(10);
  } else {
    if (millis() - lastDiagTime >= 500) {
      lastDiagTime = millis();
      Serial.printf("[DIAG] IRQ:%d | Z1:%4d Z2:%4d Z:%4d | X:%4d Y:%4d\n",
                    digitalRead(TOUCH_IRQ), z1, z2, rawZ, rawX, rawY);
    }
    if (wasTouched && (millis() - lastTouchTime > 4000)) {
      // 4 giây không chạm thì hiện lại RoboEyes
      tft.fillRect(6, 33, 308, 173, ILI9341_BLACK);
      
      // Vẽ 2 mắt RoboEyes xanh ngọc
      tft.fillRoundRect(80, 75, 65, 80, 16, 0x07FF);
      tft.fillRoundRect(175, 75, 65, 80, 16, 0x07FF);

      tft.fillRect(0, 215, 320, 25, 0x0841);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(10, 222);
      tft.print("Status: ROBO-EYES IDLE (Cham de ve tiep)");
      wasTouched = false;
    }
  }
}
