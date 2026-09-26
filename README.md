# Xiaozhi AI Voice Assistant & Desktop Robot 🤖

Dự án trợ lý ảo AI để bàn tương tác giọng nói và cử động cơ khí (Xiaozhi Robot), phát triển trên nền tảng **ESP32-S3 WeAct N16R8** kết hợp màn hình **GMT147SPI IPS**.

---

## 🛠 Phần cứng (Hardware Components)

1. **Bộ xử lý trung tâm**: ESP32-S3 WeAct CoreBoard N16R8 (Bản A, 16MB Flash, 8MB PSRAM OPI).
2. **Màn hình hiển thị**: GMT147SPI (ST7789V3, 172x320 IPS 262K màu).
3. **Micro thu âm**: INMP441 MEMS Microphone (Giao tiếp I2S số).
4. **Khuếch đại âm thanh**: MAX98357A I2S DAC/Amp Class D (3W).
5. **Cảm biến khoảng cách**: VL53L0X / VL53L1X v2 ToF (Nhận diện khoảng cách / vẫy tay đánh thức robot qua I2C).
6. **Cơ cấu chấp hành**: 4x Servo 180° (Cử động đầu Pan/Tilt + cánh tay).
7. **Nguồn điện**:
   - Module hạ áp: WeAct Buck DC/DC (Hạ áp ra 5V 3A).
   - Nguồn khuyên dùng: Pin Li-ion 2S (7.4V - 8.4V) kèm mạch sạc Type-C 2S (IP2326 hoặc TP5100).
   - Nguồn để bàn: Củ sạc Type-C 5V 3A cắm trực tiếp.

---

## 📌 Sơ đồ chân nối (Pinout Mapping)

### 1. Màn hình GMT147SPI (SPI3 / HSPI)
| Chân màn hình | Chân ESP32-S3 | Ghi chú |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Nguồn 3.3V |
| **GND** | GND | Mass chung |
| **SCL** | GPIO 40 | SPI Clock (HSPI) |
| **SDA** | GPIO 41 | MOSI (HSPI) |
| **RES** | GPIO 47 | Reset màn hình |
| **DC**  | GPIO 38 | Data / Command |
| **CS**  | GPIO 39 | Chip Select |
| **BL**  | 3.3V | Nối thẳng 3.3V (không cần software PWM) |

### 2. Âm thanh I2S (Micro & Loa)
| Cụm | Chân module | Chân ESP32-S3 | Ghi chú |
| :--- | :--- | :--- | :--- |
| **INMP441 (Mic)** | SCK | GPIO 15 | I2S0 Bit Clock |
| | WS | GPIO 16 | I2S0 Word Select (LRCK) |
| | SD | GPIO 17 | I2S0 Serial Data In |
| | L/R | GND | Kênh thu bên trái |
| **MAX98357A (Loa)** | BCLK | GPIO 12 | I2S1 Bit Clock |
| | LRC | GPIO 13 | I2S1 Word Select |
| | DIN | GPIO 14 | I2S1 Data Out |
| | GAIN | GND | Độ lợi mặc định 12dB |

### 3. Cảm biến ToF & Servo & Ngoại vi
| Chức năng | Chân ESP32-S3 | Ghi chú |
| :--- | :--- | :--- |
| **ToF SDA** | GPIO 8 | I2C Data |
| **ToF SCL** | GPIO 9 | I2C Clock |
| **ToF XSHUT** | GPIO 21 | Bật/tắt cảm biến |
| **Servo 1** | GPIO 4 | PWM 50Hz (Pan) |
| **Servo 2** | GPIO 5 | PWM 50Hz (Tilt) |
| **Servo 3** | GPIO 6 | PWM 50Hz (Arm Left) |
| **Servo 4** | GPIO 7 | PWM 50Hz (Arm Right) |
| **Status RGB** | GPIO 48 | WS2812 tích hợp trên board |
| **Wake Button** | GPIO 1 | Nút nhấn PTT / Đánh thức |

---

## ⚡ Lưu ý nguồn điện & Chống nhiễu Audio

1. **Dòng tải đỉnh**: 4 Servo khi chuyển động đồng thời có thể ngốn 1.5A - 2.4A. Luôn cấp nguồn 5V từ mạch Buck WeAct (nguồn pin 2S) hoặc Adapter 5V 3A riêng cho servo, **KHÔNG lấy nguồn từ chân 3.3V của ESP32 cấp cho servo**.
2. **Lọc nguồn**: Đặt 1 tụ hóa $470\mu F - 1000\mu F$ ngay tại cổng cấp nguồn 5V của servo để tránh sụt áp gây reset ESP32.
3. **Nối mass hình sao (Star Grounding)**: Dây mass GND từ nguồn chia nhánh riêng: 1 nhánh cho cụm Servo, 1 nhánh cho ESP32 và Audio để khử hiện tượng rè/sôi loa khi servo quay.

---

## 🚀 Hướng dẫn biên dịch & Nạp code

Cài đặt [PlatformIO IDE](https://platformio.org/):

```bash
# Biên dịch firmware
pio run -e esp32s3_xiaozhi

# Nạp firmware vào board qua cổng USB Native
pio run -e esp32s3_xiaozhi -t upload

# Mở serial monitor
pio device monitor -b 115200
```
