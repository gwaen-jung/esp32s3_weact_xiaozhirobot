/* Bản vẽ kỹ thuật và sơ đồ nguyên lý dạng SVG, vẽ bằng các hàm tiện ích */
(function () {
  const css = `
  .dwg .o{stroke:var(--ink);fill:none;stroke-width:1.5}
  .dwg .t{stroke:var(--ink);fill:none;stroke-width:.8}
  .dwg .hid{stroke:var(--ink2);fill:none;stroke-width:.9;stroke-dasharray:5 3}
  .dwg .cl{stroke:var(--ink2);fill:none;stroke-width:.7;stroke-dasharray:14 3 2 3}
  .dwg .dim{stroke:var(--ink2);fill:none;stroke-width:.7}
  .dwg .w{stroke:var(--ink);fill:none;stroke-width:1.4;stroke-linejoin:round}
  .dwg .wl{stroke:var(--act);fill:none;stroke-width:2;stroke-linejoin:round}
  .dwg .wm{stroke:var(--sense);fill:none;stroke-width:2;stroke-linejoin:round}
  .dwg .dot{fill:var(--ink)}
  .dwg .alu{fill:var(--alu);fill-opacity:.35;stroke:var(--ink);stroke-width:1.2}
  .dwg .steel{fill:var(--ink2);fill-opacity:.35;stroke:var(--ink);stroke-width:1.2}
  .dwg .wood{fill:var(--brass);fill-opacity:.16;stroke:var(--ink);stroke-width:1.2}
  .dwg .brass{fill:var(--brass);stroke:var(--ink);stroke-width:.8}
  .dwg .pztA{fill:var(--act);stroke:var(--ink);stroke-width:.8}
  .dwg .pztS{fill:var(--sense);stroke:var(--ink);stroke-width:.8}
  .dwg .pcb{fill:#7a4cc2;fill-opacity:.8;stroke:var(--ink);stroke-width:.8}
  .dwg .box{fill:var(--sheet);stroke:var(--ink);stroke-width:1.3}
  .dwg .boxA{fill:var(--act);fill-opacity:.1;stroke:var(--act);stroke-width:1.4}
  .dwg .boxS{fill:var(--sense);fill-opacity:.1;stroke:var(--sense);stroke-width:1.4}
  .dwg .glass{fill:var(--sense);fill-opacity:.08;stroke:var(--ink);stroke-width:1}
  .dwg .hatch{fill:url(#hatch)}
  .dwg .fillInk{fill:var(--ink)}
  .dwg .noF{fill:none}
  .dwg text.s{font-size:10px}.dwg text.m{font-size:12px}.dwg text.l{font-size:14px;font-weight:600}
  .dwg text.mut{fill:var(--ink2)}.dwg text.acc{fill:var(--act)}.dwg text.sen{fill:var(--sense)}.dwg text.bad{fill:var(--bad)}`;
  const st = document.createElement('style'); st.textContent = css; document.head.appendChild(st);

  let uid = 0;
  const f = n => (Math.round(n * 10) / 10);
  const svg = (w, h, body) => {
    const id = 'm' + (uid++);
    const defs = `<defs><marker id="${id}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,1 L10,5 L0,9 z" style="fill:var(--ink2)"/></marker>
      <marker id="${id}k" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" markerUnits="userSpaceOnUse" orient="auto-start-reverse"><path d="M0,1 L10,5 L0,9 z" style="fill:var(--ink)"/></marker>
      <marker id="${id}l" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="9" markerHeight="9" markerUnits="userSpaceOnUse" orient="auto-start-reverse"><path d="M0,1 L10,5 L0,9 z" style="fill:var(--act)"/></marker>
      <marker id="${id}s" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="9" markerHeight="9" markerUnits="userSpaceOnUse" orient="auto-start-reverse"><path d="M0,1 L10,5 L0,9 z" style="fill:var(--sense)"/></marker>
      <pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="6" style="stroke:var(--ink2);stroke-width:.8"/></pattern></defs>`;
    return `<svg viewBox="0 0 ${w} ${h}" role="img" xmlns="http://www.w3.org/2000/svg">${defs}${body.replace(/MARK/g, id)}</svg>`;
  };
  const L = (x1, y1, x2, y2, c = 'o') => `<line class="${c}" x1="${f(x1)}" y1="${f(y1)}" x2="${f(x2)}" y2="${f(y2)}"/>`;
  const Rc = (x, y, w, h, c = 'o', rx = 0) => `<rect class="${c}" x="${f(x)}" y="${f(y)}" width="${f(w)}" height="${f(h)}" rx="${rx}"/>`;
  const Ci = (x, y, r, c = 'o') => `<circle class="${c}" cx="${f(x)}" cy="${f(y)}" r="${f(r)}"/>`;
  const P = (d, c = 'o') => `<path class="${c}" d="${d}"/>`;
  const T = (x, y, s, c = 'm', a = 'start') => `<text class="${c}" x="${f(x)}" y="${f(y)}" text-anchor="${a}">${s}</text>`;
  const PL = (pts, c = 'w') => `<polyline class="${c}" points="${pts.map(p => p.map(f).join(',')).join(' ')}"/>`;
  // mũi tên: c = 'aK' (đen), 'aL' (vòng điều khiển), 'aM' (đo), còn lại = lớp nét
  const AR = (x1, y1, x2, y2, c = 'dim') => {
    const mk = c === 'aL' ? 'l' : c === 'aM' ? 's' : 'k', cls = c === 'aL' ? 'wl' : c === 'aM' ? 'wm' : c === 'aK' ? 't' : c;
    return `<line class="${cls}" x1="${f(x1)}" y1="${f(y1)}" x2="${f(x2)}" y2="${f(y2)}" marker-end="url(#MARK${mk})"/>`;
  };
  // kích thước ngang: đường kích thước tại y, đường gióng từ yA, yB
  const dimH = (x1, x2, y, label, yA = null, yB = null) => {
    let s = '';
    if (yA !== null) s += L(x1, yA, x1, y + (y > yA ? 4 : -4), 'dim');
    if (yB !== null) s += L(x2, yB, x2, y + (y > yB ? 4 : -4), 'dim');
    s += `<line class="dim" x1="${f(x1)}" y1="${f(y)}" x2="${f(x2)}" y2="${f(y)}" marker-start="url(#MARK)" marker-end="url(#MARK)"/>`;
    return s + T((x1 + x2) / 2, y - 3, label, 's', 'middle');
  };
  const dimV = (y1, y2, x, label, xA = null, xB = null) => {
    let s = '';
    if (xA !== null) s += L(xA, y1, x + (x > xA ? 4 : -4), y1, 'dim');
    if (xB !== null) s += L(xB, y2, x + (x > xB ? 4 : -4), y2, 'dim');
    s += `<line class="dim" x1="${f(x)}" y1="${f(y1)}" x2="${f(x)}" y2="${f(y2)}" marker-start="url(#MARK)" marker-end="url(#MARK)"/>`;
    return s + `<text class="s" x="${f(x - 4)}" y="${f((y1 + y2) / 2)}" text-anchor="middle" transform="rotate(-90 ${f(x - 4)} ${f((y1 + y2) / 2)})">${label}</text>`;
  };
  const lead = (x1, y1, x2, y2, label, anchor = 'start') => `<line class="dim" x1="${f(x1)}" y1="${f(y1)}" x2="${f(x2)}" y2="${f(y2)}"/>` + Ci(x1, y1, 1.6, 'dot') + T(x2 + (anchor === 'start' ? 3 : -3), y2 + 4, label, 's', anchor);

  // ================= Hình 1: hình chiếu đứng =================
  function side() {
    const S = 1.6, ox = 40, oy = 440;                 // 1 mm = 1,6 đơn vị
    const X = x => ox + x * S, Y = y => oy - y * S, xs = s => X(70 + s);
    let b = '';
    b += Rc(X(0), Y(0), 450 * S, 18 * S, 'wood');
    [10, 420].forEach(x => b += Rc(X(x), Y(-18), 20 * S, 6 * S, 'steel', 2));
    b += Rc(X(20), Y(80), 50 * S, 80 * S, 'alu');                         // trụ ngàm
    b += L(X(32), Y(0), X(32), Y(-18), 'hid') + L(X(58), Y(0), X(58), Y(-18), 'hid');
    b += Rc(X(20), Y(91.5), 50 * S, 10 * S, 'steel');                      // má kẹp
    b += Rc(X(40), Y(97.5), 10 * S, 6 * S, 'steel') + L(X(45), Y(91.5), X(45), Y(66.5), 'hid');
    b += Rc(X(20), Y(81.5), 350 * S, 1.5 * S, 'alu');                      // dầm
    // piezo
    const disc = (s0, top, cls) => {
      const y0 = top ? Y(81.5) : Y(80);
      b += Rc(xs(s0), top ? y0 - 0.6 * S : y0, 27 * S, 0.6 * S, 'brass');
      b += Rc(xs(s0 + 3.5), top ? y0 - 1.4 * S : y0 + 0.6 * S, 20 * S, 0.8 * S, cls);
    };
    disc(10, true, 'pztS'); disc(38, true, 'pztA'); disc(10, false, 'pztA'); disc(38, false, 'pztA');
    b += Rc(xs(70), Y(81.5) - 1.2 * S, 20 * S, 1.2 * S, 'brass');           // giám sát Ø20
    b += Rc(xs(280), Y(83.1), 20 * S, 1.6 * S, 'pcb');                      // ADXL
    b += Rc(xs(116), Y(80), 8 * S, 3 * S, 'steel');                         // nam châm
    b += Rc(xs(119), Y(77), 2 * S, 25 * S, 'fillInk');                      // que carbon
    b += Rc(xs(116), Y(52), 8 * S, 3 * S, 'steel');
    b += P(`M${xs(87)},${Y(20)} L${xs(153)},${Y(20)} L${xs(143)},${Y(49)} L${xs(97)},${Y(49)} Z`, 'steel'); // loa
    b += P(`M${xs(97)},${Y(49)} Q${xs(120)},${Y(46)} ${xs(143)},${Y(49)}`, 't');
    b += Rc(xs(80), Y(20), 80 * S, 20 * S, 'glass');                        // giá loa
    // ống dẫn bi
    b += Rc(xs(254), Y(211.5), 12 * S, 100 * S, 'glass') + Ci(xs(260), Y(205), 3 * S, 'steel');
    b += Rc(xs(220), Y(220), 4 * S, 220 * S, 'alu') + Rc(xs(220), Y(180), 34 * S, 4 * S, 'alu');
    b += L(xs(260), Y(111.5), xs(260), Y(83), 'hid');
    // hộp mạch phía sau
    b += Rc(X(250), Y(60), 150 * S, 60 * S, 'hid');
    b += T(X(325), Y(30), 'Hộp mạch (phía sau dầm)', 's mut', 'middle');
    // đường tâm
    b += L(X(12), Y(80.75), X(380), Y(80.75), 'cl');
    // kích thước
    b += dimH(X(0), X(450), Y(-34), '450', Y(-18), Y(-18));
    b += dimH(X(20), X(70), Y(118), '50 (kẹp)', Y(101.5), Y(101.5));
    b += dimH(X(70), X(370), Y(250), 'L = 300 (tự do)', Y(83), Y(83));
    b += dimH(X(70), xs(120), Y(140), '120', Y(83), Y(80));
    b += dimH(X(70), xs(260), Y(232), '260', Y(83), Y(212));
    b += dimH(X(70), xs(290), Y(241), '290', Y(83), Y(86));
    b += dimV(Y(0), Y(80), X(10), '80', X(20), X(20));
    b += dimV(Y(83), Y(111.5), xs(275), '30', xs(266), xs(266));
    // chú thích
    b += lead(X(45), Y(40), X(-2) + 0, Y(150), '① Trụ ngàm Al 50×50×80', 'start');
    b += lead(X(45), Y(96), X(80), Y(170), '② Má kẹp thép + 2×M6', 'start');
    b += lead(xs(23), Y(83), xs(18), Y(200), '③ T1 cảm biến', 'start');
    b += lead(xs(51), Y(78), xs(64), Y(55), '④ T2/B1/B2 chấp hành', 'start');
    b += lead(xs(120), Y(65), xs(165), Y(110), '⑤ Que đẩy + khớp nam châm', 'start');
    b += lead(xs(140), Y(35), xs(170), Y(10), '⑥ Loa 4 Ω Ø66 trên giá', 'start');
    b += lead(xs(260), Y(160), xs(275), Y(165), '⑦ Ống dẫn bi Ø12', 'start');
    b += lead(xs(290), Y(84), xs(300), Y(110), '⑧ ADXL345', 'start');
    b += lead(X(200), Y(-9), X(215), Y(-5), '⑨ Đế ván ép 18', 'start');
    b += T(X(0), Y(-52), 'HÌNH CHIẾU ĐỨNG · tỉ lệ 1:2 · chiều dày dầm và piezo vẽ phóng to', 's mut');
    return svg(820, 545, b);
  }

  // ================= Hình 2: hình chiếu bằng =================
  function top() {
    const S = 1.6, ox = 40, oy = 30, X = x => ox + x * S, Z = z => oy + (200 - z) * S;
    let b = '';
    b += Rc(X(0), Z(200), 450 * S, 200 * S, 'wood');
    b += Rc(X(20), Z(95), 50 * S, 50 * S, 'steel');
    b += Rc(X(20), Z(85), 350 * S, 30 * S, 'alu');
    [50, 90].forEach(z => b += Ci(X(45), Z(z), 5, 'o') + L(X(45) - 7, Z(z), X(45) + 7, Z(z), 't') + L(X(45), Z(z) - 7, X(45), Z(z) + 7, 't'));
    [32, 58].forEach(x => b += Ci(X(x), Z(70), 4, 'hid'));
    const disc = (s, cls) => b += Ci(X(70 + s), Z(70), 13.5 * S, 'brass') + Ci(X(70 + s), Z(70), 10 * S, cls);
    disc(23.5, 'pztS'); disc(51.5, 'pztA');
    b += Ci(X(150), Z(70), 10 * S, 'brass');
    b += Rc(X(350), Z(77.5), 20 * S, 15 * S, 'pcb');
    b += Ci(X(190), Z(70), 33 * S, 'hid') + Rc(X(150), Z(110), 80 * S, 80 * S, 'hid');
    b += Ci(X(330), Z(70), 6 * S, 'glass') + Rc(X(328), Z(76), 4 * S, 56 * S, 'alu') + Ci(X(330), Z(18), 6, 'alu');
    b += Rc(X(250), Z(195), 150 * S, 100 * S, 'box');
    b += Rc(X(262), Z(185), 40 * S, 22 * S, 'fillInk');
    [320, 345, 370].forEach(x => b += Ci(X(x), Z(170), 7, 'o'));
    b += Ci(X(388), Z(170), 6, 'steel');
    b += T(X(325), Z(130), 'Hộp mạch 150×100×60', 's', 'middle') + T(X(282), Z(155), 'OLED', 's mut', 'middle');
    b += T(X(345), Z(152), 'S1  S2  S3', 's mut', 'middle') + T(X(388), Z(152), 'VR2', 's mut', 'middle');
    b += P(`M${X(70)},${Z(85)} C${X(80)},${Z(120)} ${X(200)},${Z(120)} ${X(250)},${Z(130)}`, 'hid');
    b += T(X(150), Z(122), 'cáp piezo + ADXL (mềm, võng tại ngàm)', 's mut', 'middle');
    b += L(X(10), Z(70), X(380), Z(70), 'cl');
    b += dimH(X(0), X(450), Z(-14), '450', Z(0), Z(0));
    b += dimV(Z(200), Z(0), X(-14), '200', X(0), X(0));
    b += dimV(Z(70), Z(0), X(470), '70', X(450), X(450));
    b += dimH(X(0), X(45), Z(228), '45', Z(200), Z(95));
    b += dimH(X(45), X(190), Z(214), '145 (tâm loa)', Z(95), Z(103));
    b += dimH(X(45), X(330), Z(222), '285 (tâm ống bi)', null, Z(76));
    b += T(X(0), Z(-30), 'HÌNH CHIẾU BẰNG · nét đứt: loa dưới dầm, vít bắt trụ từ dưới đế', 's mut');
    return svg(820, 420, b);
  }

  // ================= Hình 3: bố trí trên dầm =================
  function beamLayout() {
    const S = 2, ox = 50, X = s => ox + (s + 50) * S;
    const band = (y0, title, items) => {
      let b = Rc(X(-50), y0, 50 * S, 30 * S, 'hatch') + Rc(X(-50), y0, 350 * S, 30 * S, 'o');
      b += Rc(X(0), y0 - 4, 95 * S, 30 * S + 8, 'hid');
      b += L(X(0), y0 - 10, X(0), y0 + 70, 'cl');
      b += T(X(-50), y0 - 8, title, 'l');
      return b + items;
    };
    const disc = (s, y0, cls, lbl) => Ci(X(s), y0 + 30, 13.5 * S, 'brass') + Ci(X(s), y0 + 30, 10 * S, cls) + T(X(s), y0 + 34, lbl, 'm', 'middle');
    let b = '';
    const y1 = 40, y2 = 240;
    b += band(y1, 'MẶT TRÊN', disc(23.5, y1 - 15, 'pztS', 'T1') + disc(51.5, y1 - 15, 'pztA', 'T2') +
      Ci(X(80), y1 + 15, 10 * S, 'brass') + T(X(80), y1 + 19, 'M', 's', 'middle') +
      Rc(X(280), y1 + 7.5 * S - 7.5 * S + 0, 20 * S, 15 * S + 15, 'pcb') + T(X(290), y1 + 19, 'ADXL', 's', 'middle') +
      L(X(260) - 6, y1 + 9, X(260) + 6, y1 + 21, 'o') + L(X(260) + 6, y1 + 9, X(260) - 6, y1 + 21, 'o'));
    b += band(y2, 'MẶT DƯỚI (nhìn từ dưới lên)', disc(23.5, y2 - 15, 'pztA', 'B1') + disc(51.5, y2 - 15, 'pztA', 'B2') + Ci(X(120), y2 + 15, 4 * S, 'steel') + T(X(120), y2 + 42, 'nam châm', 's', 'middle'));
    // kích thước
    b += dimH(X(0), X(10), y1 + 76, '10', y1 + 60, y1 + 60);
    b += dimH(X(0), X(23.5), y1 + 90, '23,5', null, y1 + 60);
    b += dimH(X(0), X(51.5), y1 + 104, '51,5', null, y1 + 60);
    b += dimH(X(0), X(80), y1 + 118, '80', null, y1 + 30);
    b += dimH(X(0), X(260), y1 + 132, '260 (điểm va đập)', null, y1 + 21);
    b += dimH(X(0), X(290), y1 + 146, '290', null, y1 + 45);
    b += dimH(X(0), X(300), y1 - 26, '300', y1 - 4, y1);
    b += dimH(X(-50), X(0), y1 - 26, '50 kẹp', y1, y1 - 4);
    b += dimH(X(0), X(95), y2 + 76, '95 vùng đánh nhám P400 (cả 2 mặt)', y2 + 64, y2 + 64);
    b += dimH(X(0), X(120), y2 + 92, '120 (que đẩy)', null, y2 + 30);
    b += dimV(y1, y1 + 60, X(310) + 12, '30', X(300), X(300));
    b += T(X(140), y2 + 118, 'Ø27 vành đồng · Ø20 gốm · khe 1 mm giữa 2 đĩa · T1 thẳng trên B1, T2 thẳng trên B2', 's mut', 'middle');
    b += T(X(140), y2 + 132, 'Dây: gốm T1 → U3A (cáp chống nhiễu) · gốm T2 → OUT1 · gốm B1, B2 → OUT2 · mọi vành đồng → GND', 's mut', 'middle');
    return svg(820, 400, b);
  }

  // ================= Hình 4: ngàm =================
  function clamp() {
    const S = 2.2;
    let b = '';
    // hình chiếu theo trục dầm (Z–Y)
    const ox = 30, oy = 260, X = z => ox + (z + 25) * S, Y = y => oy - y * S;
    b += Rc(X(-25), Y(80), 50 * S, 80 * S, 'alu') + Rc(X(-15), Y(81.5), 30 * S, 1.5 * S + 1, 'alu') + Rc(X(-25), Y(91.5), 50 * S, 10 * S, 'steel');
    [-20, 20].forEach(z => {
      b += Rc(X(z - 5), Y(97.5), 10 * S, 6 * S, 'steel');
      b += L(X(z - 3), Y(91.5), X(z - 3), Y(65), 'hid') + L(X(z + 3), Y(91.5), X(z + 3), Y(65), 'hid') + L(X(z), Y(100), X(z), Y(60), 'cl');
    });
    [-7, 7].forEach(z => b += L(X(z - 3), Y(0), X(z - 3), Y(15), 'hid') + L(X(z + 3), Y(0), X(z + 3), Y(15), 'hid'));
    b += dimH(X(-25), X(25), Y(-10), '50', Y(0), Y(0));
    b += dimH(X(-20), X(20), Y(112), '40 (2×M6)', Y(103.5), Y(103.5));
    b += dimV(Y(0), Y(80), X(-25) - 18, '80', X(-25), X(-25));
    b += dimV(Y(81.5), Y(91.5), X(25) + 20, '10', X(25), X(25));
    b += lead(X(0), Y(81), X(30), Y(70), 'dầm 30×1,5', 'start');
    b += T(X(-25), Y(-26), 'NHÌN DỌC TRỤC DẦM', 's mut');
    // chú thích gia công
    const tx = 210;
    b += T(tx, 40, 'Yêu cầu gia công', 'l');
    ['Trụ: nhôm 6061 hoặc thép CT3, 50 × 50 × 80', 'Mặt kẹp phay phẳng, Ra ≤ 1,6 µm', 'Mép kẹp phía dầm tự do: vuông, không vê', 'Ta-rô M6 sâu 15 mm: 2 lỗ trên, 2 lỗ dưới', 'Má kẹp thép 50 × 50 × 10, lỗ Ø6,5', 'Siết bu lông đều: 8–10 N·m', 'Cos vòng GND dưới đầu một bu lông', 'Trụ bắt đế bằng 2 vít M6×25 đầu chìm']
      .forEach((s, i) => b += T(tx, 62 + i * 18, '· ' + s, 's'));
    return svg(470, 300, b);
  }

  // ================= Hình 5: mặt cắt piezo =================
  function stack() {
    let b = '';
    const x0 = 60, w = 260;
    const lay = [['Gốm PZT T2 (0,2)', 'pztA', 14], ['Đồng thau (0,25)', 'brass', 10], ['Epoxy (≈ 0,03)', 'glass', 4], ['Dầm nhôm 6061 (1,5)', 'alu', 40], ['Epoxy', 'glass', 4], ['Đồng thau', 'brass', 10], ['Gốm PZT B2', 'pztA', 14]];
    let y = 50; const ys = [];
    lay.forEach(([n, c, h], i) => {
      const inset = (i === 0 || i === 6) ? 30 : 0;
      b += Rc(x0 + inset, y, w - 2 * inset, h, c);
      b += T(x0 + w + 10, y + h / 2 + 4, n, 's');
      ys.push([y, h]); y += h;
    });
    // phân cực và điện trường
    b += AR(x0 + 60, 61, x0 + 60, 49) + T(x0 + 66, 46, 'P', 's');
    b += AR(x0 + 60, 129, x0 + 60, 142) + T(x0 + 66, 150, 'P', 's');
    b += T(x0 + 150, 38, '+13 V (OUT1)', 's acc', 'middle') + T(x0 + 150, 166, '−13 V (OUT2)', 's acc', 'middle');
    b += T(x0 + 130, 98, 'GND', 's', 'middle');
    // mô men uốn
    b += P(`M${x0 - 10},${120} q -30 -25 0 -50`, 'dim') + `<path class="dim" d="M${x0 - 10},70 l 4 6" />`;
    b += P(`M${x0 + w + 160},${70} q 30 25 0 50`, 'dim');
    b += T(x0 + 130, 196, 'Cùng chiều phân cực P (hướng ra ngoài dầm), điện áp ngược dấu:', 's', 'middle');
    b += T(x0 + 130, 212, 'T2 co khi B2 giãn → mô men uốn M tỉ lệ Va', 's', 'middle');
    b += T(x0 + 130, 228, 'Dây gốm hàn trước khi dán; dây vành đồng hàn sau khi dán', 's mut', 'middle');
    return svg(470, 250, b);
  }

  // ================= Hình 6: kích rung + ống bi =================
  function shaker() {
    let b = '';
    // loa
    const cx = 170;
    b += Rc(40, 40, 300, 8, 'alu') + T(345, 48, 'dầm (x = 120 mm)', 's');
    b += Rc(cx - 8, 48, 16, 6, 'steel') + T(cx + 14, 58, 'nam châm Ø8×3 (dán dầm)', 's');
    b += Rc(cx - 8, 56, 16, 6, 'steel') + Rc(cx - 9, 62, 18, 2, 'steel') + T(cx + 14, 68, 'nam châm + long đen M4', 's');
    b += Rc(cx - 2, 64, 4, 54, 'fillInk') + T(cx + 14, 96, 'que carbon Ø3, dài 25–30', 's');
    b += P(`M${cx - 60},118 Q${cx},108 ${cx + 60},118`, 'o');
    b += P(`M${cx - 66},118 L${cx + 66},118 L${cx + 44},168 L${cx - 44},168 Z`, 'steel');
    b += Rc(cx - 22, 168, 44, 18, 'steel') + T(cx + 75, 150, 'loa 4 Ω 5 W Ø66', 's');
    b += Rc(cx - 80, 186, 160, 10, 'glass') + T(cx, 210, 'giá mica 5 mm', 's', 'middle');
    b += dimV(54, 118, cx - 90, '≈ 25 có thể chỉnh', cx - 60, cx - 60);
    b += T(40, 240, 'Que đẩy hút vào dầm bằng nam châm: tháo bằng tay khi làm thí nghiệm va đập.', 's mut');
    b += T(40, 254, 'Khi lắp, chỉnh giá sao cho dầm vẫn thẳng (que không đẩy lệch dầm).', 's mut');
    // ống bi
    const tx = 560;
    b += Rc(tx - 200 + 160, 180, 260, 8, 'alu') + T(tx + 70, 200, 'dầm (x = 260 mm)', 's');
    b += Rc(tx - 8, 40, 16, 110, 'glass') + Ci(tx, 60, 4, 'steel') + Ci(tx, 92, 6, 'steel');
    b += L(tx - 14, 66, tx + 30, 66, 'o') + T(tx + 34, 70, 'chốt giữ bi (que tăm)', 's');
    b += T(tx + 34, 96, 'bi Ø6 (chính) / Ø10 (minh họa)', 's');
    b += dimV(150, 180, tx - 24, '30', tx - 8, tx - 8);
    b += T(tx + 34, 130, 'ống mica Ø12/Ø8, dài 100', 's');
    b += T(tx - 120, 240, 'Thả: rút chốt, bi rơi 30 mm, bật ra ngoài sau va chạm.', 's mut');
    b += T(tx - 120, 254, 'Xung ≈ 1 mN·s (Ø6) · ≈ 6 mN·s (Ø10, 5 cm).', 's mut');
    return svg(820, 270, b);
  }

  // ================= Hình 7: sơ đồ khối =================
  function block() {
    let b = '';
    const bx = (x, y, w, h, t1, t2, c = 'box') => Rc(x, y, w, h, c, 3) + T(x + w / 2, y + h / 2 - (t2 ? 3 : -4), t1, 'm', 'middle') + (t2 ? T(x + w / 2, y + h / 2 + 13, t2, 's mut', 'middle') : '');
    const arr = (pts, c) => PL(pts, c) + AR(pts[pts.length - 2][0], pts[pts.length - 2][1], pts[pts.length - 1][0], pts[pts.length - 1][1], c === 'wl' ? 'aL' : c === 'wm' ? 'aM' : 'aK');
    b += bx(30, 150, 140, 60, 'Dầm công-xôn', 'Al 300×30×1,5');
    b += bx(220, 52, 140, 46, 'Đệm cảm biến', 'U3A TL074 · lọc 339 Hz', 'boxS');
    b += bx(220, 160, 140, 44, 'ADXL345', 'đầu dầm 290 mm', 'boxS');
    b += Rc(410, 30, 200, 230, 'box', 4) + T(510, 52, 'ESP32 DevKit V1', 'l', 'middle');
    b += bx(425, 70, 170, 40, 'Lõi 1 · 1 kHz', 'ADC→HPF→PPF/DVF→giới hạn', 'boxA');
    b += bx(425, 130, 170, 40, 'Lõi 0 · 200 Hz', 'nút · OLED · ADXL · CSV');
    b += bx(425, 190, 170, 40, 'Nhận dạng + NVS', 'f₁ · H₀ · SIGN');
    b += bx(430, 290, 160, 46, 'Vi sai U3C', 'u₁ = 0,55·(V_DAC − Vref)', 'boxA');
    b += bx(220, 290, 140, 46, 'Cầu BTL', '2 × TDA2030A · ±26 V', 'boxA');
    b += bx(660, 30, 130, 44, 'OLED + 3 nút', 'VR2 tần số loa');
    b += bx(660, 95, 130, 44, 'Máy tính', 'USB 921600 · Python');
    b += bx(660, 160, 130, 44, 'PAM8403 → loa', 'DAC2 hoặc jack 3,5');
    b += bx(660, 230, 130, 40, 'Vref 1,65 V', 'đệm U3B');
    b += bx(660, 292, 130, 44, 'Nguồn', '24 V → ±15 V, 5 V');
    // vòng điều khiển kín
    b += arr([[100, 150], [100, 75], [220, 75]], 'wl') + T(106, 118, 'T1', 's sen');
    b += arr([[360, 75], [390, 75], [390, 90], [425, 90]], 'wl');
    b += arr([[595, 90], [602, 90], [602, 313], [590, 313]], 'wl') + T(598, 280, 'DAC1', 's acc', 'end');
    b += arr([[430, 313], [360, 313]], 'wl');
    b += arr([[220, 313], [100, 313], [100, 210]], 'wl') + T(106, 262, 'T2 · B1 · B2', 's acc');
    // đo đánh giá
    b += arr([[170, 182], [220, 182]], 'wm');
    b += arr([[360, 182], [395, 182], [395, 150], [425, 150]], 'wm') + T(364, 176, 'I²C', 's sen');
    b += PL([[595, 145], [660, 52]], 'w') + PL([[595, 155], [660, 117]], 'w') + PL([[610, 182], [660, 182]], 'w');
    b += PL([[660, 250], [575, 250], [575, 290]], 'hid');
    // nhiễu từ loa
    b += PL([[790, 182], [805, 182], [805, 350], [60, 350], [60, 210]], 'hid') + AR(60, 218, 60, 210, 'aK');
    b += T(430, 364, 'lực kích của loa tại x = 120 mm (nhiễu cần triệt)', 's mut', 'middle');
    return svg(820, 372, b);
  }

  // ================= Hình 8: lưu đồ =================
  function flow() {
    let b = '';
    const bx = (x, y, w, h, t, c = 'box') => Rc(x, y, w, h, c, 3) + T(x + w / 2, y + h / 2 + 4, t, 's', 'middle');
    const dm = (x, y, w, h, t) => P(`M${x + w / 2},${y} L${x + w},${y + h / 2} L${x + w / 2},${y + h} L${x},${y + h / 2} Z`, 'box') + T(x + w / 2, y + h / 2 + 4, t, 's', 'middle');
    const ar = (x1, y1, x2, y2) => AR(x1, y1, x2, y2, 'aK');
    ['Khởi động: ADC 12 bit, DAC, I²C 400 kHz, OLED', 'ADXL345 ±16 g, FIFO 800 Hz · đọc f₁, H₀, SIGN từ NVS', 'Đo offset cảm biến: 1000 mẫu (dầm đứng yên)', 'Tạo 2 tác vụ FreeRTOS · ctrlOn = false']
      .forEach((t, i) => { b += bx(260, 10 + i * 40, 300, 28, t); if (i) b += ar(410, i * 40, 410, 10 + i * 40); });
    b += PL([[410, 158], [410, 170], [160, 170], [160, 185]], 't') + PL([[410, 170], [650, 170], [650, 185]], 't');
    b += T(160, 200, 'LÕI 1 · controlTask', 'm acc', 'middle');
    b += bx(40, 210, 240, 26, 'vTaskDelayUntil(1 ms)') + ar(160, 236, 160, 250);
    b += bx(40, 250, 240, 26, 'y = HPF 1 Hz (ADC − offset)') + ar(160, 276, 160, 285);
    b += dm(60, 285, 200, 40, 'Nhận dạng / thử Va?') + bx(300, 292, 140, 26, 'u = sin 3 Hz / 13 Hz') + ar(260, 305, 300, 305) + T(266, 300, 'có', 's mut');
    b += ar(160, 325, 160, 340) + T(166, 336, 'không', 's mut');
    b += dm(60, 340, 200, 40, 'ctrlOn?') + bx(300, 347, 140, 26, 'u = 0, xóa bộ nhớ') + ar(260, 360, 300, 360) + T(266, 355, 'không', 's mut');
    b += ar(160, 380, 160, 395) + T(166, 391, 'có', 's mut');
    b += dm(60, 395, 200, 40, 'PPF hay DVF?');
    b += bx(30, 450, 125, 26, 'u = SIGN·α/H₀·η') + bx(165, 450, 125, 26, 'u = −SIGN·g_v·ẏ');
    b += ar(115, 425, 92, 450) + ar(205, 425, 228, 450);
    b += bx(30, 490, 260, 26, 'loa tắt, bão hòa > 20 % × 3 s → tự tắt') + ar(92, 476, 120, 490) + ar(228, 476, 200, 490);
    b += bx(30, 530, 260, 26, '|u| ≤ 1,5 V → DAC1 · sin loa → DAC2') + ar(160, 516, 160, 530);
    b += PL([[30, 543], [18, 543], [18, 223], [40, 223]], 't') + AR(32, 223, 40, 223, 'aK');
    b += PL([[440, 305], [455, 305], [455, 580], [160, 580], [160, 556]], 't') + PL([[440, 360], [455, 360]], 't');
    b += T(650, 200, 'LÕI 0 · uiTask (5 ms)', 'm sen', 'middle');
    ['Chống dội 3 nút, nhấn giữ 1,5 s', 'Lệnh Serial: f a e c m g p i t', 'Đọc VR2 → tần số loa', 'Xả FIFO ADXL → CSV 800 Hz', 'OLED mỗi 250 ms', 'vTaskDelay(5 ms)']
      .forEach((t, i) => { b += bx(540, 210 + i * 40, 220, 26, t); if (i) b += ar(650, 196 + i * 40, 650, 210 + i * 40); });
    b += PL([[540, 433], [522, 433], [522, 223], [540, 223]], 't') + AR(532, 223, 540, 223, 'aK');
    b += Rc(520, 470, 260, 46, 'boxS', 3) + T(650, 489, 'Giữ GAIN: gõ dầm → đo f₁', 's', 'middle') + T(650, 505, 'sin 3 Hz → H₀, SIGN → lưu NVS', 's', 'middle');
    b += PL([[760, 223], [790, 223], [790, 493], [780, 493]], 'hid');
    return svg(820, 590, b);
  }

  // ================= Ký hiệu mạch =================
  const K = {
    R(x, y, d, ref, val, trim) {      // dài 60
      const h = d === 'h';
      let s = h ? L(x, y, x + 15, y, 'w') + Rc(x + 15, y - 5, 30, 10, 'box') + L(x + 45, y, x + 60, y, 'w')
        : L(x, y, x, y + 15, 'w') + Rc(x - 5, y + 15, 10, 30, 'box') + L(x, y + 45, x, y + 60, 'w');
      if (trim) s += h ? AR(x + 14, y + 12, x + 46, y - 12, 't') : AR(x - 12, y + 46, x + 12, y + 14, 't');
      s += h ? T(x + 30, y - 9, ref, 's', 'middle') + T(x + 30, y + 19, val, 's mut', 'middle')
        : T(x + 9, y + 28, ref, 's') + T(x + 9, y + 40, val, 's mut');
      return s;
    },
    C(x, y, d, ref, val, pol) {       // dài 40
      const h = d === 'h';
      let s = h ? L(x, y, x + 17, y, 'w') + L(x + 17, y - 10, x + 17, y + 10, 'o') + L(x + 23, y - 10, x + 23, y + 10, 'o') + L(x + 23, y, x + 40, y, 'w')
        : L(x, y, x, y + 17, 'w') + L(x - 10, y + 17, x + 10, y + 17, 'o') + L(x - 10, y + 23, x + 10, y + 23, 'o') + L(x, y + 23, x, y + 40, 'w');
      if (pol) s += h ? T(x + 10, y - 12, '+', 's') : T(x - 16, y + 14, '+', 's');
      s += h ? T(x + 20, y - 14, ref, 's', 'middle') + T(x + 20, y + 22, val, 's mut', 'middle')
        : T(x + 13, y + 18, ref, 's') + T(x + 13, y + 30, val, 's mut');
      return s;
    },
    D(x1, y1, x2, y2, ref) {          // anode (x1,y1) → cathode (x2,y2), thẳng đứng hoặc ngang
      const mx = (x1 + x2) / 2, my = (y1 + y2) / 2, v = x1 === x2, dir = v ? Math.sign(y2 - y1) : Math.sign(x2 - x1);
      let s = L(x1, y1, x2, y2, 'w');
      s += v ? P(`M${mx - 7},${my - 6 * dir} L${mx + 7},${my - 6 * dir} L${mx},${my + 6 * dir} Z`, 'box') + L(mx - 7, my + 6 * dir, mx + 7, my + 6 * dir, 'o')
        : P(`M${mx - 6 * dir},${my - 7} L${mx - 6 * dir},${my + 7} L${mx + 6 * dir},${my} Z`, 'box') + L(mx + 6 * dir, my - 7, mx + 6 * dir, my + 7, 'o');
      return s + (ref ? T(v ? mx + 10 : mx, v ? my + 4 : my - 10, ref, 's', v ? 'start' : 'middle') : '');
    },
    gnd(x, y) { return L(x, y, x, y + 8, 'w') + L(x - 9, y + 8, x + 9, y + 8, 'o') + L(x - 6, y + 12, x + 6, y + 12, 'o') + L(x - 3, y + 16, x + 3, y + 16, 'o'); },
    rail(x, y, lbl, down) { return down ? L(x, y, x, y + 8, 'w') + L(x - 8, y + 8, x + 8, y + 8, 'o') + T(x, y + 22, lbl, 's', 'middle') : L(x, y, x, y - 8, 'w') + L(x - 8, y - 8, x + 8, y - 8, 'o') + T(x, y - 12, lbl, 's', 'middle'); },
    dot(x, y) { return Ci(x, y, 2.8, 'dot'); },
    op(x, y, ref, flip, pins, w = 60) {  // − trên nếu !flip; ngõ ra (x+w, y)
      const a = flip ? '+' : '−', c = flip ? '−' : '+';
      let s = P(`M${x},${y - 26} L${x + w},${y} L${x},${y + 26} Z`, 'box');
      s += T(x + 4, y - 8, a, 'm') + T(x + 4, y + 17, c, 'm') + T(x + w * 0.35, y + 4, ref, 's', 'middle');
      if (pins) s += T(x - 3, y - 15, pins[0], 's mut', 'end') + T(x - 3, y + 22, pins[1], 's mut', 'end') + T(x + w + 2, y - 6, pins[2], 's mut');
      return s;
    },
    pz(x, y, ref, cls = 'pztA') { // từ (x,y) xuống (x,y+50)
      return L(x, y, x, y + 17, 'w') + L(x - 12, y + 17, x + 12, y + 17, 'o') + Rc(x - 9, y + 19, 18, 12, cls) + L(x - 12, y + 33, x + 12, y + 33, 'o') + L(x, y + 33, x, y + 50, 'w') + T(x + 16, y + 29, ref, 's') + T(x - 16, y + 15, '+', 's', 'end');
    },
    port(x, y, lbl, left) { return left ? P(`M${x},${y} l-8,-7 h-${lbl.length * 6.6 + 8} v14 h${lbl.length * 6.6 + 8} z`, 'box') + T(x - 10, y + 4, lbl, 's', 'end') : P(`M${x},${y} l8,-7 h${lbl.length * 6.6 + 8} v14 h-${lbl.length * 6.6 + 8} z`, 'box') + T(x + 10, y + 4, lbl, 's'); },
    w(...pts) { return PL(pts); }
  };

  // ================= Sơ đồ A =================
  function schA() {
    let b = '';
    b += T(20, 24, 'A1 · ĐỆM CẢM BIẾN T1 → GPIO34', 'l');
    b += K.pz(70, 80, 'T1', 'pztS') + K.gnd(70, 130) + T(84, 152, 'vành đồng / dầm', 's mut');
    b += K.w([70, 80], [70, 60], [260, 60]);
    b += K.dot(140, 60) + K.C(140, 60, 'v', 'C1', '100n') + K.gnd(140, 100);
    b += K.dot(215, 60) + K.R(215, 60, 'v', 'R1', '10M') + K.port(215, 128, 'VREF', true);
    b += K.w([215, 120], [215, 128]);
    b += K.R(260, 60, 'h', 'R2', '10k') + K.w([320, 60], [340, 60]);
    b += K.op(340, 72, 'U3A', true, ['3', '2', '1']);
    b += K.w([340, 84], [325, 84], [325, 160]) + K.dot(325, 130);
    b += K.R(265, 130, 'h', 'R3', '10k') + K.port(265, 130, 'VREF', true);
    b += K.R(325, 160, 'h', 'VR1', '50k', true) + K.w([385, 160], [415, 160], [415, 72]);
    b += K.w([400, 72], [440, 72]) + K.dot(415, 72);
    b += K.R(440, 72, 'h', 'R5', '4,7k') + K.w([500, 72], [610, 72]);
    b += K.dot(530, 72) + K.C(530, 72, 'v', 'C3', '100n') + K.gnd(530, 112);
    b += K.dot(590, 72) + K.D(590, 72, 590, 34, '') + K.rail(590, 34, '3V3') + K.D(590, 112, 590, 72, '') + K.gnd(590, 112) + T(601, 96, 'D3 BAT54S', 's');
    b += K.w([610, 72], [625, 72]) + K.port(625, 72, 'GPIO34');
    b += T(250, 200, 'Hệ số = 1 + VR1/R3 (1…6). C1 chia điện tích: V_ra ≈ Q/(C_p + C1).', 's mut');
    b += T(250, 214, 'HPF: f_c = 1/(2π·R1·(C_p+C1)) ≈ 0,14 Hz · lọc chống chồng phổ R5·C3 = 339 Hz.', 's mut');
    // Vref
    b += T(20, 262, 'A2 · ĐỆM VREF 1,65 V', 'l');
    b += K.rail(70, 300, '3V3') + K.R(70, 300, 'v', 'R6', '10k') + K.R(70, 360, 'v', 'R7', '10k') + K.gnd(70, 420);
    b += K.dot(70, 360) + K.w([70, 360], [180, 360]) + K.dot(120, 360) + K.C(120, 360, 'v', 'C4', '10µ') + K.gnd(120, 400);
    b += K.op(180, 348, 'U3B', true, ['5', '6', '7']);
    b += K.w([180, 372], [165, 372], [165, 400], [255, 400], [255, 348]);
    b += K.w([240, 348], [270, 348]) + K.dot(255, 348) + K.R(270, 348, 'h', 'R8', '100Ω') + K.w([330, 348], [360, 348]);
    b += K.dot(345, 348) + K.C(345, 348, 'v', 'C5', '1µ') + K.gnd(345, 388) + K.port(360, 348, 'VREF');
    // giám sát
    b += Rc(470, 250, 330, 170, 'hid', 4);
    b += T(485, 272, 'A3 · KÊNH GIÁM SÁT (U3D) → GPIO35', 'l');
    ['Giống A1, linh kiện:', 'PZ_M Ø20 tại 80 mm · C2 47n · R21 10M → VREF', 'R22 10k → U3D(+) chân 12', 'R23 10k (− → VREF), R24 10k (hồi tiếp) → hệ số 2', 'R25 4,7k + C7 100n → GPIO35 · D4 BAT54S', 'Dùng để thấy sự khác pha mode 2 giữa', 'cảm biến đồng vị trí và không đồng vị trí.']
      .forEach((s, i) => b += T(485, 294 + i * 17, s, 's'));
    b += T(20, 446, 'U3 = TL074: chân 4 → +15 V, chân 11 → −15 V, mỗi chân 100 nF (C13, C14) sát IC. Không để chân vào U3 thả nổi.', 's mut');
    return svg(820, 460, b);
  }

  // ================= Sơ đồ B =================
  function schB() {
    let b = '';
    b += T(20, 24, 'B · VI SAI + CẦU BTL 2 × TDA2030A → PIEZO CHẤP HÀNH', 'l');
    b += K.port(50, 100, 'GPIO25', true) + K.R(50, 100, 'h', 'R9', '20k') + K.w([110, 100], [180, 100]);
    b += K.dot(130, 100) + K.R(130, 100, 'v', 'R10', '11k') + K.gnd(130, 160);
    b += K.op(180, 112, 'U3C', true, ['10', '9', '8']);
    b += K.port(50, 200, 'VREF', true) + K.R(50, 200, 'h', 'R11', '20k') + K.w([110, 200], [165, 200]);
    b += K.w([180, 124], [165, 124], [165, 290]) + K.dot(165, 200) + K.dot(165, 240);
    b += K.R(165, 240, 'h', 'R12', '11k') + K.w([225, 240], [260, 240]);
    b += K.C(165, 290, 'h', 'C6', '4,7n') + K.w([205, 290], [260, 290], [260, 112]) + K.dot(260, 240);
    b += K.w([240, 112], [300, 112]) + K.dot(260, 112) + K.dot(300, 112) + T(282, 104, 'u₁', 'm acc', 'middle');
    // U4
    const amp = (yc, inv, refs) => {
      let s = '';
      const [U, Rin, Rf, Rg, Rz, Cz, Da, Db, Ro, Rb, OUT] = refs;
      s += K.op(420, yc, U, !inv, inv ? ['2', '1', '4'] : ['1', '2', '4'], 64) + T(452, yc + (inv ? -32 : 40), 'TDA2030A', 's mut', 'middle');
      if (!inv) {
        s += K.w([300, 112], [300, yc - 12], [420, yc - 12]) + K.dot(360, yc - 12) + K.R(360, yc - 12, 'v', Rin, '100k') + K.gnd(360, yc + 48);
        s += K.w([420, yc + 12], [405, yc + 12], [405, yc + 60]) + K.dot(405, yc + 60) + K.R(405, yc + 60, 'v', Rg, '1,5k') + K.gnd(405, yc + 120);
        s += K.w([405, yc + 60], [425, yc + 60]) + K.R(425, yc + 60, 'h', Rf, '22k') + K.w([485, yc + 60], [500, yc + 60], [500, yc]);
      } else {
        s += K.w([300, 112], [300, yc - 12], [330, yc - 12]) + K.R(330, yc - 12, 'h', Rin, '1,40k') + K.w([390, yc - 12], [420, yc - 12]);
        s += K.dot(405, yc - 12) + K.w([405, yc - 12], [405, yc - 60], [425, yc - 60]) + K.R(425, yc - 60, 'h', Rf, '22k') + K.w([485, yc - 60], [500, yc - 60], [500, yc]);
        s += K.w([420, yc + 12], [405, yc + 12]) + K.R(405, yc + 12, 'v', Rg, '1,3k') + K.gnd(405, yc + 72);
      }
      s += K.w([484, yc], [580, yc]) + K.dot(500, yc) + K.dot(520, yc) + K.dot(560, yc);
      s += K.R(520, yc, 'v', Rz, '1Ω') + K.C(520, yc + 60, 'v', Cz, '220n') + K.gnd(520, yc + 100);
      s += K.D(560, yc, 560, yc - 40, Da) + K.rail(560, yc - 40, '+15V') + K.D(560, yc + 40, 560, yc, Db) + K.rail(560, yc + 40, '−15V', true);
      s += K.R(580, yc, 'h', Ro, '10Ω') + K.w([640, yc], [760, yc]) + K.dot(660, yc) + T(700, yc - 8, OUT, 'm acc', 'middle');
      s += K.R(660, yc, 'v', Rb, '100k') + K.gnd(660, yc + 60);
      return s;
    };
    b += amp(100, false, ['U4', 'R13', 'R14', 'R15', 'R16', 'C17', 'D5', 'D6', 'R17', 'R28', 'OUT1']);
    b += amp(350, true, ['U5', 'R18', 'R19', 'R20', 'R26', 'C18', 'D7', 'D8', 'R27', 'R29', 'OUT2']);
    b += T(470, 56 + 0, '', 's');
    // piezo + dầm
    b += K.pz(760, 100, 'T2') + K.w([760, 150], [760, 176], [880, 176]);
    b += K.pz(760, 350, 'B1') + K.w([760, 350], [820, 350]) + K.dot(760, 350) + K.pz(820, 350, 'B2');
    b += K.w([760, 400], [760, 418], [880, 418]) + K.w([820, 400], [820, 418]) + K.dot(820, 418);
    b += Rc(876, 90, 10, 350, 'alu') + K.gnd(881, 440) + T(872, 84, 'Dầm = GND', 's', 'end');
    b += T(470, 506, 'U4: chân 5 → +15 V, chân 3 (và đế tản nhiệt) → −15 V. Mỗi chân nguồn 100 µF + 100 nF sát IC (C20–C27).', 's mut', 'middle');
    b += T(470, 524, 'Hệ số tổng: u (±1,5 V) × 0,55 × 15,7 = ±12,95 V mỗi ngõ ra → Va = OUT1 − OUT2 = ±25,9 V.', 's mut', 'middle');
    return svg(920, 540, b);
  }

  // ================= Sơ đồ C =================
  function schC() {
    let b = '';
    const bx = (x, y, w, h, t1, t2, c = 'box') => Rc(x, y, w, h, c, 3) + T(x + w / 2, y + 18, t1, 'm', 'middle') + (t2 ? T(x + w / 2, y + 33, t2, 's mut', 'middle') : '');
    b += T(20, 24, 'C · NGUỒN, ESP32, GIAO DIỆN, KÍCH RUNG', 'l');
    b += bx(20, 50, 80, 44, 'J1', '24 V 2 A') + K.w([100, 72], [130, 72]);
    b += bx(130, 56, 50, 32, 'F1 1A') + K.w([180, 72], [200, 72]) + K.D(200, 72, 250, 72, 'D1 SS34') + K.w([250, 72], [270, 72]);
    b += bx(270, 56, 50, 32, 'SW1') + K.w([320, 72], [380, 72]) + K.dot(345, 72) + K.C(345, 72, 'v', 'C10', '470µ/35V', true) + K.gnd(345, 112);
    b += T(358, 64, '+24 V', 's');
    b += K.w([380, 72], [400, 72]) + K.dot(380, 72) + K.w([380, 72], [380, 140], [400, 140]);
    b += bx(400, 50, 120, 44, 'U2 DC-DC cách ly', '24 V → ±15 V ≥ 6 W');
    b += K.w([520, 60], [600, 60]) + T(605, 64, '+15 V → U3, U4, U5 (C11 100µ)', 's');
    b += K.w([520, 72], [600, 72]) + T(605, 76, 'COM → GND điểm sao', 's');
    b += K.w([520, 84], [600, 84]) + T(605, 88, '−15 V → U3, U4, U5 (C12 100µ)', 's');
    b += bx(400, 120, 120, 44, 'U1 LM2596', 'chỉnh 5,00 V') + K.w([520, 142], [600, 142]) + T(605, 146, '+5 V → ESP32 VIN, PAM8403', 's');
    // ESP32
    const ex = 330, ey = 200, ew = 170, eh = 250;
    b += Rc(ex, ey, ew, eh, 'boxA', 4) + T(ex + ew / 2, ey + 20, 'ESP32 DevKit V1', 'l', 'middle');
    const lp = [['GPIO34', 'U3A · R5 (cảm biến T1)'], ['GPIO35', 'U3D · R25 (giám sát)'], ['GPIO39', 'VR2 10k (3V3–GND)'], ['GPIO25', 'DAC1 → R9 (u)'], ['GPIO26', 'DAC2 → C30 → R30/R31 → SW2'], ['3V3', 'R6, D3, D4, VR2, OLED, ADXL'], ['VIN', '+5 V'], ['GND', 'điểm sao']];
    lp.forEach(([p, t], i) => { const y = ey + 50 + i * 26; b += T(ex + 8, y + 4, p, 's') + K.w([ex, y], [ex - 40, y]) + T(ex - 46, y + 4, t, 's', 'end'); });
    const rp = [['GPIO21 SDA', 'OLED 0x3C · ADXL 0x53'], ['GPIO22 SCL', '(kéo lên 4,7k trên module)'], ['GPIO32', 'S1 ON/OFF → GND'], ['GPIO33', 'S2 MODE → GND'], ['GPIO27', 'S3 GAIN → GND'], ['USB', 'máy tính 921600']];
    rp.forEach(([p, t], i) => { const y = ey + 50 + i * 26; b += T(ex + ew - 8, y + 4, p, 's', 'end') + K.w([ex + ew, y], [ex + ew + 40, y]) + T(ex + ew + 46, y + 4, t, 's'); });
    // kích rung
    b += bx(660, 410, 120, 44, 'PAM8403', 'kênh L, núm âm lượng');
    b += bx(560, 418, 70, 30, 'SW2') + T(595, 462, 'ESP32 / jack J4', 's mut', 'middle');
    b += K.w([630, 433], [660, 433]);
    b += K.w([780, 422], [810, 422]) + K.w([780, 442], [810, 442]) + Rc(810, 416, 14, 32, 'box') + P('M824,416 L842,402 L842,462 L824,448 Z', 'box') + T(832, 476, 'Loa 4 Ω', 's', 'middle');
    b += T(720, 404, 'L+ / L− (không nối L− xuống GND)', 's mut', 'middle');
    b += K.w([ex + 40, ey + eh], [ex + 40, 470], [545, 470], [545, 433], [560, 433]) + T(ex + 46, 466, 'DAC2 (GPIO26)', 's acc');
    b += T(20, 492, 'Mass hình sao: COM của U2, GND LM2596, GND ESP32, GND analog, vỏ cáp T1 và dây dầm gặp nhau tại một terminal trên board 1.', 's mut');
    return svg(880, 505, b);
  }

  const put = (id, s) => { const el = document.getElementById(id); if (el) el.innerHTML = s; };
  put('dwgSide', side()); put('dwgTop', top()); put('dwgBeam', beamLayout()); put('dwgClamp', clamp()); put('dwgStack', stack());
  put('dwgShaker', shaker()); put('dwgBlock', block()); put('dwgFlow', flow()); put('schA', schA()); put('schB', schB()); put('schC', schC());
})();
