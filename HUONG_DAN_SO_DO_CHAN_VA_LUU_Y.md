# TỔNG HỢP KỸ THUẬT: SƠ ĐỒ CHÂN, HƯỚNG DẪN NẠP & LƯU Ý PHẦN CỨNG
### DỰ ÁN ROBOT XIAOZHI DESKTOP (ESP32-S3 CUSTOM PCB)
*Tài liệu bàn giao đóng gói kỹ thuật - Cập nhật ngày 27/10/2026*

---

## 📌 1. Sơ Đồ Chân Chi Tiết (Hardware Pinout Mapping)

Toàn bộ các chân GPIO trên bo mạch được phân bổ tối ưu, không bị xung đột với các chân Strapping hay chân Flash nội của ESP32-S3:

| Khối linh kiện | Tên linh kiện | Tên chân Module | Chân GPIO ESP32-S3 | Chức năng & Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- |
| **Màn hình hiển thị** | **OLED SSD1306**<br>(1.3" / 0.96" I2C) | **SDA** | **GPIO 1** | Đường truyền dữ liệu I2C (100 kHz) |
| | | **SCL** | **GPIO 2** | Đường truyền xung nhịp I2C Clock |
| | | **VCC / GND** | **3.3V / GND** | Nguồn màn hình (Địa chỉ I2C: `0x3C`) |
| **Khuếch đại âm thanh (Loa)** | **MAX98357A**<br>(I2S Class D 3W) | **BCLK** | **GPIO 11** | Bit Clock I2S (Dùng chung với Mic) |
| | | **LRC / WS** | **GPIO 12** | Word Select / Frame Sync (Dùng chung với Mic) |
| | | **DIN** | **GPIO 13** | Dữ liệu âm thanh từ ESP32 (`DOUT`) |
| | | **SD_MODE** | **3.3V / VCC** | **BẮT BUỘC ĐÓNG JUMPER JP1 (Xem mục 2)** |
| | | **GAIN** | *Để trống / GND* | Để trống = Gain +9dB, nối GND = +12dB |
| | | **VIN / GND** | **5V / GND** | Nguồn công suất cho loa (hoặc 3.3V) |
| **Microphone thu âm** | **INMP441**<br>(I2S MEMS Mic) | **SCK / BCLK**| **GPIO 11** | Bit Clock I2S (Dùng chung với Loa) |
| | | **WS** | **GPIO 12** | Word Select I2S (Dùng chung với Loa) |
| | | **SD** | **GPIO 14** | Dữ liệu âm thanh vào ESP32 (`DIN`) |
| | | **L/R** | **GND** | Chọn kênh trái (Left Channel) |
| | | **VDD / GND** | **3.3V / GND** | Nguồn Micro số |
| **Mạch lái động cơ** | **Mini L298N**<br>(Dual H-Bridge DC) | **IN1 (Motor A)**| **GPIO 4** | Điều khiển Động cơ A (Bánh trái) |
| | | **IN2 (Motor A)**| **GPIO 5** | Điều khiển Động cơ A (Bánh trái) |
| | | **IN3 (Motor B)**| **GPIO 6** | Điều khiển Động cơ B (Bánh phải) |
| | | **IN4 (Motor B)**| **GPIO 7** | Điều khiển Động cơ B (Bánh phải) |
| | | **MOTOR VCC** | **5V - 9V / GND** | Nguồn động cơ (GND chung với ESP32) |
| **Nút bấm người dùng** | **User Button** | **BOOT (KEY)** | **GPIO 0** | Nút bấm tích cực mức THẤP (PULLUP nội) |
| **Đèn LED báo trạng thái** | **WS2812 RGB** | **DI (Data In)** | **GPIO 48** | LED đổi màu trạng thái Onboard |

```
                       ┌─────────────────────────────────────┐
                       │          ESP32-S3 CLONE/DEV         │
                       └─────────────────────────────────────┘
                                   │      │      │
            [I2C BUS]              │      │      │           [I2S FULL-DUPLEX]
      GPIO 1 ────────> SDA (OLED)  │      │      │  GPIO 11 ───────> BCLK (Chung Loa & Mic)
      GPIO 2 ────────> SCL (OLED)  │      │      │  GPIO 12 ───────> WS/LRC (Chung Loa & Mic)
                                   │      │      │  GPIO 13 ───────> DIN  (Loa MAX98357A)
         [MOTOR MINI L298N]        │      │      │  GPIO 14 <─────── SD   (Mic INMP441)
      GPIO 4 ────────> IN1 (M1)    │      │      │
      GPIO 5 ────────> IN2 (M1)    │      │      │             [GIAO DIỆN]
      GPIO 6 ────────> IN3 (M2)    │      │      │  GPIO 0  <─────── Nút BOOT (Đổi mode)
      GPIO 7 ────────> IN4 (M2)    │      │      │  GPIO 48 ───────> LED RGB WS2812
```

---

## ⚠️ 2. Bốn Điểm Kỹ Thuật Cực Kỳ Quan Trọng Cần Lưu Ý

