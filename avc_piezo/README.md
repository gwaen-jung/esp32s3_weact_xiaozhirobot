# AVC Piezo – mô hình triệt tiêu rung động chủ động

Hồ sơ thi công cho đề tài “Hệ thống triệt tiêu rung động chủ động bằng vật liệu Piezo” (dầm công-xôn Al 6061 300 × 30 × 1,5 mm, ESP32, PPF/DVF 1 kHz).

| Thư mục | Nội dung |
|---|---|
| `web/` | Trang thiết kế + mô phỏng: 3D sản phẩm, biểu đồ thời gian thực, FRF, bản vẽ cơ khí, sơ đồ nguyên lý, BOM, quy trình, rà soát báo cáo. Mở `web/index.html` (cần mạng để tải three.js r128 từ cdnjs). |
| `web/sim-core.js` | Mô hình dầm 3 mode + phần dư tĩnh, bộ điều khiển số giống firmware. Chạy được bằng Node (`require('./sim-core.js')`). |
| `firmware/AVC_Piezo_ESP32/` | Firmware Arduino cho **ESP32 DevKit V1 (WROOM-32)**. ESP32-S3 không có DAC nên không dùng được cho mạch này. |
| `tools/ghi_du_lieu.py` | Ghi CSV 800 Hz, chạy tự động thí nghiệm cộng hưởng và va đập, tính mức giảm và thời gian tắt dần. |

## Những thay đổi chính so với báo cáo

1. Cảm biến điều khiển là đĩa trên T1 (10–37 mm), đồng vị trí với chấp hành. Nếu đặt cảm biến ở 80 mm như báo cáo, DVF làm mode 2 (85 Hz) tự kích.
2. Dầm cắt 350 mm (50 kẹp + 300 tự do); báo cáo ghi 400 mm.
3. Dầm nối GND, hàn dây vào vành đồng từng đĩa (epoxy cách điện). Mỗi đĩa nhận ±13 V.
4. TL074 (thêm đệm Vref), hai TDA2030A cùng lấy tín hiệu từ u₁ với hệ số ±15,7, có Zobel và diode bảo vệ.
5. Mạch đệm cảm biến có tụ chia điện tích, trimmer hệ số và lọc chống chồng phổ 339 Hz.
6. ADXL345 ±16 g, FIFO 800 Hz. Loa nối dầm bằng khớp nam châm tháo nhanh.
7. Firmware tự nhận dạng f₁, H₀ và chiều phân cực; hệ số khuếch đại tính theo tỉ lệ giới hạn ổn định.

Kết quả mô phỏng với bố trí đề xuất: PPF G = 0,6 giảm 98 % biên độ tại cộng hưởng (35 dB), thời gian tắt dần 4,35 s → 0,40 s; DVF Δζ = 0,08 giảm 87 %, 0,53 s. Tổng linh kiện ≈ 1,28 triệu đồng.

Firmware chưa được biên dịch trong môi trường này; hãy biên dịch bằng Arduino IDE (gói esp32 của Espressif, thư viện Adafruit SSD1306 + GFX) trước khi nạp.
