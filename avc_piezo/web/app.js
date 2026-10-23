/* Giao diện mô phỏng: mô hình 3D, điều khiển, biểu đồ thời gian thực, OLED, bảng kiểm tra mục tiêu */
(function () {
  'use strict';
  const A = window.AVC, $ = id => document.getElementById(id);
  const tok = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
  const vn = (x, d = 2) => x.toLocaleString('vi-VN', { minimumFractionDigits: d, maximumFractionDigits: d });
  const F1 = A.BM.w[0] / 2 / Math.PI, F2 = A.BM.w[1] / 2 / Math.PI;
  const GP = [0.2, 0.4, 0.6, 0.8], GD = [0.02, 0.05, 0.08, 0.12];
  const BALL = { 6: { J: 1.0e-3, h: 30, r: 3 }, 10: { J: 6.1e-3, h: 50, r: 5 } };

  const ui = { mode: 'off', gi: 1, layout: 'fixed', exc: true, f: +F1.toFixed(1), F: 0.008, exag: 10, speed: 1, trip: true, tripped: false };
  const sim = new A.Sim({ layout: 'fixed', fPPF: F1 });
  const trip = new A.TripDetector();
  function applyCfg() {
    const c = sim.cfg;
    c.mode = ui.mode; c.G = GP[ui.gi]; c.dz = GD[ui.gi]; c.exc = ui.exc; c.fExc = ui.f; c.Fexc = ui.F;
  }
  applyCfg();

  // ---------------- bộ đệm dữ liệu (500 Hz, 6 s) ----------------
  const N = 3000, buf = { acc: new Float32Array(N), va: new Float32Array(N), on: new Uint8Array(N), i: 0 };
  let k = 0, rng = 7;
  const noise = () => { rng = (rng * 1103515245 + 12345) & 0x7fffffff; return (rng / 0x7fffffff - 0.5) * 2; };
  const impact = { active: false, t0: 0, peak: 0, last: 0, J: 0, ref: null };
  function onSample(o) {
    if (ui.trip && trip.tick(o.y, o.u, ui.mode !== 'off', ui.exc)) { ui.tripped = true; setMode('off', true); }
    if (impact.active) {
      const a = Math.abs(o.tip());
      if (a > impact.peak) impact.peak = a;
      if (a > 0.05 * impact.peak) impact.last = o.t;
      if (o.t - impact.last > 1.2 || o.t - impact.t0 > 9) { impact.active = false; showDecay(); }
    }
    if (++k % 2) return;
    const j = buf.i % N;
    const az = o.tipAcc() + 0.03 * noise();
    buf.acc[j] = Math.round(az / 0.0383) * 0.0383; buf.va[j] = o.Va; buf.on[j] = ui.mode !== 'off' ? 1 : 0; buf.i++;
  }
  function showDecay() {
    const t = impact.last - impact.t0;
    $('rDecay').textContent = (impact.active ? '… ' : '') + vn(t) + ' s';
    if (impact.ref) $('rDecayOff').textContent = vn(impact.ref) + ' s' + (ui.mode !== 'off' && !impact.active ? ` · ${vn(impact.ref / Math.max(t, 0.01), 1)}×` : '');
  }

  // ---------------- điều khiển giao diện ----------------
  function seg(id, val) { $(id).querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', b.dataset.v === String(val))); }
  function setMode(m, fromTrip) {
    ui.mode = m; if (!fromTrip) ui.tripped = false;
    seg('segMode', m); applyCfg(); if (m === 'off') sim.resetCtrl(); trip.reset(); gainText(); chips(); drawFrf();
  }
  function gainText() {
    const g = GP[ui.gi].toFixed(2).replace('.', ','), d = GD[ui.gi].toFixed(2).replace('.', ',');
    $('gainTxt').textContent = ui.mode === 'ppf' ? `G = ${g}` : ui.mode === 'dvf' ? `Δζ = ${d}` : `PPF G = ${g} · DVF Δζ = ${d}`;
  }
  $('segMode').addEventListener('click', e => { const b = e.target.closest('button'); if (b) setMode(b.dataset.v); });
  $('segGain').addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; ui.gi = +b.dataset.v; seg('segGain', ui.gi); applyCfg(); trip.reset(); gainText(); drawFrf(); });
  $('segLayout').addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b) return; ui.layout = b.dataset.v; seg('segLayout', ui.layout);
    sim.setLayout(ui.layout); trip.reset(); refreshRef(); drawFrf(); runChecks();
  });
  const fIn = $('fExc'), aIn = $('aExc');
  function setF(v) { ui.f = +(+v).toFixed(1); fIn.value = ui.f; $('fTxt').textContent = vn(ui.f, 1) + ' Hz'; applyCfg(); refreshRef(); drawFrf(); }
  fIn.addEventListener('input', () => setF(fIn.value));
  aIn.addEventListener('input', () => { ui.F = aIn.value / 1000; $('aTxt').textContent = vn(+aIn.value, 1) + ' mN'; applyCfg(); refreshRef(); });
  $('btnF1').onclick = () => setF(F1); $('btnF2').onclick = () => setF(F2);
  function setExc(on) { ui.exc = on; $('btnExc').textContent = on ? 'Tắt loa' : 'Bật loa'; applyCfg(); trip.reset(); refreshRef(); }
  $('btnExc').onclick = () => setExc(!ui.exc);
  $('exag').onchange = e => { ui.exag = +e.target.value; $('exagLbl').textContent = 'phóng đại ×' + ui.exag; };
  $('speed').onchange = e => ui.speed = +e.target.value;
  $('trip').onchange = e => { ui.trip = e.target.checked; trip.reset(); };
  $('btnBall6').onclick = () => dropBall(6); $('btnBall10').onclick = () => dropBall(10);

  // tham chiếu OFF cùng kích thích
  let refRms = null, refTimer = 0;
  function refreshRef() {
    refRms = null; clearTimeout(refTimer);
    if (!ui.exc) return;
    refTimer = setTimeout(() => { refRms = A.steadyRMS({ layout: ui.layout, mode: 'off', fPPF: F1 }, ui.f, ui.F, 10, 2).rms; }, 250);
  }

  // ---------------- 3D ----------------
  let three = null;
  try { three = build3D(); } catch (err) { $('vp').insertAdjacentHTML('beforeend', '<p class="mutedtxt" style="position:absolute;inset:40% 16px auto;text-align:center">Trình duyệt không hỗ trợ WebGL. Biểu đồ và bảng vẫn hoạt động.</p>'); }

  function build3D() {
    const T3 = window.THREE; if (!T3) throw new Error('no three');
    const canvas = $('three'), renderer = new T3.WebGLRenderer({ canvas, antialias: true });
    renderer.setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
    const scene = new T3.Scene(), cam = new T3.PerspectiveCamera(32, 1.6, 1, 6000);
    scene.add(new T3.HemisphereLight(0xffffff, 0x50585c, 0.85));
    const dl = new T3.DirectionalLight(0xffffff, 0.75); dl.position.set(-200, 600, -500); scene.add(dl);
    const dl2 = new T3.DirectionalLight(0xffffff, 0.25); dl2.position.set(500, 200, 400); scene.add(dl2);
    const M = (c, o = {}) => new T3.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.55, metalness: 0.1 }, o));
    const mAlu = M(0xb3bcc0, { metalness: 0.55, roughness: 0.35 }), mSteel = M(0x6d757a, { metalness: 0.6, roughness: 0.4 });
    const mBrass = M(0xc0913a, { metalness: 0.6, roughness: 0.35 }), mWood = M(0xb58b5c, { roughness: 0.85 });
    const mBox = M(0x2f3539, { roughness: 0.7 }), mPcb = M(0x6a3fb5), mBlack = M(0x15181a);
    const mSense = M(0x16788a, { emissive: 0x000000 });
    const mGlass = new T3.MeshStandardMaterial({ color: 0xbfe6ee, transparent: true, opacity: 0.25, roughness: 0.1 });
    const add = (geo, mat, x, y, z, parent = scene) => { const m = new T3.Mesh(geo, mat); m.position.set(x, y, z); parent.add(m); return m; };
    const box = (w, h, d) => new T3.BoxGeometry(w, h, d), cyl = (r, h, s = 32) => new T3.CylinderGeometry(r, r, h, s);

    add(box(450, 18, 200), mWood, 225, -9, -30);
    [[15, 60], [435, 60], [15, -120], [435, -120]].forEach(([x, z]) => add(cyl(8, 6), mBlack, x, -21, z));
    add(box(50, 80, 50), mAlu, 45, 40, 0);
    add(box(50, 10, 50), mSteel, 45, 86.5, 0);
    [-20, 20].forEach(z => { add(cyl(5, 6, 6), mSteel, 45, 94.5, z); });
    // dầm biến dạng
    const segs = 175;
    const beamGeo = new T3.BoxGeometry(350, 1.5, 30, segs, 1, 1);
    const beam = add(beamGeo, mAlu, 195, 80.75, 0);
    const pos = beamGeo.attributes.position, y0 = new Float32Array(pos.count), sx = new Float32Array(pos.count);
    for (let i = 0; i < pos.count; i++) { y0[i] = pos.getY(i); sx[i] = pos.getX(i) + 195 - 70; }
    // các phần tử gắn trên dầm
    const att = [];
    const mount = (obj, s, yOff) => { att.push({ obj, s, yOff }); scene.add(obj); return obj; };
    const disc = (s, top, cer, r = 13.5, rc = 10) => {
      const g = new T3.Group();
      add(cyl(r, 0.5), mBrass, 0, top ? 0.25 : -0.25, 0, g);
      if (cer) add(cyl(rc, 0.7), cer, 0, top ? 0.85 : -0.85, 0, g);
      return mount(g, s, top ? 81.5 : 80);
    };
    const actMats = [M(0xd9d4c3, { emissive: 0x000000 }), M(0xd9d4c3, { emissive: 0x000000 }), M(0xd9d4c3, { emissive: 0x000000 })];
    disc(23.5, true, mSense); disc(51, true, actMats[0]); disc(23.5, false, actMats[1]); disc(51, false, actMats[2]);
    disc(80, true, M(0xd9d4c3), 10, 7);
    const adxl = new T3.Group(); add(box(20, 1.6, 15), mPcb, 0, 0.8, 0, adxl); add(box(5, 1, 5), mBlack, 0, 2.1, 0, adxl); mount(adxl, 290, 81.5);
    const mag = new T3.Group(); add(cyl(4, 3), mSteel, 0, -1.5, 0, mag); mount(mag, 120, 80);
    // loa + que đẩy
    add(box(80, 18, 70), mGlass, 190, 9, 0);
    add(new T3.CylinderGeometry(33, 24, 30, 32), mBlack, 190, 33, 0);
    const cone = add(new T3.CylinderGeometry(28, 28, 1.5, 32), M(0x3a3f44), 190, 48.5, 0);
    const stinger = add(cyl(1.5, 26, 12), mBlack, 190, 63, 0);
    // ống bi
    add(new T3.CylinderGeometry(6, 6, 100, 24, 1, true), mGlass, 330, 161.5, 0);
    add(cyl(2, 220, 12), mAlu, 330, 110, 52); add(box(4, 4, 52), mAlu, 330, 180, 26);
    const ball = add(new T3.SphereGeometry(1, 20, 14), M(0xd5d9db, { metalness: 0.9, roughness: 0.15 }), 330, 200, 0);
    // hộp mạch
    add(box(150, 60, 100), mBox, 325, 30, -75);
    const oledTex = new T3.CanvasTexture($('oled')); oledTex.magFilter = T3.NearestFilter; oledTex.minFilter = T3.LinearFilter;
    add(box(46, 26, 1), mBlack, 282, 38, -25.2);
    add(new T3.PlaneGeometry(40, 20), new T3.MeshBasicMaterial({ map: oledTex }), 282, 38, -24.6);
    const btnM = [M(0xc2571a), M(0x16788a), M(0xd0d4d6)];
    [320, 345, 370].forEach((x, i) => { const b = add(cyl(5, 4, 20), btnM[i], x, 22, -23); b.rotation.x = Math.PI / 2; });
    const knob = add(cyl(6, 8, 20), mSteel, 395, 40, -21); knob.rotation.x = Math.PI / 2;
    // dây
    const wireMat = new T3.LineBasicMaterial({ color: 0x9a6b2d });
    const curve = new T3.CatmullRomCurve3([new T3.Vector3(72, 84, -6), new T3.Vector3(90, 70, -30), new T3.Vector3(200, 40, -40), new T3.Vector3(255, 30, -40)]);
    scene.add(new T3.Line(new T3.BufferGeometry().setFromPoints(curve.getPoints(40)), wireMat));

    // camera quỹ đạo
    const views = {
      persp: { t: [215, 60, -20], th: -0.62, ph: 0.42, r: 720 },
      side: { t: [215, 90, 0], th: 0, ph: 0.02, r: 620 },
      root: { t: [115, 82, 0], th: -0.45, ph: 0.38, r: 210 },
      top: { t: [225, 0, -30], th: 0, ph: 1.45, r: 760 }
    };
    const orb = Object.assign({}, views.persp, { t: views.persp.t.slice() }), goal = Object.assign({}, views.persp, { t: views.persp.t.slice() });
    document.querySelectorAll('[data-view]').forEach(b => b.onclick = () => { const v = views[b.dataset.view]; Object.assign(goal, v, { t: v.t.slice() }); });
    let drag = null; const pts = new Map();
    canvas.addEventListener('pointerdown', e => { canvas.setPointerCapture(e.pointerId); pts.set(e.pointerId, [e.clientX, e.clientY]); drag = { x: e.clientX, y: e.clientY, d: null }; });
    canvas.addEventListener('pointermove', e => {
      if (!pts.has(e.pointerId)) return; pts.set(e.pointerId, [e.clientX, e.clientY]);
      if (pts.size === 2) { const [p, q] = [...pts.values()]; const d = Math.hypot(p[0] - q[0], p[1] - q[1]); if (drag.d) goal.r = Math.min(2500, Math.max(80, goal.r * drag.d / d)); drag.d = d; return; }
      goal.th -= (e.clientX - drag.x) * 0.006; goal.ph = Math.min(1.5, Math.max(-0.2, goal.ph + (e.clientY - drag.y) * 0.005)); drag.x = e.clientX; drag.y = e.clientY;
    });
    const up = e => { pts.delete(e.pointerId); if (!pts.size) drag = null; else if (drag) drag.d = null; };
    canvas.addEventListener('pointerup', up); canvas.addEventListener('pointercancel', up);
    canvas.addEventListener('wheel', e => { e.preventDefault(); goal.r = Math.min(2500, Math.max(80, goal.r * Math.exp(e.deltaY * 0.001))); }, { passive: false });

    const resize = () => { const r = canvas.parentElement.getBoundingClientRect(); renderer.setSize(r.width, r.height, false); cam.aspect = r.width / Math.max(1, r.height); cam.updateProjectionMatrix(); };
    new ResizeObserver(resize).observe(canvas.parentElement); resize();
    const setBg = () => scene.background = new T3.Color(tok('--grid') || '#dfe4e0');
    setBg();
    let visible = true; new IntersectionObserver(es => visible = es[0].isIntersecting).observe(canvas);

    const psiCache = [0, 1, 2].map(i => Float32Array.from(sx, s => s > 0 ? A.BM.psi(i, s / 1000) : 0));
    const wAt = s => s <= 0 ? 0 : sim.shape(s / 1000) * 1000;
    function frame(ballState) {
      if (!visible) return;
      for (let key of ['th', 'ph', 'r']) orb[key] += (goal[key] - orb[key]) * 0.15;
      for (let i = 0; i < 3; i++) orb.t[i] += (goal.t[i] - orb.t[i]) * 0.15;
      cam.position.set(orb.t[0] + orb.r * Math.cos(orb.ph) * Math.sin(orb.th), orb.t[1] + orb.r * Math.sin(orb.ph), orb.t[2] + orb.r * Math.cos(orb.ph) * Math.cos(orb.th));
      cam.lookAt(orb.t[0], orb.t[1], orb.t[2]);
      const ex = ui.exag, q = sim.q;
      for (let i = 0; i < pos.count; i++) pos.setY(i, y0[i] + ex * 1000 * (psiCache[0][i] * q[0] + psiCache[1][i] * q[1] + psiCache[2][i] * q[2]));
      pos.needsUpdate = true; beamGeo.computeVertexNormals();
      att.forEach(({ obj, s, yOff }) => {
        const w = ex * wAt(s), sl = ex * (wAt(s + 1) - wAt(s - 1)) / 2;
        obj.position.set(70 + s, yOff + w, 0); obj.rotation.z = Math.atan(sl);
      });
      const w120 = ex * wAt(120); cone.position.y = 48.5 + w120; stinger.position.y = 63 + w120;
      const v = sim.Va / 26, hot = new T3.Color(0xff7a2a), cold = new T3.Color(0x2a8cff);
      actMats[0].emissive.copy(v > 0 ? hot : cold).multiplyScalar(Math.abs(v) * 0.9);
      actMats[1].emissive.copy(v > 0 ? cold : hot).multiplyScalar(Math.abs(v) * 0.9);
      actMats[2].emissive.copy(actMats[1].emissive);
      if (ballState) { ball.visible = ballState.vis; ball.scale.setScalar(ballState.r); ball.position.set(330 + ballState.dx, ballState.y, 0); }
      renderer.render(scene, cam);
    }
    return { frame, oledTex, setBg };
  }

  // ---------------- thả bi ----------------
  const ballAnim = { phase: 'idle', y: 200, r: 3, vis: true, dx: 0, t: 0, d: 6 };
  function dropBall(d) {
    if (ballAnim.phase !== 'idle') return;
    Object.assign(ballAnim, { phase: 'fall', d, r: BALL[d].r, t: 0, dx: 0, vis: true, y0: 83 + BALL[d].h + BALL[d].r });
    ballAnim.y = ballAnim.y0;
  }
  function stepBall(dt) {   // dt: giây mô phỏng
    const b = ballAnim, g = 9810;
    if (b.phase === 'idle') { b.y = 200; b.r = 3; b.dx = 0; b.vis = true; return; }
    b.t += dt;
    if (b.phase === 'fall') {
      b.y = b.y0 - 0.5 * g * b.t * b.t;
      const contact = 81.5 + b.r + ui.exag * sim.shape(0.26) * 1000;
      if (b.y <= contact) {
        sim.impulse(BALL[b.d].J, 0.26);
        Object.assign(impact, { active: true, t0: sim.t, peak: 0, last: sim.t, J: BALL[b.d].J, ref: null });
        $('rDecay').textContent = '…'; $('rDecayOff').textContent = '…';
        const J = BALL[b.d].J, lay = ui.layout; setTimeout(() => { impact.ref = A.decayTime({ layout: lay, mode: 'off', fPPF: F1 }, J, 7).t; showDecay(); }, 30);
        b.phase = 'bounce'; b.t = 0; b.v = 0.5 * Math.sqrt(2 * g * BALL[b.d].h); b.yc = b.y;
      }
    } else if (b.phase === 'bounce') {
      b.y = b.yc + b.v * b.t - 0.5 * g * b.t * b.t; b.dx = 120 * b.t;
      if (b.t > 0.35) { b.vis = false; b.phase = 'wait'; b.t = 0; }
    } else if (b.phase === 'wait' && b.t > 0.6) b.phase = 'idle';
  }

  // ---------------- biểu đồ ----------------
  function setupCanvas(c) {
    const r = c.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
    if (c.width !== Math.round(r.width * dpr) || c.height !== Math.round(r.height * dpr)) { c.width = Math.round(r.width * dpr); c.height = Math.round(r.height * dpr); }
    const ctx = c.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0); return { ctx, w: r.width, h: r.height };
  }
  const nice = v => { const p = Math.pow(10, Math.floor(Math.log10(v))), m = v / p; return (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p; };
  let colors = {};
  const readColors = () => colors = { ink: tok('--ink'), ink2: tok('--ink2'), rule: tok('--rule'), grid: tok('--grid'), off: tok('--off'), on: tok('--sense'), act: tok('--act'), bad: tok('--bad') };
  readColors();
  const scale = { acc: 5 };
  function drawTS(c, arr, opts) {
    const { ctx, w, h } = setupCanvas(c), L = 44, R = 16, Tp = 8, B = 20, pw = w - L - R, ph = h - Tp - B;
    ctx.clearRect(0, 0, w, h);
    const n = Math.min(buf.i, N), start = buf.i - n;
    let ymax = opts.fixed;
    if (!ymax) { let m = 0; for (let i = 0; i < n; i++) m = Math.max(m, Math.abs(arr[(start + i) % N])); scale.acc = Math.max(m * 1.15, scale.acc * 0.985, 0.5); ymax = scale.acc; }
    const step = nice(ymax / 2), top = Math.ceil(ymax / step) * step;
    const X = i => L + pw * (i + (N - n)) / N, Y = v => Tp + ph / 2 - v / top * ph / 2;
    // vùng ON
    ctx.fillStyle = colors.on; ctx.globalAlpha = 0.08;
    for (let i = 0; i < n;) { if (buf.on[(start + i) % N]) { let j = i; while (j < n && buf.on[(start + j) % N]) j++; ctx.fillRect(X(i), Tp, X(j) - X(i), ph); i = j; } else i++; }
    ctx.globalAlpha = 1;
    ctx.strokeStyle = colors.rule; ctx.lineWidth = 1; ctx.fillStyle = colors.ink2; ctx.font = '11px "JetBrains Mono", monospace'; ctx.textAlign = 'right';
    for (let v = -top; v <= top + 1e-9; v += step) { const y = Y(v); ctx.beginPath(); ctx.moveTo(L, y); ctx.lineTo(L + pw, y); ctx.stroke(); ctx.fillText(vn(v, step < 1 ? 1 : 0), L - 6, y + 4); }
    ctx.textAlign = 'center';
    for (let s = 0; s <= 6; s++) { const x = L + pw * s / 6; ctx.fillText((s - 6) + ' s', x, h - 5); }
    if (opts.limits) { ctx.setLineDash([4, 3]); ctx.strokeStyle = colors.bad; opts.limits.forEach(v => { ctx.beginPath(); ctx.moveTo(L, Y(v)); ctx.lineTo(L + pw, Y(v)); ctx.stroke(); }); ctx.setLineDash([]); }
    ctx.lineWidth = 1.4;
    let cur = -1;
    for (let i = 0; i < n; i++) {
      const id = (start + i) % N, on = opts.single ? 2 : buf.on[id];
      if (on !== cur) { if (cur >= 0) ctx.stroke(); ctx.beginPath(); ctx.strokeStyle = on === 2 ? colors.act : on ? colors.on : colors.off; if (i) ctx.moveTo(X(i - 1), Y(arr[(start + i - 1) % N])); cur = on; }
      i === 0 ? ctx.moveTo(X(i), Y(arr[id])) : ctx.lineTo(X(i), Y(arr[id]));
    }
    if (cur >= 0) ctx.stroke();
  }

  function drawFrf() {
    const c = $('cFrf'); if (!c) return;
    const { ctx, w, h } = setupCanvas(c), L = 44, R = 10, Tp = 10, B = 22, pw = w - L - R, ph = h - Tp - B;
    ctx.clearRect(0, 0, w, h);
    const fmax = 110, X = f => L + pw * f / fmax;
    const co = A.coeffs(ui.layout), G = GP[ui.gi], dz = GD[ui.gi];
    const curves = [
      { cfg: { layout: ui.layout, mode: 'off' }, col: colors.off },
      { cfg: { layout: ui.layout, mode: 'ppf', G, fPPF: F1, zf: 0.35 }, col: colors.act, bad: G >= co.Gmax },
      { cfg: { layout: ui.layout, mode: 'dvf', dz, fPPF: F1 }, col: colors.on }
    ];
    const pts = curves.map(cv => { const a = []; for (let f = 1; f <= fmax; f += 0.1) a.push([f, 20 * Math.log10(A.frf(cv.cfg, f))]); return a; });
    const ymax = Math.ceil(Math.max(...pts[0].map(p => p[1])) / 10) * 10 + 5, ymin = ymax - 75;
    const Y = d => Tp + ph * (ymax - d) / (ymax - ymin);
    ctx.strokeStyle = colors.rule; ctx.fillStyle = colors.ink2; ctx.font = '11px "JetBrains Mono", monospace'; ctx.lineWidth = 1; ctx.textAlign = 'right';
    for (let d = Math.ceil(ymin / 15) * 15; d <= ymax; d += 15) { ctx.beginPath(); ctx.moveTo(L, Y(d)); ctx.lineTo(L + pw, Y(d)); ctx.stroke(); ctx.fillText(d, L - 6, Y(d) + 4); }
    ctx.textAlign = 'center';
    for (let f = 0; f <= fmax; f += 20) { ctx.fillText(f + ' Hz', X(f), h - 6); }
    ctx.setLineDash([3, 3]); ctx.strokeStyle = colors.ink2; ctx.beginPath(); ctx.moveTo(X(ui.f), Tp); ctx.lineTo(X(ui.f), Tp + ph); ctx.stroke(); ctx.setLineDash([]);
    ctx.save(); ctx.beginPath(); ctx.rect(L, Tp, pw, ph); ctx.clip();
    pts.forEach((a, i) => {
      if (curves[i].bad) return;
      ctx.strokeStyle = curves[i].col; ctx.lineWidth = i ? 2 : 1.6; ctx.beginPath();
      a.forEach(([f, d], j) => j ? ctx.lineTo(X(f), Y(d)) : ctx.moveTo(X(f), Y(d))); ctx.stroke();
    });
    ctx.restore();
    const pk = a => Math.max(...a.filter(p => p[0] < 40).map(p => p[1])), pk2 = a => Math.max(...a.filter(p => p[0] > 60 && p[0] < 110).map(p => p[1]));
    const r1p = pk(pts[0]) - pk(pts[1]), r1d = pk(pts[0]) - pk(pts[2]), r2d = pk2(pts[0]) - pk2(pts[2]);
    $('frfNote').textContent = curves[1].bad ? `PPF G = ${vn(G)} ≥ G_max ≈ ${vn(co.Gmax)}: hệ mất ổn định, không vẽ FRF PPF` :
      `Đỉnh mode 1: PPF −${vn(r1p, 1)} dB · DVF −${vn(r1d, 1)} dB · mode 2 với DVF: ${r2d >= 0 ? '−' : '+'}${vn(Math.abs(r2d), 1)} dB · G_max PPF ≈ ${vn(co.Gmax)}`;
  }

  // ---------------- OLED ----------------
  const oc = $('oled').getContext('2d');
  function drawOled(rms, tipmm) {
    oc.fillStyle = '#000'; oc.fillRect(0, 0, 128, 64);
    oc.fillStyle = '#9fe8ff'; oc.font = '8px monospace'; oc.textBaseline = 'top';
    const m = ui.mode === 'off' ? 'OFF' : 'ON ', law = ui.mode === 'dvf' ? 'DVF' : 'PPF';
    oc.fillText(`AVC ${law} ${m} G${Math.min(ui.gi, 2) + 1}`, 0, 0);
    oc.fillText(`f1=${F1.toFixed(2)}`, 0, 10);
    oc.fillText(`Loa:${ui.exc ? 'ON ' : 'OFF'} ${ui.f.toFixed(1)}Hz`, 0, 20);
    oc.fillText(ui.tripped ? '!TU TAT: MAT ON DINH' : `y=${Math.abs(sim.y || 0).toFixed(3)}V u=${(sim.u >= 0 ? '+' : '') + sim.u.toFixed(2)}`, 0, 30);
    oc.font = 'bold 16px monospace'; oc.fillText(rms.toFixed(2), 0, 44); oc.font = '8px monospace'; oc.fillText('m/s2', 60, 52);
    if (three) three.oledTex.needsUpdate = true;
  }

  // ---------------- trạng thái ----------------
  function chips(sat) {
    const cc = $('chipCtrl'), cs = $('chipStab');
    cc.textContent = ui.mode === 'off' ? 'ĐIỀU KHIỂN: OFF' : `ĐIỀU KHIỂN: ${ui.mode.toUpperCase()} · MỨC ${ui.gi === 3 ? 'THỬ' : ui.gi + 1}`;
    cc.className = 'chip ' + (ui.mode === 'off' ? 'dim' : 'ok');
    if (ui.tripped) { cs.textContent = 'TỰ TẮT: MẤT ỔN ĐỊNH'; cs.className = 'chip bad'; }
    else if (sat > 0.2 && ui.mode !== 'off') { cs.textContent = ui.exc ? 'BÃO HÒA ±26 V' : 'BÃO HÒA / NGHI MẤT ỔN ĐỊNH'; cs.className = 'chip warn'; }
    else { cs.textContent = 'ỔN ĐỊNH'; cs.className = 'chip ok'; }
  }

  // ---------------- vòng lặp ----------------
  let last = performance.now(), uiT = 0;
  function loop(now) {
    const dt = Math.min(0.05, (now - last) / 1000); last = now;
    const sdt = dt * ui.speed;
    sim.advance(sdt, onSample); stepBall(sdt);
    if (impact.active) showDecay();
    if (three) three.frame(ballAnim);
    uiT += dt;
    if (uiT > 0.1) {
      uiT = 0;
      drawTS($('cAcc'), buf.acc, {}); drawTS($('cVa'), buf.va, { fixed: 30, limits: [26, -26], single: true });
      const n = Math.min(buf.i, 500); let s2 = 0, vmax = 0, sat = 0;
      for (let i = 1; i <= n; i++) { const j = (buf.i - i) % N; s2 += buf.acc[j] ** 2; vmax = Math.max(vmax, Math.abs(buf.va[j])); if (Math.abs(buf.va[j]) > 25.5) sat++; }
      const rms = n ? Math.sqrt(s2 / n) : 0;
      tipHist.push(Math.abs(sim.tip())); if (tipHist.length > 6) tipHist.shift();
      $('rRms').textContent = vn(rms); $('rVa').textContent = vn(vmax, 1);
      $('rTip').textContent = vn(Math.max(...tipHist) * 1000);
      if (ui.exc && refRms && ui.mode !== 'off') { const r = 1 - rms / refRms; $('rRed').textContent = `${vn(100 * r, 1)} % · ${vn(20 * Math.log10(refRms / Math.max(rms, 1e-4)), 1)} dB`; }
      else $('rRed').textContent = ui.exc ? (ui.mode === 'off' ? 'đang OFF' : '…') : 'loa tắt';
      chips(n ? sat / n : 0); drawOled(rms);
    }
    requestAnimationFrame(loop);
  }
  const tipHist = [];

  // ---------------- bảng kiểm tra mục tiêu ----------------
  let checkGen = 0;
  function runChecks() {
    const gen = ++checkGen, lay = ui.layout, tb = $('tblCheck').tBodies[0];
    tb.innerHTML = '<tr><td colspan="6" class="mutedtxt">Đang tính…</td></tr>';
    const cases = [['OFF', { mode: 'off' }], ['PPF G = 0,2', { mode: 'ppf', G: 0.2 }], ['PPF G = 0,4', { mode: 'ppf', G: 0.4 }], ['PPF G = 0,6', { mode: 'ppf', G: 0.6 }],
      ['DVF Δζ = 0,02', { mode: 'dvf', dz: 0.02 }], ['DVF Δζ = 0,05', { mode: 'dvf', dz: 0.05 }], ['DVF Δζ = 0,08', { mode: 'dvf', dz: 0.08 }]];
    const res = []; let off = null, i = 0;
    const next = () => {
      if (gen !== checkGen) return;
      if (i >= cases.length) return render();
      const [name, c] = cases[i++], cfg = Object.assign({ layout: lay, fPPF: F1 }, c);
      const st = A.steadyRMS(cfg, F1, 0.008, 10, 2), dc = A.decayTime(cfg, 1.0e-3, 7);
      res.push({ name, st, dc, cfg }); if (!off) off = res[0];
      tb.innerHTML = `<tr><td colspan="6" class="mutedtxt">Đang tính ${i}/${cases.length}…</td></tr>`;
      setTimeout(next, 0);
    };
    const render = () => {
      tb.innerHTML = res.map((r, j) => {
        if (!j) return `<tr><td>${r.name}</td><td class="n">RMS ${vn(r.st.rms)} m/s²</td><td class="n">${vn(r.dc.t)} s</td><td class="n">—</td><td class="n">0</td><td class="mutedtxt">tham chiếu</td></tr>`;
        const red = 1 - r.st.rms / off.st.rms, db = 20 * Math.log10(off.st.rms / r.st.rms), ratio = off.dc.t / r.dc.t;
        const unstable = r.dc.unstable || red < 0, ok = !unstable && red >= 0.5 && ratio >= 3 && r.st.vmax <= 30;
        return `<tr><td>${r.name}</td><td class="n">${unstable ? '<span class="fail">mất ổn định</span>' : `${vn(100 * red, 1)} % · ${vn(db, 1)} dB`}</td><td class="n">${unstable ? '—' : vn(r.dc.t) + ' s'}</td><td class="n">${unstable ? '—' : vn(ratio, 1) + '×'}</td><td class="n">${vn(r.st.vmax, 1)} V</td><td class="${ok ? 'pass' : 'fail'}">${ok ? 'Đạt' : 'Không'}</td></tr>`;
      }).join('') + `<tr><td colspan="6" class="mutedtxt">Mục tiêu: giảm ≥ 50 % (6 dB), OFF/ON ≥ 3, |Va| ≤ 30 V. Bố trí: ${A.LAYOUTS[lay].label}.</td></tr>`;
    };
    setTimeout(next, 30);
  }

  // theo dõi đổi giao diện sáng/tối
  const retheme = () => { readColors(); if (three) three.setBg(); drawFrf(); };
  window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', retheme);
  new MutationObserver(retheme).observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });
  window.addEventListener('resize', () => drawFrf());

  setF(ui.f); setExc(true); gainText(); chips(0); drawOled(0);
  requestAnimationFrame(loop); runChecks();
})();
