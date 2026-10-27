# CLAUDE.md - HƯỚNG DẪN KỸ THUẬT & YÊU CẦU DỰ ÁN CHO CLAUDE CODE
## DỰ ÁN: XIAOZHI AI VOICE LIVE ASSISTANT (ESP32-S3 CUSTOM PCB)

Tài liệu này được biên soạn chuyên biệt để cung cấp toàn bộ ngữ cảnh phần cứng, sơ đồ chân, các lưu ý kỹ thuật đã kiểm chứng thực tế và định hướng phát triển tính năng **Lắng nghe thông minh như Siri (Siri-like Always-Listening)** cho Claude Code.

---

## 🎯 1. MỤC TIÊU NHIỆM VỤ CỦA CLAUDE CODE

1. **Tích hợp Voice Live / Trợ lý AI đàm thoại hai chiều**:
   - Nâng cấp firmware robot thành một trợ lý AI thời gian thực (hỗ trợ kết nối WiFi, stream âm thanh WebSocket / OpenAI Realtime / Xiaozhi Cloud AI Server).
2. **Xây dựng cơ chế lắng nghe thông minh thay thế cảm biến khoảng cách**:
   - ⚠️ **LƯU Ý CỐT LÕI**: Trên bo mạch này **HOÀN TOÀN KHÔNG CÓ CẢM BIẾN LASER TOF (VL53L0X)** hay cảm biến chuyển động như các phiên bản robot chó cũ.
   - Claude Code cần xây dựng **cơ chế tự động lắng nghe (Always-Listening / Siri-like Voice Trigger)**:
     - Robot luôn ở trạng thái nền yên lặng, liên tục giám sát âm thanh qua Micro INMP441.
     - Tự động nhận diện khi người dùng cất tiếng nói (VAD - Voice Activity Detection với ngưỡng thích ứng Adaptive Noise Floor) hoặc Wake-Word ("Hi Xiaozhi" / "A lô") để ngay lập tức mở phiên đàm thoại.
     - Tích hợp cơ chế **chống tự kích hoạt (Acoustic Echo Suppression)**: Tự động ngắt mic hoặc tạm dừng VAD trong lúc loa đang phát âm thanh để robot không tự nghe tiếng mình nói rồi tự trả lời.

---

## 🔌 2. SƠ ĐỒ CHÂN PHẦN CỨNG CHUẨN XÁC 100% (EXACT HARDWARE PINOUT)

> ⚠️ **QUY TẮC BẤT DI BẤT DỊCH**: Phần cứng chỉ có duy nhất các linh kiện dưới đây, tuyệt đối không khai báo hay gọi code cho các linh kiện không tồn tại (như Servo, Laser ToF, IMU MPU6050, Màn hình SPI ILI9341).

| Khối chức năng | Tên linh kiện | Tên chân Module | Chân GPIO ESP32-S3 | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- |
| **Màn hình hiển thị** | **OLED SSD1306**<br>(1.3" / 0.96" I2C) | **SDA** | **GPIO 1** | I2C Data (Tần số: 100 kHz) |
| | | **SCL** | **GPIO 2** | I2C Clock (Địa chỉ I2C: `0x3C`) |
| | | **VCC / GND** | **3.3V / GND** | Nguồn màn hình OLED |
| **Khuếch đại âm thanh (Loa)** | **MAX98357A**<br>(I2S Class D 3W) | **BCLK** | **GPIO 11** | Bit Clock I2S (Dùng chung với Mic) |
| | | **LRC / WS** | **GPIO 12** | Word Select I2S (Dùng chung với Mic) |
| | | **DIN** | **GPIO 13** | Audio Data Out từ ESP32 (`DOUT`) |
| | | **SD_MODE** | **3.3V / VCC** | **BẮT BUỘC KÉO LÊN 3.3V QUA JUMPER JP1** |
| | | **VIN / GND** | **5V / GND** | Nguồn công suất loa |
| **Microphone thu âm** | **INMP441**<br>(I2S MEMS Mic) | **SCK / BCLK**| **GPIO 11** | Bit Clock I2S (Dùng chung với Loa) |
| | | **WS** | **GPIO 12** | Word Select I2S (Dùng chung với Loa) |
| | | **SD** | **GPIO 14** | Audio Data In vào ESP32 (`DIN`) |
| | | **L/R** | **GND** | Nối GND để xuất kênh Trái (Left Channel) |
| | | **VDD / GND** | **3.3V / GND** | Nguồn Micro số |
| **Mạch lái động cơ** | **Mini L298N**<br>(Cầu H DC Motor) | **IN1 / IN2** | **GPIO 4 / GPIO 5** | Motor A (Bánh trái) |
| | | **IN3 / IN4** | **GPIO 6 / GPIO 7** | Motor B (Bánh phải) |
| | | **VCC / GND** | **5V - 9V / GND** | Nguồn động cơ (GND chung ESP32) |
| **Nút bấm người dùng** | **User Button** | **KEY / BOOT** | **GPIO 0** | Nút bấm vật lý (Tích cực mức LOW) |
| **Đèn LED RGB** | **WS2812** | **DI** | **GPIO 48** | LED hiển thị trạng thái hệ thống |

