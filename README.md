<div align="center">

# XIAOZHI DESK ROBOT DOG (ESP32-S3 WeAct CoreBoard N16R8)
### Hệ thống Robot AI Để Bàn Điều Khiển Bằng Giọng Nói & Chuyển Động 4 Chân

</div>

---

## 📌 Tổng Quan Dự Án & Tiến Độ Hiện Tại

Dự án phát triển robot trợ lý AI thông minh dạng chó để bàn (Desk Robot Dog) sử dụng vi điều khiển **ESP32-S3 WeAct Studio CoreBoard (N16R8)**. 

### Tình trạng kiểm tra phần cứng (Đã hoàn thành hôm nay):
1. ✅ **Màn hình hiển thị**: **TFT LCD 2.4 inch SPI (Driver ILI9341, 240x320)**
   - Đã chạy mượt mà trên bus phần cứng **FSPI** (`Adafruit_ILI9341`).
   - Hiển thị RoboEyes biểu cảm động, HUD trạng thái góc các chân, và thang đo âm thanh VU Meter.
2. ✅ **Phát âm thanh (Amp/DAC)**: **WeAct Studio PCM5100A I2S DAC + Dual Speaker**
   - Đã kiểm tra phát âm thanh tần số chuẩn và giai điệu nhạc chime trong trẻo.
   - **Lưu ý cấu hình quan trọng**: Chân `MC` (Master Clock) bắt buộc nối xuống **GND** để kích hoạt bộ nhân xung nội PLL. Chân `SD` kéo lên **3.3V / VCC** để bật âm thanh (Unmute).
3. ✅ **Thu âm giọng nói (Microphone)**: **INMP441 MEMS I2S Omnidirectional Mic**
   - Chạy chuẩn I2S Full-Duplex dùng chung Clock (`BCLK = 16`, `WS = 17`) với PCM5100A.
   - Đã đo và hiển thị biên độ âm thanh giọng nói / vỗ tay trực tiếp lên màn hình.
4. ⚠️ **Hệ thống 4 Động cơ Chân (Servos)**: 4x Micro Servo SG90 / MG90S
   - Đã cấu hình và test đảo chân theo đúng vị trí cơ khí:
     - **Hai chân trước**: FL = `GPIO 6`, FR = `GPIO 7`
     - **Hai chân sau**: RL = `GPIO 4`, RR = `GPIO 5`
   - Đã lập trình thuật toán điều khiển mượt mà tăng/giảm tốc Cosine S-Curve (các tư thế Stand, Sit, Wave, Crouch, Sweep).
   - **Ghi chú phần cứng**: Phát hiện 2 servo bị kẹt cơ học / hỏng nhông, cần thay thế trong đợt lắp ráp tiếp theo.

---

## 🔌 Sơ Đồ Đấu Nối Chuẩn (Pinout Mapping)

```
                            ┌───────────────────┐
                      3.3V ─┤ 3V3           GND ├─ GND
                       EN  ─┤ EN             44 ├─ (UART0 RX)
          SERVO_RL (Sau-Tr) ─┤ 4              43 ├─ (UART0 TX)
          SERVO_RR (Sau-Ph) ─┤ 5              42 ├─ (Reserved / TFT MISO)
          SERVO_FL (Tr-Tr)  ─┤ 6              41 ├─ TFT_MOSI / SDI
          SERVO_FR (Tr-Ph)  ─┤ 7              40 ├─ TFT_SCLK / SCK
        I2C_SDA (Laser/IMU) ─┤ 8              39 ├─ TFT_CS
        I2C_SCL (Laser/IMU) ─┤ 9              38 ├─ TFT_DC / RS
                   (Free)  ─┤ 10             37 ├─ (PSRAM OPI - CẤM DÙNG)
                   (Free)  ─┤ 11             36 ├─ (PSRAM OPI - CẤM DÙNG)
                   (Free)  ─┤ 12             35 ├─ (PSRAM OPI - CẤM DÙNG)
                   (Free)  ─┤ 13              0 ├─ BOOT Button (Sweep Test)
         I2S_DIN (Mic SD)  ─┤ 14             45 ├─ (Strapping - CẤM DÙNG)
        I2S_DOUT (Amp DIN) ─┤ 15             48 ├─ Onboard WS2812 RGB LED
        I2S_BCLK (Shared)  ─┤ 16             47 ├─ TFT_RST / RESET
         I2S_WS  (Shared)  ─┤ 17             21 ├─ (Free GPIO)
                   (Free)  ─┤ 18             19 ├─ USB D- (Native USB)
                       5V  ─┤ 5V             20 ├─ USB D+ (Native USB)
                            └───────────────────┘
                                    [USB-C]
```

