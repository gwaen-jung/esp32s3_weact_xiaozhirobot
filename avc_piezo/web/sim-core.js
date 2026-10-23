/* Mô hình mô phỏng hệ AVC dầm công-xôn + piezo (dùng chung cho trình duyệt và Node).
 * Dầm Euler–Bernoulli 3 mode động + phần dư tĩnh mode 4..6, bộ điều khiển số 1 kHz
 * mô phỏng đúng firmware: ADC 12 bit + nhiễu, HPF 1 Hz, PPF Tustin / DVF, bão hòa, DAC 8 bit. */
(function (root) {
  'use strict';
  const BL = [1.87510407, 4.69409113, 7.85475744, 10.99554073, 14.13716839, 17.27875953];

  const BEAM = { E: 69e9, rho: 2700, L: 0.300, b: 0.030, h: 0.0015 };
  const NDYN = 3;                 // số mode tích phân động
  const ZETA = [0.008, 0.010, 0.012];

  // Hệ số điện – cơ (ước lượng, xem tài liệu thiết kế mục "Ngân sách lực")
  const MV = 1.08e-4;             // mô men uốn / 1 V điện áp cầu Va (N·m/V), cặp đĩa trên+dưới
  const KS = 288;                 // hệ số cảm biến (V trên ADC / đơn vị độ dốc)
  const U2VA = 2 * 4.3 * 2;       // u (V quanh Vref) -> vi sai x2 -> TDA x4.3 -> cầu x2 = 17.2
  const VA_RAIL = 26;             // giới hạn phần cứng |Va|
  const FS = 1000, T = 1 / FS;

  const LAYOUTS = {
    // theo báo cáo: 4 đĩa chấp hành 10–64 mm, cảm biến Ø20 tại 70–90 mm
    report: { act: [[0.010, 0.064, 1]], sen: [0.070, 0.090], label: 'Theo báo cáo (cảm biến Ø20 tại 80 mm)' },
    // đề xuất: đĩa trên #1 (10–37 mm) làm cảm biến, 3 đĩa còn lại chấp hành
    fixed: { act: [[0.010, 0.064, 0.5], [0.037, 0.064, 0.5]], sen: [0.0135, 0.0335], label: 'Đề xuất (đĩa trên #1 làm cảm biến)' }
  };

  function beam() {
    const { E, rho, L, b, h } = BEAM;
    const I = b * h * h * h / 12, A = b * h, EI = E * I, m = rho * A * L;
    const w = BL.map(x => x * x / (L * L) * Math.sqrt(EI / (rho * A)));
    const sig = BL.map(x => (Math.cosh(x) + Math.cos(x)) / (Math.sinh(x) + Math.sin(x)));
    const psi = (i, x) => { const k = BL[i] / L, u = k * x; return Math.cosh(u) - Math.cos(u) - sig[i] * (Math.sinh(u) - Math.sin(u)); };
    const dpsi = (i, x) => { const k = BL[i] / L, u = k * x; return k * (Math.sinh(u) + Math.sin(u) - sig[i] * (Math.cosh(u) - Math.cos(u))); };
    return { EI, m, w, psi, dpsi, L };
  }
  const BM = beam();

  function coeffs(layoutKey) {
    const lay = LAYOUTS[layoutKey];
    const Ba = BL.map((_, i) => lay.act.reduce((s, [a, c, wt]) => s + wt * (BM.dpsi(i, c) - BM.dpsi(i, a)), 0));
    const Bs = BL.map((_, i) => BM.dpsi(i, lay.sen[1]) - BM.dpsi(i, lay.sen[0]));
    // độ lợi tĩnh u -> y của từng mode (V/V)
    const H = BL.map((_, i) => U2VA * MV * KS * Ba[i] * Bs[i] / (BM.m * BM.w[i] * BM.w[i]));
    let Dres = 0; for (let i = NDYN; i < BL.length; i++) Dres += H[i];
    const K1 = H[0];
    const H0 = H.reduce((s, x) => s + x, 0);
    return { Ba, Bs, H, K1, H0, Dres, rEff: (H0 - K1) / K1, Gmax: K1 / H0 };
  }

  class Sim {
    constructor(cfg) {
      this.cfg = Object.assign({
        layout: 'fixed', mode: 'off', G: 0.6, dz: 0.08, fPPF: BM.w[0] / 2 / Math.PI, zf: 0.35,
        exc: false, fExc: 13.6, Fexc: 0.012, sat: true, noise: true, substeps: 10
      }, cfg || {});
      this.t = 0; this.rng = 12345;
      this.setLayout(this.cfg.layout);
      this.reset();
    }
    setLayout(k) { this.cfg.layout = k; this.co = coeffs(k); this.resetCtrl(); }
    reset() {
      this.q = new Float64Array(NDYN); this.qd = new Float64Array(NDYN); this.yaa = 0;
      this.t = 0; this.Va = 0; this.u = 0; this.resetCtrl();
    }
    resetCtrl() {
      this.y1 = this.y2 = this.e1 = this.e2 = 0; this.hp = 0; this.rawPrev = 0; this.vel = 0;
      this.computePPF();
    }
    computePPF() {
      const wf = 2 * Math.PI * this.cfg.fPPF, c = wf / Math.tan(wf * T / 2), z = this.cfg.zf;
      this.a0 = c * c + 2 * z * wf * c + wf * wf; this.a1 = -2 * c * c + 2 * wf * wf;
      this.a2 = c * c - 2 * z * wf * c + wf * wf; this.bn = wf * wf;
    }
    gauss() { // Box–Muller với LCG để kết quả lặp lại được
      const r = () => { this.rng = (this.rng * 1664525 + 1013904223) >>> 0; return (this.rng + 0.5) / 4294967296; };
      return Math.sqrt(-2 * Math.log(r())) * Math.cos(2 * Math.PI * r());
    }
    sensorTrue() { let s = 0; for (let i = 0; i < NDYN; i++) s += this.co.Bs[i] * this.q[i]; return KS * s + this.co.Dres * this.Va / U2VA; }
    impulse(J, x) { for (let i = 0; i < NDYN; i++) this.qd[i] += BM.psi(i, x) * J / BM.m; }
    deriv(q, qd, yaa, t, out) {
      const c = this.cfg, F = c.exc ? c.Fexc * Math.sin(2 * Math.PI * c.fExc * t) : 0;
      for (let i = 0; i < NDYN; i++) {
        const w = BM.w[i];
        out.q[i] = qd[i];
        out.qd[i] = -2 * ZETA[i] * w * qd[i] - w * w * q[i] + (BM.psi(i, 0.12) * F + this.co.Ba[i] * MV * this.Va) / BM.m;
      }
      let s = 0; for (let i = 0; i < NDYN; i++) s += this.co.Bs[i] * q[i];
      const y = KS * s + this.co.Dres * this.Va / U2VA;
      out.yaa = (y - yaa) * 2 * Math.PI * 339;  // RC chống chồng phổ 4,7 kΩ / 100 nF
    }
    integrate(dt) {
      const n = NDYN, mk = () => ({ q: new Float64Array(n), qd: new Float64Array(n), yaa: 0 });
      const k1 = mk(), k2 = mk(), k3 = mk(), k4 = mk(), tq = new Float64Array(n), tqd = new Float64Array(n);
      const stage = (k, src, h, t) => {
        for (let i = 0; i < n; i++) { tq[i] = this.q[i] + h * (src ? src.q[i] : 0); tqd[i] = this.qd[i] + h * (src ? src.qd[i] : 0); }
        this.deriv(tq, tqd, this.yaa + h * (src ? src.yaa : 0), t, k);
      };
      stage(k1, null, 0, this.t); stage(k2, k1, dt / 2, this.t + dt / 2); stage(k3, k2, dt / 2, this.t + dt / 2); stage(k4, k3, dt, this.t + dt);
      for (let i = 0; i < n; i++) {
        this.q[i] += dt / 6 * (k1.q[i] + 2 * k2.q[i] + 2 * k3.q[i] + k4.q[i]);
        this.qd[i] += dt / 6 * (k1.qd[i] + 2 * k2.qd[i] + 2 * k3.qd[i] + k4.qd[i]);
      }
      this.yaa += dt / 6 * (k1.yaa + 2 * k2.yaa + 2 * k3.yaa + k4.yaa);
      this.t += dt;
    }
    controlTick() {  // giống controlTask() trong firmware
      const c = this.cfg;
      let v = 1.65 + this.yaa + (c.noise ? 0.004 * this.gauss() : 0);
      v = Math.min(3.3, Math.max(0, v));
      const raw = Math.round(v / 3.3 * 4095) * 3.3 / 4095 - 1.65;
      this.hp = (1 / (1 + 2 * Math.PI * 1 * T)) * (this.hp + raw - this.rawPrev);
      const y = this.hp; let u = 0;
      if (c.mode === 'ppf') {
        const eta = (this.bn * (y + 2 * this.y1 + this.y2) - this.a1 * this.e1 - this.a2 * this.e2) / this.a0;
        this.e2 = this.e1; this.e1 = eta;
        u = (c.G / this.co.K1) * eta;
      } else if (c.mode === 'dvf') {
        const AL = 1 / (1 + 2 * Math.PI * 100 * T);
        this.vel = AL * this.vel + (1 - AL) * (y - this.y1) * FS;
        u = -(2 * c.dz / (this.co.K1 * BM.w[0])) * this.vel;
      } else { this.e1 = this.e2 = 0; this.vel = 0; }
      if (c.sat) u = Math.max(-1.5, Math.min(1.5, u));
      const code = Math.max(0, Math.min(255, Math.round((1.65 + u) / 3.3 * 255)));
      const uq = c.mode === 'off' ? 0 : code * 3.3 / 255 - 1.65;
      this.u = uq; this.y = y;
      this.Va = Math.max(-VA_RAIL, Math.min(VA_RAIL, U2VA * uq));
      this.y2 = this.y1; this.y1 = y; this.rawPrev = raw;
    }
    advance(seconds, onSample) {
      const N = Math.round(seconds * FS), sub = this.cfg.substeps;
      for (let k = 0; k < N; k++) {
        this.controlTick();
        for (let s = 0; s < sub; s++) this.integrate(T / sub);
        if (onSample) onSample(this);
      }
    }
    tip(x = 0.29) { let s = 0; for (let i = 0; i < NDYN; i++) s += BM.psi(i, x) * this.q[i]; return s; }
    tipAcc(x = 0.29) {
      const c = this.cfg, F = c.exc ? c.Fexc * Math.sin(2 * Math.PI * c.fExc * this.t) : 0; let s = 0;
      for (let i = 0; i < NDYN; i++) {
        const w = BM.w[i];
        const a = -2 * ZETA[i] * w * this.qd[i] - w * w * this.q[i] + (BM.psi(i, 0.12) * F + this.co.Ba[i] * MV * this.Va) / BM.m;
        s += BM.psi(i, x) * a;
      }
      return s;
    }
    shape(x) { let s = 0; for (let i = 0; i < NDYN; i++) s += BM.psi(i, x) * this.q[i]; return s; }
  }

  // Phát hiện mất ổn định giống firmware: loa tắt mà u vẫn bão hòa > 20 % thời gian trong 3 cửa sổ 1 s liên tiếp.
  // Va đập mạnh nhất (bi Ø10) chỉ làm bão hòa khoảng 1,2 s nên không bị tắt nhầm.
  class TripDetector {
    constructor() { this.reset(); }
    reset() { this.n = 0; this.s2 = 0; this.sat = 0; this.prev = 0; this.stuck = 0; }
    tick(y, u, ctrlOn, excOn) {
      this.s2 += y * y; if (Math.abs(u) >= 1.49) this.sat++;
      if (++this.n < FS) return false;
      const r = Math.sqrt(this.s2 / this.n), satF = this.sat / this.n;
      if (ctrlOn && !excOn && satF > 0.2) this.stuck++; else this.stuck = 0;
      this.prev = r; this.n = 0; this.s2 = 0; this.sat = 0;
      return this.stuck >= 3;
    }
  }

  // ---- Phân tích offline (dùng cho bảng chỉ tiêu) ----
  function decayTime(cfg, J = 1.0e-3, dur = 6) {
    const s = new Sim(Object.assign({}, cfg, { exc: false }));
    s.advance(0.05); s.impulse(J, 0.26);
    const xs = []; s.advance(dur, o => xs.push(o.tip()));
    const win = 80; let peak = 0; const env = [];
    for (let i = 0; i < xs.length; i++) { const a = Math.abs(xs[i]); env.push(a); if (a > peak) peak = a; }
    let last = 0; const thr = 0.05 * peak;
    for (let i = 0; i < env.length; i++) if (env[i] > thr) last = i;
    const tEnd = Math.max(...env.slice(-win));
    return { t: (last + 1) / FS, peak, unstable: cfg.mode !== 'off' && tEnd > 0.2 * peak };
  }
  function steadyRMS(cfg, f, Fexc, settle = 10, meas = 2) {
    const s = new Sim(Object.assign({}, cfg, { exc: true, fExc: f, Fexc }));
    s.advance(settle); let acc = 0, n = 0, vmax = 0;
    s.advance(meas, o => { const a = o.tipAcc(); acc += a * a; n++; vmax = Math.max(vmax, Math.abs(o.Va)); });
    return { rms: Math.sqrt(acc / n), tipAmp: null, vmax };
  }

  // FRF giải tích gần đúng (mm/N tại x = 290 mm do lực tại 120 mm), có trễ ZOH nửa mẫu
  function frf(cfg, f) {
    const co = coeffs(cfg.layout), w = 2 * Math.PI * f, j = (re, im) => ({ re, im });
    const add = (a, b) => j(a.re + b.re, a.im + b.im), mul = (a, b) => j(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
    const div = (a, b) => { const d = b.re * b.re + b.im * b.im; return j((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d); };
    let Pt = j(0, 0), Pty = j(0, 0), Pyf = j(0, 0), Pyv = j(co.Dres / U2VA, 0), Ptv = j(0, 0);
    for (let i = 0; i < NDYN; i++) {
      const g = div(j(1 / BM.m, 0), j(BM.w[i] ** 2 - w * w, 2 * ZETA[i] * BM.w[i] * w));
      Pt = add(Pt, mul(g, j(BM.psi(i, 0.29) * BM.psi(i, 0.12), 0)));
      Ptv = add(Ptv, mul(g, j(BM.psi(i, 0.29) * co.Ba[i] * MV, 0)));
      Pyf = add(Pyf, mul(g, j(KS * co.Bs[i] * BM.psi(i, 0.12), 0)));
      Pyv = add(Pyv, mul(g, j(KS * co.Bs[i] * co.Ba[i] * MV, 0)));
    }
    if (cfg.mode === 'off') return Math.hypot(Pt.re, Pt.im) * 1000;
    const zinv = j(Math.cos(w * T), -Math.sin(w * T));
    let K;
    if (cfg.mode === 'ppf') {
      const wf = 2 * Math.PI * cfg.fPPF, c = wf / Math.tan(wf * T / 2), z = cfg.zf ?? 0.35;
      const a0 = c * c + 2 * z * wf * c + wf * wf, a1 = -2 * c * c + 2 * wf * wf, a2 = c * c - 2 * z * wf * c + wf * wf;
      const z2 = mul(zinv, zinv);
      const num = mul(j(wf * wf, 0), add(add(j(1, 0), mul(j(2, 0), zinv)), z2));
      const den = add(add(j(a0, 0), mul(j(a1, 0), zinv)), mul(j(a2, 0), z2));
      K = mul(div(num, den), j(cfg.G / co.K1, 0));
    } else {
      const AL = 1 / (1 + 2 * Math.PI * 100 * T);
      const d = mul(j(FS * (1 - AL), 0), add(j(1, 0), mul(j(-1, 0), zinv)));
      K = mul(div(d, add(j(1, 0), mul(j(-AL, 0), zinv))), j(-(2 * cfg.dz / (co.K1 * BM.w[0])), 0));
    }
    const aa = div(j(1, 0), j(1, w / (2 * Math.PI * 339)));
    const zoh = j(Math.cos(w * T / 2), -Math.sin(w * T / 2));
    const Kc = mul(mul(mul(K, aa), zoh), j(U2VA, 0));      // y -> Va
    // Va = Kc*y ; y = Pyf*F + Pyv*Va/... (Pyv trả về y theo Va)
    const Va_F = div(mul(Kc, Pyf), add(j(1, 0), mul(j(-1, 0), mul(Kc, Pyv))));
    const tipF = add(Pt, mul(Ptv, Va_F));
    return Math.hypot(tipF.re, tipF.im) * 1000;
  }

  const AVC = { TripDetector, BEAM, BM, ZETA, MV, KS, U2VA, FS, LAYOUTS, coeffs, Sim, decayTime, steadyRMS, frf, NDYN };
  if (typeof module !== 'undefined') module.exports = AVC; else root.AVC = AVC;
})(typeof window !== 'undefined' ? window : globalThis);