### 🔴 Lưu ý 1: Loa MAX98357A bị câm hoàn toàn (Nguyên nhân do mạch Shutdown)
- **Hiện tượng**: Code nạp thành công, chạy bình thường, I2S báo OK nhưng loa **không hề có bất kỳ tiếng nào**.
- **Nguyên nhân phần cứng**: Trên thiết kế PCB, chân `SD_MODE` của IC MAX98357A được nối qua điện trở 10k và tụ 100uF xuống Mass (GND) mà không có điện trở kéo lên 3.3V. Điện áp đo tại chân `SD` bị giữ ở mức $V_{SD} < 0.16\text{V}$, khiến IC MAX98357A tự động khóa ở chế độ ngắt nguồn hoàn toàn (Shutdown Mode).
- **Cách xử lý**:
  - **Bắt buộc phải hàn chập Jumper `JP1`** trên bo mạch để cấp thẳng điện áp 3.3V vào chân `SD`.
  - Hoặc dùng một sợi dây nhỏ câu chân `SD` của module MAX98357A lên chân `3.3V` / `VCC` ($V_{SD} > 1.4\text{V}$). Loa sẽ lập tức mở và phát to rõ ràng.

### 🔴 Lưu ý 2: Ký hiệu chân màn hình OLED trên mặt in lụa (Silkscreen)
- **Hiện tượng**: Một số phiên bản PCB in lụa là `SCL: IO1, SDA: IO2`. Nếu người lắp chỉ nhìn mặt chữ silkscreen cắm dây có thể bị nhầm.
- **Thực tế phần cứng**: Đường mạch I2C chạy chuẩn xác là:
  - **`SDA` = GPIO 1**
  - **`SCL` = GPIO 2**
  - **Địa chỉ I2C**: `0x3C` (Tần số chuẩn: `100.000 Hz`).
- Code trong dự án (`voice_parrot.cpp` và `test_pcb_all.cpp`) đều đã tích hợp cơ chế thông minh: Tự động thử cả `{1, 2}` lẫn `{2, 1}` để đảm bảo màn hình luôn sáng dù có cắm phiên bản bo mạch nào.

### 🔴 Lưu ý 3: Tình trạng "Mic to Mic nhỏ" và xử lý hàn Mic INMP441
- **Hiện tượng**: Khi thay mic mới hàn vào bo mạch, âm lượng thu được có thể bị bé hơn bình thường.
- **Nguyên nhân vật lý**:
  1. *Dính nhựa thông/Flux vào lỗ thu âm (Acoustic Port)*: Mặt chip INMP441 có một lỗ tròn kim loại siêu nhỏ. Khi hàn mỏ hàn hoặc khò nhiệt, chỉ cần 1 hạt nhựa thông hoặc cồn lau mạch chui vào lỗ này sẽ làm kẹt màng rung MEMS, âm lượng tụt tới 80%. Khi hàn tuyệt đối không khò trực tiếp vào lỗ mic!
  2. *Quá nhiệt hàn*: Màng MEMS chịu nhiệt tối đa $260^\circ\text{C}$ trong vài giây.
- **Giải pháp phần mềm đã được tích hợp sẵn 100% trong code**:
  - **Bộ lọc DC Blocker (High-Pass 20Hz)**: Loại bỏ sạch dòng DC tĩnh do chênh lệch linh kiện.
  - **Tiền khuếch đại số Pre-amp +12dB (`>> 14`)**: Tăng biên độ tín hiệu thu vào gấp 4 lần.
  - **Thuật toán Auto-Gain Normalization**: Tự động đo biên độ đỉnh giọng nói thực tế và **nhân hệ số kích âm tự động lên tới $16\times$** đưa âm thanh lên mức $26.000$ (~$80\%$ công suất loa MAX98357A). Nhờ vậy, **dù mic yếu hay mic nhạy, tiếng loa nhại lại vẫn to, tròn và vang như nhau!**

### 🔴 Lưu ý 4: Cấu hình PlatformIO cho ESP32-S3 Clone (Tránh lỗi PSRAM)
- Các bo mạch ESP32-S3 clone phổ biến trên thị trường thường sử dụng chip Flash chế độ `DIO` và **không có chip PSRAM Octal 8-dây**.
- Nếu cấu hình `-DBOARD_HAS_PSRAM` hoặc `memory_type = qio_opi`, chip sẽ bị treo ngay lúc Reset với lỗi: `PSRAM ID read error: 0xffffffff`.
- Cấu hình chuẩn xác và ổn định tuyệt đối đã được thiết lập sẵn trong file `platformio.ini`:
  ```ini
  board_build.arduino.memory_type = qio_qspi
  board_build.flash_mode = dio
  board_build.partitions = default.csv
  build_flags =
      -DARDUINO_USB_MODE=1
      -DARDUINO_USB_CDC_ON_BOOT=0
  ```

---

## 🚀 3. Danh Sách Firmware & Hướng Dẫn Sử Dụng

