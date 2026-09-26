# esp32s3_weact_xiaozhirobot

Firmware trợ lý AI để bàn Xiaozhi (Xiaozhi Desktop Robot) phát triển trên nền tảng **ESP32-S3 WeAct CoreBoard N16R8** (16MB Flash, 8MB PSRAM OPI). 

Robot tích hợp giao tiếp giọng nói 2 chiều (Micro INMP441 + Loa MAX98357A), màn hình IPS **GMT147SPI** hiển thị biểu cảm/avatar động, cảm biến khoảng cách **VL53L0X/1X** nhận diện người lại gần/vẫy tay đánh thức, và **4 động cơ Servo 180°** điều khiển cử động đầu Pan/Tilt và cánh tay.

| Env PlatformIO | Board MCU | Vai trò kỹ thuật | Lệnh nạp firmware |
|---|---|---|---|
| `esp32s3_xiaozhi` | WeAct ESP32-S3-A N16R8 | Firmware chính robot Xiaozhi | `pio run -e esp32s3_xiaozhi -t upload` |

```bash
# Biên dịch và nạp firmware
pio run -e esp32s3_xiaozhi -t upload

# Mở serial monitor (115200 baud)
pio device monitor -b 115200
```

---

## 🛠 Danh mục Linh kiện (Hardware BOM)

| Cụm chức năng | Linh kiện | Giao tiếp | Vai trò trong Robot | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- |
| **Xử lý trung tâm** | WeAct ESP32-S3 CoreBoard | — | Chạy FreeRTOS, xử lý Audio/WiFi, điều khiển ngoại vi | Bản A (N16R8: 16MB Flash, 8MB PSRAM OPI) |
| **Màn hình biểu cảm** | GMT147SPI (ST7789V3) | SPI (HSPI) | Hiển thị mắt biểu cảm, cử động miệng, HUD thông số | IPS 172×320 pixel, 262K màu |
| **Micro thu âm** | INMP441 MEMS | I2S (I2S0) | Thu giọng nói người dùng gửi lên server STT/LLM | Micro kỹ thuật số 24-bit, độ nhạy cao |
| **Phát âm thanh** | MAX98357A | I2S (I2S1) | Khuếch đại âm thanh phản hồi từ server TTS | Mạch DAC/Amp Class-D công suất 3W |
| **Cảm biến khoảng cách**| VL53L0X / VL53L1X v2 | I2C | Nhận diện người lại gần, nhận diện cử chỉ vẫy tay | Cảm biến ToF đo khoảng cách bằng laser |
| **Cơ cấu cử động** | 4× Servo 180° | PWM (LEDC) | Cử động đầu (Pan/Tilt) và 2 cánh tay | Tần số 50Hz, góc quay 0° – 180° |
| **Hạ áp nguồn** | WeAct Buck DC/DC | Nguồn xung | Hạ áp từ pin 2S (7.4V) xuống 5.0V 3A cấp cho Servo & ESP32 | Hiệu suất cao (~92%), chịu tải đỉnh 3A |

---

## 🔌 Sơ đồ đấu nối chân WeAct ESP32-S3-A

Vị trí chân thực tế trên board WeAct ESP32-S3 CoreBoard (Bản A):