### Bảng Kết Nối Chi Tiết

| Chức năng | Module | Chân Module | Chân ESP32-S3 | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- |
| **Màn hình LCD 2.4"** | **ILI9341 SPI** | SCK / SCLK | **GPIO 40** | Bus FSPI Clock |
| | | MOSI / SDI | **GPIO 41** | Bus FSPI Data |
| | | CS | **GPIO 39** | Chip Select |
| | | DC / RS | **GPIO 38** | Data/Command |
| | | RESET | **GPIO 47** | Reset chân cứng |
| | | LED | **3.3V** | Đèn nền (Backlight) |
| | | VCC / GND | **3.3V / GND** | Nguồn màn hình |
| **Microphone** | **INMP441 I2S** | SCK | **GPIO 16** | Dùng chung Bit Clock |
| | | WS | **GPIO 17** | Dùng chung Word Select |
| | | SD | **GPIO 14** | Data Out từ Mic vào ESP32 DIN |
| | | L/R | **GND** | Kênh trái (Left Channel) |
| | | VDD / GND | **3.3V / GND** | Nguồn Mic |
| **DAC & Loa** | **PCM5100A I2S** | BCK | **GPIO 16** | Dùng chung Bit Clock |
| | | WS | **GPIO 17** | Dùng chung Word Select |
| | | DIN | **GPIO 15** | Data Out từ ESP32 DOUT |
| | | MC | **GND** | **NỐI GND ĐỂ BẬT INTERNAL PLL** |
| | | SD | **3.3V / VCC** | Unmute DAC |
| | | VCC / GND | **5V / GND** | Nguồn công suất loa |
| **4 Chân Servos** | **SG90 / MG90S** | FL Signal | **GPIO 6** | Chân trước bên trái |
| | | FR Signal | **GPIO 7** | Chân trước bên phải |
| | | RL Signal | **GPIO 4** | Chân sau bên trái |
| | | RR Signal | **GPIO 5** | Chân sau bên phải |
| | | VCC / GND | **5V Nguồn ngoài / GND** | Mass chung với ESP32 |
| **Cảm biến (Dự kiến)**| **VL53L0X & MPU6050** | SDA | **GPIO 8** | I2C Data chung |
| | | SCL | **GPIO 9** | I2C Clock chung |
| | | VCC / GND | **3.3V / GND** | Nguồn cảm biến |

---

## 🚀 Kế Hoạch Tiếp Theo (Roadmap Ngày Mai)

1. **Cảm biến đo khoảng cách Laser ToF VL53L0X**:
   - Kết nối vào bus I2C (`SDA=8`, `SCL=9`).
   - Lập trình nhận diện vật cản trước mặt và nhận diện người đến gần.
2. **Cảm biến con quay hồi chuyển & gia tốc MPU-6050**:
   - Kết nối chung bus I2C (`SDA=8`, `SCL=9`).
   - Nhận biết góc nghiêng của robot, trạng thái bị nhấc bổng hoặc ngã để kích hoạt phản ứng tự vệ/kêu cứu.
3. **Firmware Trợ lý AI Xiaozhi (ESP-IDF Voice Chatbot)**:
   - Sử dụng source code `c:\Users\trung\Downloads\xiaozhi-esp32-dog\xiaozhi-esp32`.
   - Kết nối Wi-Fi, tích hợp đàm thoại AI Xiaozhi Cloud tiếng Việt (`vi-VN`), đồng bộ biểu cảm mắt RoboEyes và cử động chân theo trạng thái trò chuyện.