---

## ⚠️ 3. BỐN LƯU Ý PHẦN CỨNG QUAN TRỌNG ĐÃ ĐƯỢC XÁC THỰC THỰC TẾ

### 1. Mạch Loa MAX98357A: Phải giữ chân `SD_MODE` ở mức CAO (> 1.4V)
- Trên PCB, chân `SD` có mạch lọc trở 10k tụ 100uF nối xuống GND mà không có trở kéo lên 3.3V $\rightarrow$ Mặc định IC rơi vào Shutdown mode ($V_{SD} < 0.16\text{V}$) gây mất tiếng hoàn toàn.
- **Yêu cầu**: Jumper `JP1` trên PCB đã được nối kín lên 3.3V. Code không cần can thiệp chân SD, chỉ cần xuất I2S chuẩn tại BCLK=11, WS=12, DIN=13.

### 2. Màn hình OLED SSD1306: Thứ tự chân I2C
- Mặt in lụa PCB có thể ghi `SCL: IO1, SDA: IO2`, nhưng kết nối chuẩn xác đã kiểm tra bằng I2C Scan là:
  - `SDA = GPIO 1`, `SCL = GPIO 2`, Địa chỉ `0x3C`, Tần số I2C `100.000 Hz`.
  - Khởi tạo màn hình: `display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)` sau khi gọi `Wire.begin(1, 2, 100000)`.

### 3. Xử lý âm thanh số Micro INMP441 (Bắt buộc dùng DC Blocker & Digital Gain)
- INMP441 xuất dữ liệu 24-bit căn lề trái trong slot 32-bit.
- Khi hàn mạch thủ công, mic thường có độ lệch điện áp DC tĩnh (DC Offset) và độ nhạy giữa các lô hàng chênh lệch nhau.
- **Code xử lý âm thanh chuẩn (Đã test chạy hoàn hảo trong `src/voice_parrot.cpp`)**:
  ```cpp
  // 1. Lấy dữ liệu và tiền khuếch đại số +12dB (dịch 14-bit)
  int32_t sL = rxChunk[i * 2] >> 14;
  int32_t sR = rxChunk[i * 2 + 1] >> 14;
  int32_t rawSample = (abs(sL) > abs(sR)) ? sL : sR;

  // 2. Bộ lọc chặn một chiều DC Blocker (IIR High-Pass 20Hz) - TRIỆT TIÊU DC OFFSET
  static float dcX = 0.0f, dcY = 0.0f;
  float filtered = (float)rawSample - dcX + 0.995f * dcY;
  dcX = (float)rawSample;
  dcY = filtered;
  int16_t cleanSample = (int16_t)constrain((int)filtered, -32767, 32767);
  ```
- **Khuếch đại tự động (Auto-Gain Normalization)**: Tự động đo đỉnh câu nói (`speechPeak`) và nhân hệ số khuếch đại (lên tới $16\times$) trước khi phát ra loa để tiếng luôn to tròn, không bị nhỏ.

### 4. Cấu hình Chip ESP32-S3 Clone trong `platformio.ini`
- Chip trên bo mạch là bản Clone sử dụng Flash SPI chế độ `DIO` và **KHÔNG có chip PSRAM Octal 8-dây**.
- **CẤM** thêm cờ `-DBOARD_HAS_PSRAM` hoặc `memory_type = qio_opi` vì sẽ gây lỗi treo boot `PSRAM ID read error: 0xffffffff`.
- Cấu hình chuẩn trong `platformio.ini`:
  ```ini
  board_build.arduino.memory_type = qio_qspi
  board_build.flash_mode = dio
  board_build.partitions = default.csv
  build_flags =
      -DARDUINO_USB_MODE=1
      -DARDUINO_USB_CDC_ON_BOOT=0
  ```

---

## 🧠 4. THIẾT KẾ CƠ CHẾ LẮNG NGHE THÔNG MINH NHƯ SIRI (SIRI-LIKE LISTENING)

Vì không có cảm biến khoảng cách Laser ToF, robot cần hoạt động theo máy trạng thái âm thanh liên tục (Continuous Audio State Machine):

