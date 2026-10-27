# XIAOZHI DESKTOP ROBOT - ESP32-S3 CUSTOM PCB

Dự án firmware mã nguồn mở cho Robot Trợ Lý Để Bàn Xiaozhi sử dụng vi điều khiển **ESP32-S3**, màn hình OLED I2C, hệ thống âm thanh I2S Full-Duplex (Micro INMP441 + Loa MAX98357A 3W) và động cơ điều khiển bánh xe Mini L298N.

> 📄 **TÀI LIỆU HƯỚNG DẪN CHI TIẾT & LƯU Ý PHẦN CỨNG**:  
> Xem chi tiết tại file [HUONG_DAN_SO_DO_CHAN_VA_LUU_Y.md](file:///d:/TRIAD%20PROJECT/Projects/esp32s3_weact_xiaozhirobot/HUONG_DAN_SO_DO_CHAN_VA_LUU_Y.md).

---

## 🔌 Sơ Đồ Chân Phần Cứng (Pinout)

| Thiết bị | Chân thiết bị | GPIO ESP32-S3 | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- |
| **Màn hình OLED SSD1306** | SDA | **GPIO 1** | Bus I2C 100 kHz (Địa chỉ `0x3C`) |
| | SCL | **GPIO 2** | Bus I2C 100 kHz |
| **Loa MAX98357A (Amp 3W)** | BCLK | **GPIO 11** | I2S Bit Clock (Dùng chung với Mic) |
| | LRC / WS | **GPIO 12** | I2S Word Select (Dùng chung với Mic) |
| | DIN | **GPIO 13** | I2S Data Out từ ESP32 |
| | SD_MODE | **3.3V / VCC** | **BẮT BUỘC ĐÓNG JUMPER JP1** |
| **Micro INMP441 (MEMS)** | SCK / BCLK | **GPIO 11** | I2S Bit Clock (Dùng chung với Loa) |
| | WS | **GPIO 12** | I2S Word Select (Dùng chung với Loa) |
| | SD | **GPIO 14** | I2S Data In vào ESP32 |
| | L/R | **GND** | Kênh trái (Left Channel) |
| **Động cơ Mini L298N** | IN1 / IN2 | **GPIO 4 / GPIO 5** | Motor A (Bánh trái) |
| | IN3 / IN4 | **GPIO 6 / GPIO 7** | Motor B (Bánh phải) |
| **Nút bấm người dùng** | BOOT / KEY | **GPIO 0** | Đổi chế độ giọng nói / Chuyển bài test |
| **Đèn LED RGB** | WS2812 | **GPIO 48** | LED hiển thị trạng thái |

---

## ⚡ Các Điểm Lưu Ý Phần Cứng Trọng Yếu

1. **Loa MAX98357A**: Chân `SD_MODE` có mạch trở 10k tụ 100uF xuống GND khiến IC bị ngắt nguồn (Shutdown) $\rightarrow$ **Bắt buộc phải đóng jumper `JP1`** nối chân `SD` lên nguồn `3.3V/VCC`.
2. **Màn hình OLED**: Ký hiệu in lụa trên mạch có thể là `SCL: IO1, SDA: IO2`, thực tế kết nối chuẩn I2C là `SDA = GPIO 1, SCL = GPIO 2`, địa chỉ `0x3C`.
3. **Micro INMP441**: Tránh để nhựa thông/cồn dính vào lỗ thu âm. Firmware đã tích hợp sẵn **DC Blocker** và **Auto-Gain Normalization (kích âm tự động lên tới 16x)** giúp tiếng loa luôn to rõ dù mic mới hàn có bị nhỏ.
4. **Cấu hình ESP32-S3 Clone**: Dùng flash mode `dio`, memory type `qio_qspi`, không bật cờ `-DBOARD_HAS_PSRAM` trên chip clone để tránh lỗi treo boot.

---

## 🚀 Các Môi Trường Firmware Có Sẵn

- **`[env:voice_parrot]` (Mặc định)**: Firmware Robot Nhại Giọng Thông Minh. Tự động nhận diện tiếng nói (VAD), tự động khuếch đại âm lượng, 3 chế độ đổi giọng bằng nút BOOT (Talking Tom x1.35, Giọng chuẩn x1.0, Robot trầm x0.8), cử động khuôn miệng OLED và đèn trạng thái RGB.
- **`[env:test_pcb_all]`**: Bộ kiểm tra liên hoàn toàn bộ phần cứng mới (Màn hình OLED, Nhạc Mario, Động cơ A/B tiến lùi xoay, Quét tần số + Mic VU Meter, RoboEyes).
- **`[env:test_speaker]`**: Test riêng biệt loa MAX98357A.

---

## 💻 Lệnh Nạp Code Nhanh (PlatformIO CLI)

```bash
# Nạp Robot Nhại Giọng (Voice Parrot)
pio run -e voice_parrot --target upload

# Nạp Bộ Test Toàn Diện Mạch (Test All PCB)
pio run -e test_pcb_all --target upload

# Mở Serial Monitor xem log ở baudrate 115200
pio device monitor -b 115200
```