```
                             ┌────────────────────────┐
                             │   WeAct ESP32-S3-A     │
                             │      (Ăng-ten PCB)     │
                       3V3 ──┤ 3V3                GND ├── GND (Mass chung)
                       3V3 ──┤ 3V3                 5V ├── 5V (Nguồn 5V từ Buck WeAct)
                             │ EN                 TX0 ├── GPIO43 (UART0 TX Debug)
           Servo 1 (Pan) ────┤ GPIO4              RX0 ├── GPIO44 (UART0 RX Debug)
          Servo 2 (Tilt) ────┤ GPIO5            GPIO1 ├── Nút Wake / PTT (Active-Low)
        Servo 3 (Arm L) ─────┤ GPIO6            GPIO2 ├── (Dự phòng ADC1)
        Servo 4 (Arm R) ─────┤ GPIO7           GPIO42 ├── (Dự phòng MISO)
       INMP441 SCK (I2S) ────┤ GPIO15          GPIO41 ├── GMT147 SDA (MOSI)
        INMP441 WS (I2S) ────┤ GPIO16          GPIO40 ├── GMT147 SCL (SCLK)
        INMP441 SD (I2S) ────┤ GPIO17          GPIO39 ├── GMT147 CS
              (Dự phòng) ────┤ GPIO18          GPIO38 ├── GMT147 DC
             ToF SDA (I2C) ──┤ GPIO8           GPIO37 ├── [PSRAM OPI - Cấm dùng]
             ToF SCL (I2C) ──┤ GPIO9           GPIO36 ├── [PSRAM OPI - Cấm dùng]
              (Dự phòng) ────┤ GPIO10          GPIO35 ├── [PSRAM OPI - Cấm dùng]
              (Dự phòng) ────┤ GPIO11           GPIO0 ├── [BOOT - Nút trên board]
      MAX98357A BCLK ────────┤ GPIO12          GPIO45 ├── [Strapping - Cấm dùng]
       MAX98357A LRC ────────┤ GPIO13          GPIO46 ├── [Strapping - Cấm dùng]
       MAX98357A DIN ────────┤ GPIO14           GPIO3 ├── [JTAG - Cấm dùng]
           ToF XSHUT ────────┤ GPIO21          GPIO19 ├── [USB D- Native]
          GMT147 RES ────────┤ GPIO47          GPIO20 ├── [USB D+ Native]
       [WS2812 RGB LED] ─────┤ GPIO48             GND ├── GND
                       GND ──┤ GND                3V3 ├── 3V3 (Cấp cho GMT147/INMP441/ToF)
                             └────────────────────────┘
                                      [USB-C]
```

### Bảng tra cứu chân chi tiết

| Cụm thiết bị | Chân linh kiện | Chân ESP32-S3 | Điện áp | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- |
| **Màn hình GMT147SPI** | VCC | 3V3 | 3.3V | Lấy nguồn 3.3V từ board |
| | GND | GND | 0V | Nối mass chung |
| | SCL | **GPIO 40** | 3.3V | SPI Clock — Bắt buộc dùng bus SPI3 (HSPI) |
| | SDA | **GPIO 41** | 3.3V | MOSI (HSPI) |
| | CS  | **GPIO 39** | 3.3V | Chip Select |
| | DC  | **GPIO 38** | 3.3V | Data / Command |
| | RES | **GPIO 47** | 3.3V | Reset cứng màn hình |
| | BL  | 3V3 | 3.3V | **Nối thẳng 3.3V**, không nối GPIO48 để tránh đụng LED RGB |
| **Micro INMP441** | VDD | 3V3 | 3.3V | Không cấp 5V |
| | GND | GND | 0V | |
| | SCK | **GPIO 15** | 3.3V | I2S0 Bit Clock |
| | WS  | **GPIO 16** | 3.3V | I2S0 Word Select (LRCK) |
| | SD  | **GPIO 17** | 3.3V | I2S0 Serial Data In |
| | L/R | GND | 0V | Kéo xuống GND để thu kênh trái |
| **Loa MAX98357A** | VIN | **5V** | 5.0V | **Bắt buộc cấp 5V từ Buck** để đạt công suất 3W |
| | GND | GND | 0V | |
| | BCLK | **GPIO 12** | 3.3V | I2S1 Bit Clock |
| | LRC  | **GPIO 13** | 3.3V | I2S1 Word Select |
| | DIN  | **GPIO 14** | 3.3V | I2S1 Data Out |
| | GAIN | GND | 0V | Mặc định 12dB (hoặc 100k lên GND để chọn 9dB) |
| **ToF VL53L0X/1X** | VIN | 3V3 | 3.3V | Cảm biến chạy 3.3V |
| | GND | GND | 0V | |
| | SDA | **GPIO 8** | 3.3V | I2C Data |
| | SCL | **GPIO 9** | 3.3V | I2C Clock |
| | XSHUT | **GPIO 21** | 3.3V | Bật/tắt cảm biến bằng phần mềm |
| **4× Servo 180°** | VCC (Đỏ) | **5V Buck** | 5.0V | **Tuyệt đối không lấy từ 3.3V của ESP32** |
| | GND (Nâu/Đen) | GND | 0V | Nối mass chung về trạm nguồn |
| | Signal (Vàng/Cam) | **GPIO 4, 5, 6, 7** | 3.3V | Điều khiển xung PWM (LEDC channel 0..3, 50Hz) |
| **Nút bấm / Status** | Wake Button | **GPIO 1** | 3.3V | Nút nhấn kéo xuống GND khi nhấn (Internal Pull-up) |
| | RGB LED | **GPIO 48** | 3.3V | Đèn WS2812 tích hợp sẵn trên board WeAct |

---

## ⚡ Kiến trúc Nguồn & Chống nhiễu Audio