```
                  ┌──────────────────────────────────────────────┐
                  │              STATE 1: IDLE_WAIT              │
                  │   - LED: Xanh dương                          │
                  │   - Mắt OLED: Chớp mắt tự nhiên              │
                  │   - Bật I2S RX, chạy Adaptive Noise Floor    │
                  └──────────────────────┬───────────────────────┘
                                         │ Tiếng nói vượt ngưỡng:
                                         │ RMS > (noiseFloor + 180)
                                         ▼
                  ┌──────────────────────────────────────────────┐
                  │            STATE 2: LISTENING (VAD)          │
                  │   - LED: Đỏ (hoặc Xanh lơ)                   │
                  │   - Mắt OLED: Mở to chú ý, miệng cử động     │
                  │   - Ghi âm / Stream audio lên Cloud AI       │
                  └──────────────────────┬───────────────────────┘
                                         │ Dừng nói > 550ms:
                                         │ RMS < (noiseFloor + 80)
                                         ▼
                  ┌──────────────────────────────────────────────┐
                  │            STATE 3: THINKING & SPEAKING      │
                  │   - LED: Xanh lá                             │
                  │   - Loa MAX98357A phát âm thanh phản hồi     │
                  │   - Miệng OLED nhấp nhô theo nhịp nói        │
                  │   - Tạm khóa Mic RX chống tự nghe lại tiếng  │
                  └──────────────────────┬───────────────────────┘
                                         │ Phát xong âm thanh + Cooldown 400ms
                                         ▼
                                   Quay lại STATE 1
```

### Các nguyên tắc chống tự kích hoạt (Anti-Self-Triggering):
1. Khi robot đang phát âm thanh qua loa (STATE 3), không kích hoạt VAD.
2. Sau khi phát loa xong, thực hiện đọc xả (drain/flush) sạch các mẫu âm thanh còn lưu trong hàng đợi DMA của I2S (`i2s_read` bỏ đi khoảng 8 chunk) và chờ một khoảng thời gian nghỉ tĩnh (cooldown $\approx 400\text{ms}$) trước khi cho phép VAD kích hoạt trở lại.

---

## 📂 5. FILE THAM KHẢO TỐT NHẤT TRONG DỰ ÁN

- **[src/xiaozhi_robot_combined.cpp](file:///d:/TRIAD%20PROJECT/Projects/esp32s3_xiaozhi_live/src/xiaozhi_robot_combined.cpp)**:  
  *Mã nguồn kết hợp toàn diện nhất (All-in-One) trên nền Arduino PlatformIO:*
  - Nhại giọng VAD tự thích ứng + Pitch-shifting (Tom / Normal / Deep) + Auto-Gain.
  - Mắt RoboEyes chớp nháy tự nhiên + khuôn miệng động theo biên độ tiếng nói trên SSD1306 OLED.
  - Cử động nhích bánh xe báo hiệu đang nghe + nhảy múa theo nhịp câu nói trên động cơ Mini L298N (20kHz siêu âm).
  - Khóa an toàn rơi bàn cao (Desk Safety Lock: giữ nút BOOT > 1.2s).
- **[xiaozhi-esp32/main/boards/esp32s3-companion-robot/](file:///d:/TRIAD%20PROJECT/Projects/esp32s3_weact_xiaozhirobot/xiaozhi-esp32/main/boards/esp32s3-companion-robot)**:  
  *Board profile chính thức trong bộ mã nguồn ESP-IDF Xiaozhi:*
  - Mạng nơ-ron nhận diện giọng nói offline ESP-SR MultiNet với từ khóa: **"Chào Nhi"** (Siri-style voice wake).
  - Cơ chế đàm thoại liên tục (Continuous Listening) với server AI Xiaozhi tiếng Việt (`vi-VN`).
  - 6 MCP Tools tích hợp sẵn cho AI ra lệnh di chuyển động cơ (`self.move_forward`, `self.move_backward`, `self.turn_left`, `self.turn_right`, `self.dance`, `self.stop`).
- **[src/voice_parrot.cpp](file:///d:/TRIAD%20PROJECT/Projects/esp32s3_xiaozhi_live/src/voice_parrot.cpp)**:  
  *Mã nguồn chuẩn mẫu nhại giọng đơn thuần.*
- **[src/test_pcb_all.cpp](file:///d:/TRIAD%20PROJECT/Projects/esp32s3_xiaozhi_live/src/test_pcb_all.cpp)**:  
  Bộ test tự động tuần hoàn toàn diện tất cả các khối ngoại vi (OLED, Loa Mario, Motor DC L298N, VU Meter Micro).

---
*Chúc Claude Code phát triển thành công tính năng đàm thoại AI thông minh mượt mà cho Xiaozhi Desktop Robot!*
