# Firmware chatbot Xiaozhi (ESP-IDF) – board `esp32s3-weact-dog`

Firmware AI chính của robot, chạy trên nền [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
(commit gốc `c0fe42a`). Thư mục này chỉ chứa phần riêng của robot, không chứa toàn bộ upstream.

| Thư mục | Nội dung |
|---|---|
| `esp32s3-weact-dog/` | Mã nguồn board (copy vào `main/boards/esp32s3-weact-dog/`) |
| `patches/` | 3 patch `git format-patch` từ `c0fe42a`: board + đăng ký Kconfig/CMake + docs |
| `docs/` | Kế hoạch, test log, checklist phần cứng của bản port đầu tiên (còn nhắc tới servo, đã bỏ) |
| `build_flash.ps1` | Script build + nạp trên Windows |

## Tính năng

- Chatbot tiếng Việt qua server xiaozhi.me (MQTT + Opus).
- Âm thanh: INMP441 + PCM5100A dùng chung 1 bus I2S full-duplex (BCLK 16, WS 17, DIN 14, DOUT 15), 16 kHz.
- Màn hình ILI9341: mắt RoboEyes với 24 biểu cảm, màu đổi mượt theo cảm xúc, hiệu ứng tim / nước mắt /
  má hồng / Zzz / ? / ! / kính râm / nốt nhạc, phụ đề câu trả lời.
- Gọi robot không cần nút:
  - Wake word **"Chào Nhi"** (MultiNet, phiên âm `chao ni`).
  - **Đưa tay gần cảm biến ToF VL53L0X** (I2C 8/9, XSHUT 21) → vào chế độ nghe.
  - Nút BOOT vẫn dùng được.
- Wi-Fi ổn định hơn: tắt power save, buffer Wi-Fi/LWIP trong PSRAM.
- Servo chân **đã tắt** (chân 7/4/6/5 giữ mức LOW).

Chi tiết cấu hình và bảng cảm xúc: [`esp32s3-weact-dog/README.md`](esp32s3-weact-dog/README.md).

## Build và nạp

Cần **ESP-IDF v6.0.1+**.

```powershell
git clone https://github.com/78/xiaozhi-esp32.git
cd xiaozhi-esp32
git checkout c0fe42a
git am ..\esp32s3_weact_xiaozhirobot\firmware\patches\*.patch
python scripts/build.py esp32s3-weact-dog --language vi-VN
idf.py -p COM8 flash monitor
```

Hoặc dùng `build_flash.ps1` (xem tham số trong file).

Lần đầu khởi động: robot phát Wi-Fi `Xiaozhi-XXXX` → vào `http://192.168.4.1` nhập Wi-Fi →
robot đọc mã 6 số → thêm thiết bị trên xiaozhi.me.

## Giấy phép

Phần mắt là bản port của FluxGarage RoboEyes (GPL-3.0-or-later, `LICENSE-RoboEyes.txt`),
nên firmware build ra là GPL-3.0. Upstream xiaozhi-esp32 là MIT.
