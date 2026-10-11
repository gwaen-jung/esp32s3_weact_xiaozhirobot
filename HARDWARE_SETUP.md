# BÁO CÁO TỔNG HỢP PHẦN CỨNG & TIẾN ĐỘ DỰ ÁN XIAOZHI ROBOT DOG
*Ngày cập nhật: 12/10/2026*
*Dự án: Desk Dog Robot AI Xiaozhi trên ESP32-S3 WeAct CoreBoard*

---

## 1. Danh sách linh kiện phần cứng (Hardware Inventory)

| Linh kiện | Model / Chi tiết | Tình trạng kiểm tra | Ghi chú kỹ thuật quan trọng |
| :--- | :--- | :---: | :--- |
| **Vi điều khiển (MCU)** | ESP32-S3 WeAct Studio CoreBoard (N16R8) | ✅ **HOẠT ĐỘNG TỐT** | 16MB Flash, 8MB Octal PSRAM. Nạp qua cổng Type-C UART CH343 (`COM18`). Native USB CDC ở `COM19`. |
| **Màn hình (LCD)** | 2.4 inch SPI TFT LCD (**ILI9341**, 240x320) | ✅ **HOẠT ĐỘNG TỐT** | Dùng bus FSPI phần cứng (`Adafruit_ILI9341`). Chân LED nối 3.3V. |
| **DAC & Khuếch đại âm thanh (Amp)** | WeAct Studio **PCM5100A** I2S DAC + Dual Speaker | ✅ **HOẠT ĐỘNG TỐT** | **Bắt buộc**: Chân `MC` (Master Clock) nối xuống **GND** để bật Internal PLL. Chân `SD` kéo lên **VCC (3.3V)** để unmute. Dùng chung I2S Clock với Mic. |
| **Microphone** | **INMP441** MEMS I2S Omnidirectional Mic | ✅ **HOẠT ĐỘNG TỐT** | Chân L/R nối GND (Kênh trái). Dùng chung BCLK & WS với PCM5100A (I2S Full-Duplex). |
| **Động cơ 4 chân (Servos)** | 4x Micro Servo SG90 / MG90S | ⚠️ **2 CON BỊ HỎNG** | 2 con servo bị kẹt cơ học/hỏng nhông. 2 con còn lại phản hồi góc mượt mà. Cần thay 2 con hỏng hoặc điều chỉnh góc di chuyển an toàn. |
| **Cảm biến Laser khoảng cách** | **VL53L0X** (Time-of-Flight I2C) | ⏳ *Kế hoạch ngày mai* | Giao tiếp I2C: SDA (`GPIO 8`), SCL (`GPIO 9`). |
| **Cảm biến con quay hồi chuyển** | **MPU-6050** (6-DOF IMU I2C) | ⏳ *Kế hoạch ngày mai* | Giao tiếp I2C chung bus với VL53L0X: SDA (`GPIO 8`), SCL (`GPIO 9`). |
| **Trí tuệ nhân tạo (AI Chatbot)** | **Xiaozhi Cloud AI** Voice Assistant | ⏳ *Kế hoạch ngày mai* | Nạp firmware ESP-IDF `xiaozhi-esp32-dog` hỗ trợ tiếng Việt (`vi-VN`), giao tiếp WebSocket/MQTT với server AI Xiaozhi. |

---

## 2. Bảng sơ đồ đấu nối dây chuẩn (Wiring Matrix)