Trong thư mục `src/` đã xây dựng sẵn 3 bộ firmware phục vụ các mục đích khác nhau:

### 1️⃣ Firmware Robot Nhại Giọng: `src/voice_parrot.cpp` (Mặc định)
- **Tên môi trường PlatformIO**: `[env:voice_parrot]`
- **Tính năng**:
  - **Tự động nhận diện giọng nói (VAD)**: Cậu chỉ cần nói vào Mic, robot tự động ghi âm vào RAM. Khi dừng nói khoảng 0.55s, robot tự động nhại lại ngay lập tức.
  - **Chống Echo / Vòng lặp phản hồi âm**: Xả sạch bộ đệm âm thanh sau khi phát loa để robot không bao giờ bị tình trạng tự thu lại tiếng loa của chính mình.
  - **3 Chế độ đổi giọng bằng nút BOOT (`GPIO 0`)**:
    1. 🐱 **TALKING TOM** (Mặc định): Tần số nâng cao $1.35\times$ (giọng chú mèo Tom / sóc chuột Chipmunk cực vui nhộn).
    2. 🎙️ **GIỌNG CHUẨN**: Tần số $1.0\times$ (giữ nguyên chất giọng gốc).
    3. 🤖 **ROBOT TRẦM**: Tần số hạ thấp $0.80\times$ (giọng người máy siêu trầm).
  - **Màn hình OLED**: Hiển thị mắt biểu cảm và cử động khuôn miệng há to/nhỏ nhịp nhàng theo câu nói.
  - **Đèn LED RGB (`GPIO 48`)**: Xanh dương (Chờ nghe) $\rightarrow$ Đỏ (Đang ghi âm) $\rightarrow$ Xanh lá (Đang phát loa nhại).

### 2️⃣ Firmware Kiểm Tra Toàn Diện Phần Cứng: `src/test_pcb_all.cpp`
- **Tên môi trường PlatformIO**: `[env:test_pcb_all]`
- **Tính năng**: Chạy chu trình test tự động liên hoàn từ A đến Z:
  1. Quét I2C và bật sáng màn hình OLED SSD1306.
  2. Loa phát giai điệu nhạc Mario cổ điển trong trẻo.
  3. Motor A (Bánh trái) quay thuận 1.5s $\rightarrow$ quay nghịch 1.5s.
  4. Motor B (Bánh phải) quay thuận 1.5s $\rightarrow$ quay nghịch 1.5s.
  5. Cả 2 motor cùng tiến $\rightarrow$ lùi $\rightarrow$ xoay trái $\rightarrow$ xoay phải.
  6. Loa quét tần số (Sweep) từ 350Hz lên 2200Hz kết hợp đọc Micro INMP441 và vẽ thanh sóng VU Meter nhảy theo nhịp âm thanh trên OLED.
  7. Hiển thị mắt robot RoboEyes cử động biểu cảm.

### 3️⃣ Firmware Test Riêng Loa: `src/test_speaker.cpp`
- **Tên môi trường PlatformIO**: `[env:test_speaker]`
- Chuyên dụng để kiểm tra loa MAX98357A, ngắt toàn bộ động cơ để đo kiểm âm thanh độc lập.

---

## 🛠️ 4. Hướng Dẫn Nạp Code Bằng PlatformIO

Dự án được chuẩn hóa 100% theo tiêu chuẩn **PlatformIO IDE** (trên VS Code):

### Cách 1: Nạp trực tiếp trên giao diện VS Code
1. Mở thư mục dự án bằng **VS Code**.
2. Nhìn xuống thanh trạng thái màu xanh dưới cùng, bấm vào biểu tượng hình **Cái Phích Cắm / Tên Môi Trường** để chọn môi trường muốn nạp:
   - `env:voice_parrot` (Robot nhại giọng)
   - `env:test_pcb_all` (Bài test toàn bộ phần cứng)
3. Bấm vào biểu tượng **Mũi tên sang phải (`→` Upload)** để nạp code vào ESP32-S3.
4. Bấm vào biểu tượng **Cái phích cắm (`Serial Monitor`)** để xem nhật ký Serial ở tốc độ baud **`115200`**.

### Cách 2: Nạp nhanh bằng lệnh Terminal / Command Line
Mở Terminal trong thư mục dự án và chạy các lệnh sau:

- **Nạp Robot Nhại Giọng (Voice Parrot)**:
  ```bash
  pio run -e voice_parrot --target upload
  ```
- **Nạp Bộ Test Toàn Diện Mạch (Test All PCB)**:
  ```bash
  pio run -e test_pcb_all --target upload
  ```
- **Nạp chỉ định rõ cổng COM (ví dụ COM16)**:
  ```bash
  pio run -e voice_parrot --target upload --upload-port COM16
  ```
- **Mở Serial Monitor kiểm tra**:
  ```bash
  pio device monitor -b 115200
  ```

---
*Tài liệu được biên soạn phục vụ bàn giao và lưu hành nội bộ dự án Xiaozhi Robot.*
