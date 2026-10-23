/* Nội dung dạng bảng: BOM, giá trị linh kiện, quy trình, rà soát */
(function () {
  const $ = id => document.getElementById(id);
  const vnd = n => n.toLocaleString('vi-VN');

  const BOM = [
    ['Điều khiển – cảm biến – hiển thị'],
    ['MCU', 'ESP32 DevKit V1 (ESP32-WROOM-32, 30 chân, USB CP2102/CH340)', 1, 120000, 'Bắt buộc ESP32 đời đầu có DAC. ESP32-S3/C3 không có DAC.'],
    ['ACC', 'Gia tốc kế ADXL345, module GY-291 (I²C 0x53)', 1, 45000, 'Gắn tại 290 mm, đặt ±16 g, FIFO 800 Hz'],
    ['DISP', 'OLED 0,96" SSD1306 128×64 I²C (0x3C)', 1, 60000, 'Mặt trước hộp mạch'],
    ['T1, T2, B1, B2', 'Đĩa piezo Ø27 mm, gốm Ø20 mm, không vỏ, có sẵn dây hoặc trơn', 6, 5000, '4 dùng + 2 dự phòng. Đo điện dung, chọn 4 đĩa lệch < 10 %'],
    ['PZ_M', 'Đĩa piezo Ø20 mm', 1, 4000, 'Cảm biến giám sát tại 80 mm (tùy chọn)'],
    ['Mạch tương tự'],
    ['U3', 'TL074CN DIP-14 (4 op-amp JFET) + đế 14 chân', 1, 15000, 'Thay TL072: thêm tầng đệm Vref và kênh giám sát'],
    ['U4, U5', 'TDA2030A Pentawatt-5 (ST hoặc UTC)', 2, 20000, 'Mắc cầu BTL, hệ số ≈ 15,7'],
    ['HS1, HS2', 'Tản nhiệt nhôm 25×23×16 + đệm mica + ống cách điện + vít M3', 2, 8000, 'Đế IC nối −15 V, phải cách điện'],
    ['R1…R31', 'Điện trở ¼ W 1 % theo bảng giá trị (≈ 32 con)', 1, 25000, 'R16, R26 loại ½ W'],
    ['C1…C30', 'Bộ tụ theo bảng giá trị (film, gốm, điện phân)', 1, 40000, 'C1, C2, C17, C18 dùng tụ film'],
    ['D1…D8', '1N4001 ×4, BAT54S ×2 (kèm đế chuyển SOT-23), SS34 ×1', 1, 20000, 'Bảo vệ ngõ ra TDA, kẹp ADC, chống ngược cực'],
    ['VR1', 'Trimmer 3296W 50 kΩ', 1, 5000, 'Chỉnh hệ số khuếch đại cảm biến'],
    ['VR2', 'Biến trở 10 kΩ tuyến tính (B) + núm', 1, 8000, 'Chỉnh tần số loa'],
    ['Nguồn'],
    ['PSU', 'Adapter 24 V – 2 A, jack 5,5 × 2,1 mm', 1, 90000, ''],
    ['U2', 'DC-DC cách ly 24 V → ±15 V, ≥ 6 W (Mornsun WRA2415S-6WR2 hoặc tương đương)', 1, 150000, 'Dòng tĩnh 2 TDA ≈ 80–120 mA mỗi nhánh'],
    ['U1', 'Module hạ áp LM2596, chỉnh ra 5,0 V', 1, 25000, 'Cấp ESP32 và PAM8403'],
    ['J1, SW1, F1', 'Jack DC cái, công tắc bập bênh, cầu chì ống 1 A + đế', 1, 20000, ''],
    ['Kích rung và va đập'],
    ['SPK', 'Loa 4 Ω – 5 W, Ø66 mm', 1, 45000, 'Đặt dưới dầm tại 120 mm'],
    ['AMP', 'Module PAM8403 có núm âm lượng, 5 V', 1, 25000, 'Ngõ ra BTL: không nối L− xuống GND'],
    ['SW2, J4', 'Công tắc gạt 3 chân + jack 3,5 mm cái', 1, 10000, 'Chọn nguồn sin: ESP32 hoặc điện thoại/máy phát'],
    ['MAG', 'Nam châm NdFeB Ø8 × 3 mm ×2, long đen thép M4, que carbon Ø3 × 100 mm', 1, 20000, 'Khớp nối que đẩy tháo nhanh'],
    ['BALL', 'Bi thép Ø6 và Ø10, ống mica Ø12/Ø8 dài 100 mm, kẹp ống', 1, 15000, 'Thí nghiệm va đập lặp lại'],
    ['Cơ khí'],
    ['BEAM', 'Thanh nhôm 6061, 30 × 1,5 mm, cắt dài 350 mm', 2, 40000, '50 kẹp + 300 tự do; 1 thanh dự phòng'],
    ['BASE', 'Ván ép / MDF 18 mm, 450 × 200 mm + 4 chân cao su', 1, 60000, ''],
    ['POST', 'Trụ ngàm nhôm 50×50×80, má kẹp thép 50×50×10, 2 bu lông M6×30, 2 vít M6×25 đầu chìm', 1, 90000, 'Gia công tại xưởng, mặt kẹp phay phẳng'],
    ['BRK', 'Giá đỡ loa mica 5 mm (hoặc in 3D) + giá ống dẫn bi', 1, 30000, ''],
    ['BOX', 'Hộp nhựa 150 × 100 × 60 mm', 1, 40000, 'Khoét OLED, 3 nút, VR2, SW2, J4'],
    ['Vật tư lắp ráp'],
    ['—', 'Keo epoxy AB cường độ cao loại đông cứng 30 phút', 1, 30000, 'Cứng hơn loại 5 phút, truyền lực tốt hơn'],
    ['PCB', 'Board lỗ 7 × 9 cm ×2, header cái, terminal 5,08 mm', 1, 40000, 'Board 1: nguồn + analog; board 2: ESP32 + giao diện'],
    ['—', 'Cáp chống nhiễu 2 lõi 1 m; dây silicone 30 AWG 4 màu 2 m; dây 22 AWG', 1, 35000, 'Dây mềm dọc dầm để không đổi dao động'],
    ['S1–S3', 'Nút nhấn 12 mm ×3, ống co nhiệt, cos vòng M6', 1, 25000, 'Cos vòng nối đất dầm dưới bu lông ngàm'],
    ['—', 'Giấy nhám P400, cồn IPA, băng keo Kapton', 1, 20000, '']
  ];
  let total = 0, idx = 0;
  const rows = BOM.map(r => {
    if (r.length === 1) return `<tr class="grp"><td colspan="7">${r[0]}</td></tr>`;
    const [ref, name, q, p, note] = r; const s = q * p; total += s; idx++;
    return `<tr><td class="n">${idx}</td><td class="mono">${ref}</td><td>${name}</td><td class="n">${q}</td><td class="n">${vnd(p)}</td><td class="n">${vnd(s)}</td><td class="mutedtxt">${note}</td></tr>`;
  });
  rows.push(`<tr class="total"><td></td><td></td><td>TỔNG CỘNG</td><td></td><td></td><td class="n">${vnd(total)}</td><td class="mutedtxt">Mục tiêu &lt; 1.500.000 đ: đạt</td></tr>`);
  $('tblBom').tBodies[0].innerHTML = rows.join('');
  $('bomTotalTop').textContent = vnd(total) + ' đ';

  const R = [
    ['R1, R21', '10 MΩ', 'Phân cực cảm biến về Vref; cùng C_p+C1 tạo HPF < 1 Hz'],
    ['R2, R22', '10 kΩ', 'Hạn dòng vào chân (+) TL074'],
    ['R3', '10 kΩ', 'U3A, chân (−) về Vref'],
    ['VR1', '50 kΩ trimmer', 'U3A hồi tiếp: hệ số 1 … 6'],
    ['R23, R24', '10 kΩ, 10 kΩ', 'U3D hệ số 2'],
    ['R5, R25', '4,7 kΩ', 'Lọc chống chồng phổ với C3/C7 (339 Hz), hạn dòng kẹp'],
    ['R6, R7', '10 kΩ, 10 kΩ', 'Chia áp 3V3 → 1,65 V'],
    ['R8', '100 Ω', 'Cách ly tải dung cho U3B'],
    ['R9, R11', '20 kΩ', 'Vi sai U3C: ngõ vào'],
    ['R10, R12', '11 kΩ', 'Vi sai U3C: hệ số 0,55'],
    ['R13', '100 kΩ', 'U4 chân 1 về GND'],
    ['R14, R19', '22 kΩ', 'Hồi tiếp TDA2030A'],
    ['R15', '1,5 kΩ', 'U4: hệ số 1 + 22/1,5 = 15,7'],
    ['R18', '1,40 kΩ', 'U5: hệ số −22/1,40 = −15,7'],
    ['R20', '1,3 kΩ', 'U5 chân 1 về GND (cân bằng dòng phân cực)'],
    ['R16, R26', '1 Ω ½ W', 'Mạng Zobel'],
    ['R17, R27', '10 Ω', 'Nối tiếp ngõ ra, ổn định với tải dung'],
    ['R28, R29', '100 kΩ', 'Xả điện tích piezo khi tắt nguồn'],
    ['R30, R31', '22 kΩ, 4,7 kΩ', 'Chia tín hiệu DAC2 vào PAM8403']
  ];
  $('tblR').innerHTML = R.map(r => `<tr><td class="mono">${r[0]}</td><td class="mono">${r[1]}</td><td>${r[2]}</td></tr>`).join('');
  const C = [
    ['C1', '100 nF film (JP1: 0 / 47 / 100 nF)', 'Chia điện tích cảm biến T1, tránh bão hòa'],
    ['C2', '47 nF film', 'Chia điện tích cảm biến giám sát'],
    ['C3, C7', '100 nF', 'Lọc chống chồng phổ trước ADC'],
    ['C4', '10 µF', 'Lọc Vref'], ['C5', '1 µF', 'Sau R8, ngõ VREF_BUF'],
    ['C6', '4,7 nF', 'Song song R12, làm mượt bậc DAC (fc ≈ 3 kHz)'],
    ['C10', '470 µF / 35 V', 'Ngõ vào 24 V'],
    ['C11, C12, C20–C23', '100 µF / 25 V', 'Lọc ±15 V tại board và tại chân TDA'],
    ['C13, C14, C24–C27', '100 nF gốm', 'Thoát cao tần sát chân IC'],
    ['C17, C18', '220 nF film', 'Mạng Zobel TDA2030A'],
    ['C30', '10 µF', 'Tách DC tín hiệu loa'],
    ['D1', 'SS34', 'Chống ngược cực 24 V'],
    ['D3, D4', 'BAT54S', 'Kẹp ADC về 0…3,3 V'],
    ['D5–D8', '1N4001', 'Bảo vệ ngõ ra TDA về hai đường nguồn'],
    ['JP1', 'Jumper 3 vị trí', 'Chọn C1'],
    ['S1–S3', 'Nút 12 mm', 'GPIO32 / 33 / 27 → GND']
  ];
  $('tblC').innerHTML = C.map(r => `<tr><td class="mono">${r[0]}</td><td class="mono">${r[1]}</td><td>${r[2]}</td></tr>`).join('');

  const PH = [
    ['Gia công cơ khí', 'Tuần 8 · 2 buổi', [
      'Cắt thanh nhôm dài 350 mm (50 mm kẹp + 300 mm tự do), mài phẳng và vê mép. Đánh dấu mép ngàm x = 0 bằng bút kim.',
      'Trụ ngàm 50×50×80: phay phẳng mặt trên, ta-rô 2 lỗ M6 sâu 15 mm cách nhau 40 mm (đối xứng hai bên dầm), mặt dưới ta-rô 2 lỗ M6 bắt đế. Má kẹp thép 50×50×10 khoan 2 lỗ Ø6,5.',
      'Đế 450×200: khoan 2 lỗ Ø6,5 khoét chìm bắt trụ (tâm trụ cách mép trái 45 mm, cách mép trước 70 mm); khoan lỗ bắt giá loa tại tâm x = 120 mm; lỗ bắt giá ống bi tại x = 260 mm; dán 4 chân cao su.',
      'Giá loa: mica 5 mm uốn chữ L hoặc in 3D, giữ mặt loa song song đáy dầm, mặt màng cách đáy dầm ≈ 25 mm.'
    ]],
    ['Chuẩn bị piezo', 'Tuần 8 · 1 buổi', [
      'Đo điện dung 6 đĩa Ø27 bằng đồng hồ (thường 15–30 nF), chọn 4 đĩa lệch nhau < 10 %. Ghi số hiệu T1, T2, B1, B2 bằng bút lên vành đồng.',
      'Hàn dây silicone 30 AWG vào mặt gốm (bạc) ở mép ngoài, mỏ 300 °C, chạm ≤ 2 s. Hàn dây thứ hai vào vành đồng thau lộ ra (ngoài vùng gốm) để nối GND. Keo epoxy cách điện, nên không thể dùng thân dầm làm điện cực như báo cáo.',
      'Kiểm tra: gõ nhẹ đĩa đặt trên bàn, đồng hồ mV phải nhảy; điện trở gốm–đồng > 20 MΩ.'
    ]],
    ['Dán piezo lên dầm', 'Tuần 9 · 2 ngày chờ keo', [
      'Đánh nhám P400 vùng x = 0…95 mm cả hai mặt dầm, lau IPA, không chạm tay sau khi lau.',
      'In mẫu giấy 1:1 có vòng tròn tâm 23,5 và 51 mm, dán tạm dưới dầm để căn vị trí. Dán mặt dưới trước (B1, B2), lật dầm đặt lên tấm xốp phẳng, đặt quả nặng 1–2 kg qua miếng mút lên từng đĩa, chờ 24 h.',
      'Dán mặt trên T1 (tâm 23,5 mm, cảm biến) và T2 (tâm 51 mm) thẳng hàng với B1, B2. Lớp keo càng mỏng càng tốt, không để keo tràn lên mặt gốm. Chờ 24 h.',
      'Kiểm tra lại điện dung từng đĩa (không đổi quá 15 %) và cách điện gốm–dầm > 20 MΩ. Nếu đĩa nứt, điện dung tụt mạnh: thay bằng đĩa dự phòng.'
    ]],
    ['Gắn cảm biến và phụ kiện', 'Tuần 9 · 1 buổi', [
      'Cảm biến giám sát Ø20 tại tâm 80 mm mặt trên (tùy chọn).',
      'ADXL345 tại tâm 290 mm mặt trên, trục Z vuông góc mặt dầm, dán băng keo 2 mặt mỏng hoặc keo 502. Dây 4 sợi 30 AWG xoắn nhẹ, dán Kapton dọc dầm mỗi 50 mm về phía ngàm, để võng 3–4 cm tại ngàm.',
      'Nam châm Ø8×3 dán epoxy mặt dưới tại tâm 120 mm. Đầu que carbon dán long đen thép M4 và nam châm thứ hai: khớp hút tháo được bằng tay.',
      'Đánh dấu điểm va đập 260 mm mặt trên.'
    ]],
    ['Lắp mạch điện tử', 'Tuần 10 · 3 buổi', [
      'Board 1 (nguồn + analog): hàn theo thứ tự jack, cầu chì, SS34, C10 → DC-DC ±15 V → cấp nguồn thử, đo ±15 V. Sau đó hàn đế TL074, mạch Vref, đệm cảm biến, vi sai; cuối cùng 2 TDA2030A trên tản nhiệt có mica.',
      'Thử từng TDA với tải giả 47 nF trước khi nối piezo: cấp sin 1 V từ máy phát vào u₁, ngõ ra phải sạch, không có dao động cao tần trên dao động ký.',
      'Board 2: ESP32 trên header cái, LM2596 chỉnh 5,0 V trước khi cắm ESP32, PAM8403, VR2, nút, OLED.',
      'Đi dây: nguồn công suất (TDA) và tín hiệu tách riêng; nối mass hình sao tại một terminal trên board 1, từ đó một dây về cos vòng ở bu lông ngàm (dầm = GND). Cáp cảm biến T1 dùng dây chống nhiễu, vỏ nối GND một đầu tại board.'
    ]],
    ['Lắp ráp tổng thể', 'Tuần 10 · 1 buổi', [
      'Đặt dầm lên trụ, mép ngàm thẳng mép trụ (x = 0), đặt má kẹp, siết 2 bu lông M6 đều tay đến 8–10 N·m. Cos vòng GND nằm dưới một đầu bu lông.',
      'Bắt giá loa, chỉnh chiều cao để khớp nam châm hút vừa chạm đáy dầm khi dầm thẳng, không đẩy dầm lệch.',
      'Bắt hộp mạch phía sau, đi dây piezo: T2 → OUT1; B1 // B2 → OUT2; T1 → ngõ vào cảm biến; mọi dây vành đồng → GND.',
      'Kiểm tra tự do: gõ đầu dầm, dao động phải kéo dài vài giây (khi đã tháo que đẩy).'
    ]],
    ['Nạp firmware, hiệu chỉnh', 'Tuần 11–12', [
      'Cài Arduino IDE, gói board esp32 (Espressif), thư viện Adafruit SSD1306 và Adafruit GFX. Chọn board “ESP32 Dev Module”, nạp AVC_Piezo_ESP32.ino.',
      'Thực hiện danh sách hiệu chỉnh ở mục 08.'
    ]],
    ['Thí nghiệm và báo cáo', 'Tuần 13–16', [
      'Thực hiện thí nghiệm ở mục 08, mỗi chế độ 5 lần, điền Bảng 5.3 và 5.4, chèn ảnh ket_qua_do.png.'
    ]]
  ];
  $('phases').innerHTML = PH.map((p, i) => `<div class="phase"><div class="ph">${i + 1}. ${p[0]}<small>${p[1]}</small></div><ol class="steps">${p[2].map(s => `<li>${s}</li>`).join('')}</ol></div>`).join('');

  $('calib').innerHTML = [
    'Chưa cắm ESP32, chưa nối piezo: cấp 24 V, đo +5,0 V, ±15 V, VREF_BUF = 1,65 V. Dòng 24 V khi không tải khoảng 0,15–0,25 A.',
    'Cắm ESP32, nối piezo. Mở Serial Plotter 921600. Gửi <code>t</code>: dao động ký đo OUT1 − OUT2 phải là sin 13 Hz ≈ 52 V đỉnh-đỉnh, không méo. Nếu méo ở đỉnh: kiểm tra nguồn ±15 V tụt.',
    'Chỉnh độ nhạy cảm biến: kéo đầu dầm lệch 5 mm rồi thả. Cột y trên Serial phải dao động trong ±1,2 V. Lớn hơn thì cắm JP1 sang 100 nF hoặc vặn VR1 giảm; nhỏ hơn 0,2 V thì tăng VR1.',
    'Tháo que đẩy khỏi dầm. Giữ S3 GAIN 1,5 s → OLED “Go nhe dau dam” → búng nhẹ đầu dầm (biên độ 2–3 mm). Firmware đo f₁ từ số lần qua 0.',
    'Bước 2 tự chạy: phát sin 3 Hz, đo H₀ và chiều phân cực, lưu vào NVS. Kết quả mong đợi: f₁ = 12–14 Hz (thấp hơn 13,61 Hz do khối lượng ADXL ≈ 2 g ở đầu dầm), H₀ trong khoảng 0,003–0,03.',
    'Kiểm tra: bật PPF mức 1, búng dầm → tắt dần nhanh hơn rõ rệt. Nếu rung tăng hoặc kêu rè: tắt ngay, chạy lại nhận dạng; kiểm tra dây T2/B1/B2 có cùng chiều đĩa.',
    'Lắp lại que đẩy, bật loa (giữ S2), vặn VR2 tìm đỉnh cộng hưởng (biên độ lớn nhất). Ghi f₁ có que đẩy, gửi <code>p&lt;f&gt;</code> nếu khác f₁ đã đo quá 3 %.',
    'Chỉnh núm PAM8403 sao cho biên độ đầu dầm khi OFF khoảng 1,5–2,5 mm. Theo ngân sách lực, piezo triệt hết dao động đến khoảng 3 mm; lớn hơn thì bộ điều khiển bão hòa và mức giảm thấp hơn mô phỏng.'
  ].map(s => `<li>${s}</li>`).join('');
  $('exper').innerHTML = [
    '<b>Cộng hưởng (Bảng 5.3):</b> chạy <code>python ghi_du_lieu.py COM5 cong_huong 13.4 ppf</code>. Script tự bật loa, chờ 10 s, ghi 10 s OFF, bật điều khiển, ghi 10 s ON, in RMS và mức giảm (%, dB). Lặp 5 lần, rồi làm lại với <code>dvf</code>.',
    '<b>Va đập (Bảng 5.4):</b> tháo que đẩy. Đặt ống dẫn bi thẳng đứng trên điểm 260 mm, đáy ống cao 30 mm. Chạy <code>python ghi_du_lieu.py COM5 va_dap ppf</code>, thả bi Ø6 khi được nhắc, lần OFF rồi lần ON. Script tính thời gian đường bao còn 5 % và tỉ số OFF/ON.',
    '<b>Bi Ø10 thả 5 cm (đúng báo cáo):</b> dùng để minh họa giới hạn. Biên độ ban đầu ≈ 6–7 mm, vượt khả năng lực của piezo nên đoạn đầu tắt dần gần như tuyến tính (bão hòa), sau đó mới tắt nhanh.',
    '<b>Khảo sát hệ số (Bảng 5.2):</b> lặp thí nghiệm cộng hưởng với g0, g1, g2. Không thử vượt mức 3 trên mô hình thật.',
    '<b>Tiêu chí đạt:</b> trung bình mức giảm ≥ 50 % (≥ 6 dB) và tỉ số tắt dần OFF/ON ≥ 3. Báo cáo cả độ lệch chuẩn của 5 lần đo.',
    '<b>So sánh mô phỏng:</b> bảng ở mục 02 là giá trị dự đoán. Sai khác chính đến từ hệ số lực của đĩa buzzer (ước lượng ±50 %), lớp keo và khối lượng dây.'
  ].map(s => `<li>${s}</li>`).join('');

  const IS = [
    ['s1', 'Nghiêm trọng', 'Cảm biến điều khiển đặt tại 80 mm không đồng vị trí với cụm chấp hành 10–64 mm.',
      'Ở mode 2, độ dốc dầm trên đoạn 70–90 mm ngược dấu với đoạn 10–64 mm (tích hệ số chấp hành × cảm biến âm). Mô phỏng: DVF Δζ = 0,05 làm đỉnh mode 2 tăng 20 dB, Δζ ≥ 0,08 làm mode 2 (≈ 85 Hz) tự kích, không còn giảm rung. Kết luận “DVF luôn ổn định, giảm mode 2 thêm 22,7 dB” trong báo cáo chỉ đúng khi cảm biến thực sự đồng vị trí.',
      'Dùng đĩa trên T1 (10–37 mm) làm cảm biến; T2, B1, B2 làm chấp hành. Mô phỏng: G<sub>max</sub> PPF ≈ 0,72 (báo cáo 0,78), DVF ổn định và giảm mode 2 ≈ 7 dB.'],
    ['s1', 'Nghiêm trọng', 'Kích thước dầm không khớp: dầm 400 mm, kẹp 50 mm nhưng phần tự do ghi 300 mm.',
      '400 − 50 = 350 mm tự do thì f₁ ≈ 10,0 Hz, f₂ ≈ 62,7 Hz, không còn khớp các bảng tính và vùng đánh nhám 50–140 mm.',
      'Cắt dầm 350 mm (50 kẹp + 300 tự do).'],
    ['s1', 'Nghiêm trọng', 'Đế đồng thau dán epoxy được coi là tiếp xúc điện với dầm.',
      'Keo epoxy cách điện nên tiếp xúc này không chắc chắn. Nếu dầm thả nổi, cảm biến mất điểm tham chiếu và nhiễu nặng, dầm mang điện áp. Thêm nữa, mỗi đĩa chỉ nhận ±13 V (nửa cầu), không phải ±26 V.',
      'Hàn dây vào vành đồng từng đĩa, nối dầm về GND tại bu lông ngàm. T2 nối OUT1, B1 và B2 nối OUT2 (ngược pha). Lực chấp hành tính theo ±13 V mỗi đĩa.'],
    ['s2', 'Lớn', 'Vref 1,65 V từ cầu chia 10 kΩ/10 kΩ không được đệm nhưng lại nối vào tầng vi sai.',
      'Điện trở tầng vi sai kéo lệch Vref và làm Vref dao động theo tín hiệu, tạo hồi tiếp ký sinh giữa cảm biến và chấp hành.',
      'TL074: U3B đệm Vref (R8 100 Ω + C5 1 µF).'],
    ['s2', 'Lớn', 'TDA2030A #2 mắc đảo hệ số −1 lấy tín hiệu từ OUT1, cả hai IC chạy hệ số thấp và tải thuần dung.',
      'Hệ số nhiễu thấp và tải dung dễ gây tự dao động cao tần. Báo cáo cũng không có mạng Zobel và diode bảo vệ như mạch ứng dụng trong datasheet.',
      'Cả hai TDA lấy từ u₁: U4 không đảo +15,7, U5 đảo −15,7; vi sai U3C giảm còn 0,55. Thêm Zobel 1 Ω + 220 nF, 10 Ω nối tiếp, 1N4001 về hai đường nguồn, 100 kΩ xả.'],
    ['s2', 'Lớn', 'Mạch đệm cảm biến hệ số 5,7, không có lọc chống chồng phổ; 1 kΩ + BAT54S kẹp từ op-amp ±15 V.',
      'Đĩa PZT gần ngàm cho vài volt khi đầu dầm lệch vài mm, nhân 5,7 làm ADC bão hòa và PPF mất tác dụng. Lấy mẫu 1 kHz mà mode 3 ≈ 239 Hz và nhiễu > 500 Hz gập phổ. Dòng kẹp có thể tới ≈ 11 mA vào đường 3V3.',
      'C1 100 nF song song chia điện tích, hệ số chỉnh bằng VR1 (1–6); R5 4,7 kΩ + C3 100 nF vừa lọc 339 Hz vừa hạn dòng kẹp còn ≈ 2 mA.'],
    ['s2', 'Lớn', 'ADXL345 đặt ±4 g, ODR 800 Hz nhưng đọc 5 ms một lần.',
      'Đầu dầm lệch 5 mm ở 13,6 Hz có gia tốc ≈ 3,7 g, sát giới hạn. Đọc 200 Hz trong khi băng thông cảm biến 400 Hz làm mode 3 gập về ≈ 39 Hz trong phổ.',
      '±16 g full-res, FIFO stream 800 Hz, gửi đủ từng mẫu với mốc thời gian đều 1,25 ms.'],
    ['s2', 'Lớn', 'Loa gắn cứng que đẩy vào dầm.',
      'Độ cứng dầm tại 120 mm là 3EI/a³ ≈ 1010 N/m, cùng bậc với độ cứng treo màng loa. Que dán cứng làm tăng f₁, thêm cản (cuộn loa bị nối tắt qua ampli), làm sai thí nghiệm va đập.',
      'Khớp nam châm tháo nhanh; tháo que khi đo va đập; đo lại f₁ khi có que (bước hiệu chỉnh 7).'],
    ['s2', 'Lớn', 'Bi Ø10 thả 5 cm quá mạnh so với khả năng lực của đĩa buzzer.',
      'Xung ≈ 6 mN·s, biên độ đầu dầm ban đầu ≈ 6–7 mm, gần giới hạn 10 mm an toàn cho gốm. Ngân sách lực ước tính piezo chỉ triệt được dao động đến ≈ 3 mm, nên bộ điều khiển bão hòa trong phần lớn thời gian tắt dần.',
      'Dùng bi Ø6 thả 3 cm (≈ 1 mm) cho số liệu chính; bi Ø10 làm thí nghiệm minh họa giới hạn.'],
    ['s3', 'Nhỏ', 'Ảnh hưởng khối lượng gắn thêm bị đánh giá thấp (“vài phần trăm”).',
      'Khối lượng hiệu dụng mode 1 tại đầu dầm ≈ 9 g; ADXL ≈ 2 g kèm dây làm f₁ giảm ≈ 8–10 %. PPF đặt sai 10 % tần số vẫn chạy nhưng hiệu quả giảm.',
      'Firmware tự đo f₁ và lưu vào NVS.'],
    ['s3', 'Nhỏ', 'Hệ số khuếch đại trong firmware là hằng số thử-sai (×3,0; 0,002…), SIGN chỉnh tay, nút không chống dội.',
      'Hệ số không gắn với giới hạn ổn định nên mỗi mô hình phải dò lại, dễ vượt G<sub>max</sub>.',
      'Nhận dạng H₀ và SIGN tự động, chống dội 30 ms, nhấn giữ, tự tắt khi loa tắt mà u vẫn bão hòa > 20 % trong 3 s.'],
    ['s3', 'Nhỏ', 'Repo dùng ESP32-S3.',
      'ESP32-S3 (và C3) không có DAC; mạch theo báo cáo cần DAC1/DAC2.',
      'Dùng ESP32 DevKit V1 (WROOM-32). Muốn dùng S3 phải thêm DAC ngoài (MCP4725, PCM5102) và sửa mạch.'],
    ['s3', 'Nhỏ', 'Chi phí 994.000 đ trong Bảng 4.1.',
      'Bổ sung nguồn cách ly, linh kiện bảo vệ, nam châm, dầm dự phòng.',
      'Tổng mới ≈ 1,28 triệu đ, vẫn dưới 1,5 triệu đ.']
  ];
  $('issues').innerHTML = IS.map((x, i) => `<div class="issue"><span class="sev ${x[0]}">${x[1]}</span><div><p><b>${i + 1}. ${x[2]}</b></p><p class="mutedtxt">${x[3]}</p><p><b class="fix">Sửa:</b> ${x[4]}</p></div></div>`).join('');

  $('codeCtrl').textContent = `void controlTask(void *) {
  float y1 = 0, y2 = 0, e1 = 0, e2 = 0, hp = 0, rawPrev = 0, vel = 0;
  const float BETA  = 1.0f / (1.0f + 2 * PI * 1.0f * DT);    // HPF 1 Hz
  const float ALPHA = 1.0f / (1.0f + 2 * PI * 100.0f * DT);  // LPF 100 Hz cho đạo hàm
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
    float raw = readV(PIN_SENSOR) - offsetV;
    hp = BETA * (hp + raw - rawPrev);
    float y = hp, u = 0;
    if (ctrlOn) {
      if (mode == MODE_PPF) {                                 // (7)-(8): bộ lọc bậc 2 tại F_PPF
        float eta = (bn * (y + 2 * y1 + y2) - a1 * e1 - a2 * e2) / a0;
        e2 = e1; e1 = eta;
        u = SIGN * (ALPHA_PPF[gainIdx] / H0) * eta;           // g = α / H0, α = 0,25 / 0,5 / 0,75
      } else {                                                // (6): F = −Gv·ẋ
        vel = ALPHA * vel + (1 - ALPHA) * (y - y1) * FS;
        float gv = 2.0f * DZETA_DVF[gainIdx] / (K1_OVER_H0 * H0 * 2 * PI * F_PPF);
        u = -SIGN * gv * vel;
      }
    } else { e1 = e2 = 0; vel = 0; }
    // loa tắt mà u vẫn bão hòa > 20 % trong 3 cửa sổ 1 s liên tiếp -> tự tắt
    if (ctrlOn && fabsf(u) >= U_MAX) satN++;
    if (++winN >= 1000) { stuck = (ctrlOn && !excOn && satN > 200) ? stuck + 1 : 0;
                          if (stuck >= 3) { ctrlOn = false; tripped = true; } satN = winN = 0; }
    u = constrain(u, -U_MAX, U_MAX);                          // |Va| ≤ 26 V
    writeDAC(u);
    y2 = y1; y1 = y; rawPrev = raw;
  }
}`;
})();