```
                       +-------------------------------+
                       |   ESP32-S3 WeAct CoreBoard    |
                       +-------------------------------+
                         |   |   |   |   |   |   |   |
  ====================== LCD ILI9341 2.4" SPI ======================
  [GPIO 40] --------------> SCK / SCLK (SPI Clock)
  [GPIO 41] --------------> SDI / MOSI (Data In)
  [GPIO 39] --------------> CS (Chip Select)
  [GPIO 38] --------------> DC / RS (Data/Command)
  [GPIO 47] --------------> RST / RESET
  [ 3.3V  ] --------------> LED (Backlight) + VCC
  [  GND  ] --------------> GND

  ================== AUDIO: INMP441 (MIC) + PCM5100A (DAC) ==================
  [GPIO 16] --------------> INMP441 SCK  &  PCM5100A BCK (Shared Clock)
  [GPIO 17] --------------> INMP441 WS   &  PCM5100A WS  (Shared Word Select)
  [GPIO 14] <-------------- INMP441 SD (Data Out from Mic to ESP32 DIN)
  [GPIO 15] --------------> PCM5100A DIN (Data In to DAC from ESP32 DOUT)
  [  GND  ] --------------> INMP441 L/R  &  PCM5100A MC (PLL internal clock enable!)
  [ 3.3V  ] --------------> PCM5100A SD (Unmute) & INMP441 VDD
  [ 5.0V  ] --------------> PCM5100A VCC (Cấp nguồn sạch cho Amp)
  [  GND  ] --------------> Audio GND chung với ESP32

  ============================ 4 CHÂN ROBOT (SERVOS) ============================
  [GPIO 6 ] --------------> Servo Chân Trước Trái (Front-Left - FL)
  [GPIO 7 ] --------------> Servo Chân Trước Phải (Front-Right - FR)
  [GPIO 4 ] --------------> Servo Chân Sau Trái   (Rear-Left - RL)
  [GPIO 5 ] --------------> Servo Chân Sau Phải   (Rear-Right - RR)
  [ 5.0V  ] --------------> Nguồn 5V công suất riêng cho Servo (Khuyên dùng nguồn ngoài / pin)
  [  GND  ] --------------> GND chung với ESP32

  ================== CHUẨN BỊ CHO NGÀY MAI: I2C SENSORS ==================
  [GPIO 8 ] --------------> I2C SDA (Chung cho VL53L0X & MPU6050)
  [GPIO 9 ] --------------> I2C SCL (Chung cho VL53L0X & MPU6050)
  [ 3.3V  ] --------------> VCC cảm biến
  [  GND  ] --------------> GND cảm biến
```

---

## 3. Nhật ký kiểm tra & Kết quả (Validation Log)

1. **Màn hình ILI9341**:
   - Sử dụng thư viện `Adafruit_ILI9341` qua phần cứng FSPI của ESP32-S3. Màn hình hiển thị sắc nét, không bị lỗi treo SPI `StoreProhibited` như thư viện TFT_eSPI thiếu cấu hình FSPI.
   - Hiển thị HUD Dashboard, vẽ biểu tượng mắt RoboEyes và đồng hồ đo góc servo.

2. **Microphone INMP441**:
   - Khởi tạo I2S kênh chuẩn 16kHz, 32-bit (chuyển đổi 16-bit PCM).
   - Đọc mẫu âm thanh chính xác, đo được biên độ âm thanh môi trường và tiếng gõ/nói.

3. **Loa PCM5100A**:
   - Hoàn thành cấu hình DAC WeAct PCM5100A sử dụng bộ tạo xung nội PLL bằng cách nối chân `MC -> GND`.
   - Chạy I2S Full-Duplex chung `BCLK (16)` và `WS (17)` với Mic.
   - Phát âm thanh tần số âm mẫu chuẩn 440Hz, 523Hz, 880Hz trong trẻo, to rõ.

4. **Hệ thống Servo 4 chân**:
   - Đảo thành công vị trí: 2 chân trước = GPIO 6, 7; 2 chân sau = GPIO 4, 5.
   - Đã kiểm tra xoay đồng loạt các góc 45°, 90°, 135° và quét mượt (sweep) với thuật toán tăng giảm tốc Cosine.
   - *Phát hiện*: 2 servo bị lỗi cơ khí phần cứng, cần lưu ý thay thế khi lắp ráp hoàn chỉnh.

---

## 4. Kế hoạch tiếp theo (Next Steps - Ngày mai)
1. **Lắp ráp & Test Cảm biến khoảng cách Laser ToF VL53L0X** trên bus I2C (`SDA=8`, `SCL=9`).
2. **Lắp ráp & Test Cảm biến góc nghiêng MPU-6050** trên bus I2C (`SDA=8`, `SCL=9`) để phát hiện robot ngã, nhấc lên hoặc bị va chạm.
3. **Build & Flash Firmware AI Xiaozhi Dog Robot chính thức (ESP-IDF)**:
   - File cấu hình: `c:\Users\trung\Downloads\xiaozhi-esp32-dog\xiaozhi-esp32`.
   - Kết nối WiFi, ghép nối token trên console Xiaozhi, đàm thoại song ngữ / tiếng Việt với trợ lý AI.
