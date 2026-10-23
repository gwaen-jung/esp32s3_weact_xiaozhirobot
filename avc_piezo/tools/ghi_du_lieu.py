"""Ghi dữ liệu ESP32 (CSV 800 Hz), chạy thí nghiệm tự động và tính chỉ tiêu.

Cài đặt: pip install pyserial numpy matplotlib
Chạy:
  python ghi_du_lieu.py COM5 ghi 20              # chỉ ghi 20 s
  python ghi_du_lieu.py COM5 cong_huong 13.4 ppf # 10 s OFF -> 10 s ON tại f = 13,4 Hz
  python ghi_du_lieu.py COM5 va_dap ppf          # ghi 8 s, thả bi trong lúc ghi (OFF rồi lặp lại với ON)
"""
import sys, time, serial, numpy as np, matplotlib.pyplot as plt

COLS = "t_ms,y_V,u_V,az_ms2,on,mode,f_exc,exc_on"


def record(ser, sec):
    rows, t0 = [], time.time()
    ser.reset_input_buffer()
    while time.time() - t0 < sec:
        p = ser.readline().decode(errors="ignore").strip().split(",")
        if len(p) == 8:
            try: rows.append([float(v) for v in p])
            except ValueError: pass
    return np.array(rows)


def cmd(ser, s):
    ser.write((s + "\n").encode()); time.sleep(0.05)


def rms(x): return float(np.sqrt(np.mean(x ** 2)))


def decay_time(t, a, frac=0.05):
    """Thời gian từ đỉnh va đập đến khi đường bao còn frac × đỉnh."""
    i0 = int(np.argmax(np.abs(a))); env = np.abs(a[i0:])
    w = 80  # ~ 1 chu kỳ mode 1 tại 800 Hz
    env = np.array([env[i:i + w].max() for i in range(0, len(env) - w)])
    below = np.where(env < frac * np.abs(a[i0]))[0]
    return (t[i0 + below[0]] - t[i0]) if len(below) else float("nan")


def plot(d, title, fname):
    t = (d[:, 0] - d[0, 0]) / 1000; az = d[:, 3]; fs = 800.0
    fig, ax = plt.subplots(3, 1, figsize=(9, 8))
    ax[0].plot(t, az, lw=.7); ax[0].set_ylabel("Gia tốc đầu dầm (m/s²)"); ax[0].set_title(title)
    ax[0].fill_between(t, az.min(), az.max(), where=d[:, 4] > 0, alpha=.15, label="Điều khiển ON"); ax[0].legend()
    ax[1].plot(t, d[:, 2] * 17.2, color="tab:orange", lw=.7); ax[1].set_ylabel("Va ≈ 17,2·u (V)"); ax[1].set_xlabel("Thời gian (s)")
    for on, lab in [(0, "OFF"), (1, "ON")]:
        seg = az[d[:, 4] == on]
        if len(seg) > 256:
            F = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))); f = np.fft.rfftfreq(len(seg), 1 / fs)
            ax[2].semilogy(f, F, label=lab, lw=.8)
    ax[2].set_xlim(0, 300); ax[2].set_xlabel("Tần số (Hz)"); ax[2].set_ylabel("Phổ biên độ"); ax[2].legend()
    plt.tight_layout(); plt.savefig(fname, dpi=150)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    job = sys.argv[2] if len(sys.argv) > 2 else "ghi"
    with serial.Serial(port, 921600, timeout=1) as ser:
        time.sleep(2)
        if job == "ghi":
            d = record(ser, float(sys.argv[3]) if len(sys.argv) > 3 else 20)
        elif job == "cong_huong":
            f = float(sys.argv[3]); law = sys.argv[4] if len(sys.argv) > 4 else "ppf"
            cmd(ser, "c0"); cmd(ser, "m1" if law == "dvf" else "m0"); cmd(ser, f"f{f}"); cmd(ser, "e1")
            time.sleep(10)                         # chờ ổn định
            d_off = record(ser, 10); cmd(ser, "c1"); time.sleep(2)
            d_on = record(ser, 10); cmd(ser, "c0"); cmd(ser, "e0")
            d = np.vstack([d_off, d_on])
            a_off, a_on = rms(d_off[:, 3]), rms(d_on[:, 3])
            print(f"RMS OFF = {a_off:.4f} m/s², RMS ON = {a_on:.4f} m/s²")
            print(f"Mức giảm = {100 * (a_off - a_on) / a_off:.1f} %  = {20 * np.log10(a_off / a_on):.1f} dB  (mục tiêu ≥ 50 % / 6 dB)")
        elif job == "va_dap":
            law = sys.argv[3] if len(sys.argv) > 3 else "ppf"
            res = {}
            for on in (0, 1):
                cmd(ser, "m1" if law == "dvf" else "m0"); cmd(ser, f"c{on}")
                input(f"[{'ON' if on else 'OFF'}] Nhấn Enter rồi thả bi trong 2 s...")
                dd = record(ser, 8); t = (dd[:, 0] - dd[0, 0]) / 1000
                res[on] = decay_time(t, dd[:, 3]); d = dd
                print(f"{'ON' if on else 'OFF'}: thời gian tắt dần 5 % = {res[on]:.2f} s")
            cmd(ser, "c0")
            print(f"Tỉ số OFF/ON = {res[0] / res[1]:.1f} lần (mục tiêu ≥ 3)")
        else:
            raise SystemExit("job = ghi | cong_huong | va_dap")
    np.savetxt("du_lieu_do.csv", d, delimiter=",", header=COLS, comments="")
    plot(d, job, "ket_qua_do.png"); plt.show()


if __name__ == "__main__":
    main()
