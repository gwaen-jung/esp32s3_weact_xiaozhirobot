/**
 * ============================================================================
 * CHƯƠNG TRÌNH QUÉT I2C TỰ ĐỘNG TRÊN CHÂN GPIO 41 & GPIO 42 (ESP32-S3)
 * Môi trường: [env:test_i2c_scan] (PlatformIO)
 * ============================================================================
 * 
 * Tính năng thông minh:
 *  1. Kiểm tra trạng thái điện áp 2 chân (phát hiện chập GND hoặc thiếu trở kéo).
 *  2. Quét cấu hình 1: SDA = GPIO 41, SCL = GPIO 42.
 *  3. Quét cấu hình 2: SDA = GPIO 42, SCL = GPIO 41.
 *     (Giúp người dùng không sợ cắm nhầm chiều SDA / SCL).
 *  4. Tự động nhận diện thiết bị I2C phổ biến (OLED, MPU6050, VL53L0X, BMP280, ...).
 *  5. Nháy LED WS2812 GPIO 48 báo hiệu chương trình đang chạy tốt.
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>

#define PIN_1 41
#define PIN_2 42
#define STATUS_LED_PIN 48

// Danh sách tra cứu tên thiết bị I2C thông dụng
const char* identifyDevice(uint8_t addr) {
  switch (addr) {
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
      return "PCF8574 (I/O Expander) / BH1750 (0x23)";
    case 0x29:
      return "VL53L0X / VL53L1X (Laser ToF Distance Sensor)";
    case 0x36:
      return "MAX17043 (LiPo Fuel Gauge) / AS5600 (Magnetic Encoder)";
    case 0x38: case 0x39:
      return "AHT10 / AHT20 / FT6236 (Touch Controller)";
    case 0x3C: case 0x3D:
      return "SSD1306 / SH1106 (OLED Display 128x64 / 128x32)";
    case 0x40: case 0x41:
      return "INA219 (Current Sensor) / PCA9685 (16-Ch PWM)";
    case 0x44: case 0x45:
      return "SHT30 / SHT31 (Temperature & Humidity)";
    case 0x48: case 0x49: case 0x4A: case 0x4B:
      return "ADS1115 / ADS1015 (ADC) / TMP102";
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
      return "AT24Cxx (EEPROM) / MAX30102 (Pulse Oximeter 0x57)";
    case 0x5A: case 0x5B:
      return "MLX90614 (IR Temp) / CCS811 (Air Quality)";
    case 0x68:
      return "MPU-6050 / MPU-9250 / DS3231 (RTC) / ICM-20602";
    case 0x69:
      return "MPU-6050 (AD0=VCC) / ICM-20602";
    case 0x76: case 0x77:
      return "BMP280 / BME280 / BMP180 / MS5611";
    default:
      return "Thiết bị I2C khác (Custom / Chưa xác định)";
  }
}

// Kiểm tra nhanh mức logic tĩnh của 2 chân với PULLUP nội
void checkBusLines() {
  pinMode(PIN_1, INPUT_PULLUP);
  pinMode(PIN_2, INPUT_PULLUP);
  delay(10);
  int v1 = digitalRead(PIN_1);
  int v2 = digitalRead(PIN_2);

  Serial.println("[BUS CHECK]");
  Serial.printf("  - GPIO 41: %s\n", v1 == HIGH ? "HIGH (OK / Kéo cao)" : "LOW (CHU Y: Co the bi cham GND hoac thieu pull-up!)");
  Serial.printf("  - GPIO 42: %s\n", v2 == HIGH ? "HIGH (OK / Kéo cao)" : "LOW (CHU Y: Co the bi cham GND hoac thieu pull-up!)");
}

int scanBus(int sdaPin, int sclPin, const char* label) {
  Serial.println("------------------------------------------------------------");
  Serial.printf(">> DANG QUET VOI CAU HINH: %s\n", label);
  Serial.printf("   SDA = GPIO %d | SCL = GPIO %d | Speed = 100 kHz\n", sdaPin, sclPin);
  Serial.println("------------------------------------------------------------");

  Wire.end();
  delay(20);
  Wire.begin(sdaPin, sclPin, 100000);
  Wire.setTimeOut(25);
  delay(50);

  int devicesFound = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      devicesFound++;
      Serial.printf("  [+] TIM THAY THIET BI tai dia chi: 0x%02X (Dec %3d)\n", addr, addr);
      Serial.printf("      -> Loai du doan: %s\n", identifyDevice(addr));
    } else if (err == 4) {
      Serial.printf("  [!] Loi khong xac dinh tai dia chi 0x%02X\n", addr);
    }
  }

  if (devicesFound == 0) {
    Serial.println("  [-] Khong phat hien thiet bi nao tren cau hinh nay.");
  } else {
    Serial.printf("  ==> Tong cong: %d thiet bi duoc phat hien!\n", devicesFound);
  }
  Serial.println();

  return devicesFound;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n============================================================");
  Serial.println("     ESP32-S3 I2C SCANNER TREN GPIO 41 & GPIO 42            ");
  Serial.println("============================================================");
  Serial.println("He thong se tu dong thu ca 2 chieu SDA/SCL de dam bao");
  Serial.println("khong bi anh huong neu ban cam nguoc day.");
  Serial.println("============================================================\n");
}

int scanCycle = 1;

void loop() {
  // Nháy nhẹ WS2812 GPIO 48 màu xanh lá (báo hiệu đang quét)
  neopixelWrite(STATUS_LED_PIN, 0, 30, 0);

  Serial.printf("\n========== [LAN QUET SO %d] ==========\n", scanCycle++);
  checkBusLines();

  // Test cấu hình 1: SDA = 41, SCL = 42
  int found1 = scanBus(PIN_1, PIN_2, "SDA=GPIO 41, SCL=GPIO 42");

  // Test cấu hình 2: SDA = 42, SCL = 41
  int found2 = scanBus(PIN_2, PIN_1, "SDA=GPIO 42, SCL=GPIO 41");

  Serial.println("================ TONG KET ================");
  if (found1 > 0 && found2 == 0) {
    Serial.printf("KET QUA: Day duoc dau dung chieu: SDA = GPIO %d, SCL = GPIO %d\n", PIN_1, PIN_2);
  } else if (found2 > 0 && found1 == 0) {
    Serial.printf("KET QUA: Day duoc dau dung chieu: SDA = GPIO %d, SCL = GPIO %d\n", PIN_2, PIN_1);
  } else if (found1 > 0 && found2 > 0) {
    Serial.println("KET QUA: Ca 2 chieu deu phan hoi (kiem tra xem bus co bi loopback khong)!");
  } else {
    Serial.println("KET QUA: KHONG tim thay thiet bi nao!");
    Serial.println("Goi y kiem tra:");
    Serial.println("  1. Thiet bi I2C da duoc cap VCC (3.3V hoac 5V) va GND chung chua?");
    Serial.println("  2. Cac chan SDA/SCL co tro keo Pull-up (4.7k) len 3.3V khong?");
    Serial.println("  3. Day cam co bi tiep xuc kem khong?");
  }
  Serial.println("==========================================\n");

  // Đổi LED sang xanh dương chờ đợt quét tiếp
  neopixelWrite(STATUS_LED_PIN, 0, 0, 20);

  Serial.println("Cho 3 giay truoc lan quet tiep theo...\n");
  delay(3000);
}