1. **Dòng tải đỉnh (Peak Current)**:
   - ESP32-S3 phát WiFi + WebSocket: ~350mA.
   - Loa MAX98357A: ~500mA đỉnh ở 5V.
   - 4 Servo chuyển động đồng thời: Mỗi servo ăn dòng đỉnh 300mA – 600mA ➔ **Tổng 4 servo đỉnh từ 1.5A đến 2.4A!**
   - **Tổng dòng đỉnh toàn hệ thống: ~2.5A – 3.0A ở 5V.**
   - *Khuyến cáo*: Cấp nguồn bằng pin Li-ion 2S (7.4V – 8.4V) qua mạch **Buck WeAct (hạ áp ra 5V 3A)** kèm mạch sạc 2S cổng Type-C (IP2326 hoặc TP5100). Khi đang code để bàn, dùng củ sạc Type-C 5V 3A cắm trực tiếp.

2. **Chống nhiễu âm thanh (Star Grounding & Tụ lọc)**:
   - Động cơ chổi than trong servo khi quay sẽ sinh ra tia lửa điện và xung gai dội ngược về đường mass, gây tiếng rè/sôi loa hoặc lỗi micro.
   - **Tụ bù sụt áp**: Hàn song song **1 tụ hóa 470µF – 1000µF (10V/16V)** ngay tại trạm cấp nguồn 5V của 4 servo.
   - **Nối mass hình sao (Star Grounding)**: Dây mass GND từ ngõ ra của module Buck WeAct phải chia thành 2 nhánh độc lập:
     - *Nhánh 1*: Chạy thẳng về chân GND của 4 Servo.
     - *Nhánh 2*: Chạy về chân GND của ESP32, màn hình, Micro và Loa.
     - *Không đi dây GND nối tiếp (daisy-chain) qua cụm servo rồi mới về micro/loa*.

---

## ⚠️ Lưu ý Phần cứng ESP32-S3 WeAct (Gotchas)

- **Chân cấm sử dụng**:
  - `GPIO 0, 3, 45, 46`: Chân cấu hình khởi động (Strapping pins).
  - `GPIO 19, 20`: Kênh USB D- / D+ Native (dùng để nạp firmware và in Serial CDC).
  - `GPIO 35, 36, 37`: Kết nối trực tiếp với chip PSRAM OPI 8MB bên trong module. Không được cấu hình làm GPIO thường.
- **Cờ biên dịch màn hình**: Màn hình GMT147SPI cắm ở GPIO 40/41 bắt buộc phải định nghĩa cờ `-DUSE_HSPI_PORT=1` trong `platformio.ini` để thư viện `TFT_eSPI` sử dụng bộ điều khiển SPI3 (HSPI), tránh lỗi crash loop `StoreProhibited`.
- **Lỗi font TFT_eSPI**: Font 6 chỉ chứa số (0–9), không hiển thị được chữ cái. Để hiển thị chữ to cần dùng Font 4 với `tft.setTextSize(2)`.

---

## 🗺 Lộ trình Phát triển (Development Roadmap)

- [x] **Phase 1: Display & UI Engine**: Khởi tạo màn hình GMT147SPI, render ReShape Logo animation (Sprite 1bpp chống giật, ~35 FPS), thanh HUD stats và dải Marquee chạy chữ.
- [ ] **Phase 2: I2S Audio Bring-up**: Kiểm tra thu âm micro INMP441 và phát âm thanh qua MAX98357A.
- [ ] **Phase 3: Sensor & Servo Integration**: Đọc khoảng cách từ VL53L0X/1X, tạo thuật toán điều khiển 4 servo biểu cảm mượt mà (chuyển động gia tốc S-curve).
- [ ] **Phase 4: Xiaozhi Cloud AI Protocol**: Tích hợp giao thức WebSocket / MQTT kết nối server Xiaozhi (STT, LLM, TTS Opus streaming).
- [ ] **Phase 5: Emotional Facial Expressions**: Thiết kế bộ biểu cảm động (chớp mắt, lắng nghe, suy nghĩ, nói chuyện) khớp với trạng thái AI.

---

Hệ sinh thái liên quan: [DRONE_TEST](https://github.com/gwaen-jung/DRONE_TEST) | [JS-CONTROLER](https://github.com/gwaen-jung/JS-CONTROLER) | [GCS-STATION](https://github.com/gwaen-jung/GCS-STATION) | [QUAD-UAV](https://github.com/gwaen-jung/QUAD-UAV).
