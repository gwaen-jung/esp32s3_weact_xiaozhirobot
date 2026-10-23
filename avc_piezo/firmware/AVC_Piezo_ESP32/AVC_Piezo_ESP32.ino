/*******************************************************************
 * HỆ THỐNG TRIỆT TIÊU RUNG ĐỘNG CHỦ ĐỘNG BẰNG VẬT LIỆU PIEZO – bản thi công
 * Bộ điều khiển: ESP32 DevKit V1 (ESP32-WROOM-32, có DAC). KHÔNG dùng ESP32-S3/C3 (không có DAC).
 * Arduino core 2.x / 3.x. Thư viện: Adafruit SSD1306, Adafruit GFX (ADXL345 đọc trực tiếp thanh ghi).
 *
 * Thay đổi so với Phụ lục 1 của báo cáo:
 *  - Cảm biến điều khiển = đĩa trên #1 (10–37 mm), đồng vị trí với chấp hành -> DVF không làm mất ổn định mode 2.
 *  - Tự nhận dạng: f1 (gõ nhẹ dầm), độ lợi tĩnh H0 và chiều phân cực (SIGN) bằng sin 3 Hz -> hệ số khuếch đại
 *    tính theo tỉ lệ giới hạn ổn định thay vì hằng số thử-sai; lưu NVS.
 *  - ADXL345 đọc FIFO 800 Hz (±16 g, full-res) -> không chồng phổ mode 3, không bão hòa ±4 g.
 *  - DAC2 (GPIO26) phát sin cho loa kích rung, tần số chỉnh bằng biến trở (GPIO39) hoặc lệnh Serial.
 *  - Chống dội nút, nhấn giữ; tự TẮT khi loa tắt mà u vẫn bão hòa > 20 % trong 3 s liên tiếp (mất ổn định).
 *******************************************************************/
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----------------- Chân kết nối -----------------
const int PIN_SENSOR = 34;   // ADC1_CH6: cảm biến điều khiển (đĩa trên #1) sau mạch đệm
const int PIN_MON    = 35;   // ADC1_CH7: cảm biến giám sát Ø20 tại 80 mm (tùy chọn)
const int PIN_FPOT   = 39;   // ADC1_CH3: biến trở chỉnh tần số kích rung
const int PIN_DAC    = 25;   // DAC1: tín hiệu điều khiển -> mạch vi sai -> TDA2030A
const int PIN_EXC    = 26;   // DAC2: tín hiệu sin -> PAM8403 -> loa kích rung
const int BTN_ONOFF  = 32;   // nhấn: bật/tắt điều khiển
const int BTN_MODE   = 33;   // nhấn: PPF/DVF   | giữ 1,5 s: bật/tắt loa kích rung
const int BTN_GAIN   = 27;   // nhấn: mức 1–3   | giữ 1,5 s: tự nhận dạng (f1, H0, SIGN)

// ----------------- Thông số hệ thống -----------------
const float FS = 1000.0f, DT = 1.0f / FS;
const float V_REF = 1.65f;
const float U_MAX = 1.50f;                          // |u| quanh V_REF -> |Va| ≈ 26 V
const float ZETA_F = 0.35f;
const float ALPHA_PPF[] = {0.25f, 0.50f, 0.75f};    // tỉ lệ so với giới hạn ổn định 1/H0 (≈ G = 0,2 / 0,4 / 0,6)
const float DZETA_DVF[] = {0.02f, 0.05f, 0.08f};    // tỉ số cản thêm cho mode 1
const float K1_OVER_H0 = 0.75f;                     // phần đóng góp mode 1 trong H0 (mô phỏng: 0,72–0,78)
const float SAT_FRAC_TRIP = 0.20f;                  // tỉ lệ thời gian bão hòa trong cửa sổ 1 s
const int   SAT_WIN_TRIP = 3;                       // số cửa sổ liên tiếp -> tự tắt

float F_PPF = 13.6f;      // tần số lọc PPF (Hz) – ghi đè bằng nhận dạng
float H0 = 0.010f;        // độ lợi tĩnh u -> y (V/V) – ghi đè bằng nhận dạng
int   SIGN = +1;          // chiều phân cực – ghi đè bằng nhận dạng
bool  calibrated = false;

enum Mode { MODE_PPF = 0, MODE_DVF = 1 };
enum State { ST_RUN, ST_ID_TAP, ST_ID_SINE, ST_TEST };
volatile bool ctrlOn = false, excOn = false;
volatile Mode mode = MODE_PPF;
volatile int gainIdx = 1;
volatile State state = ST_RUN;
volatile float excFreq = 13.6f, excAmp = 0.5f;      // amp 0..1 (toàn thang DAC2)
volatile bool excFreqFromPot = true;
volatile bool tripped = false;

volatile float g_y = 0, g_u = 0, g_mon = 0, g_rmsY = 0;
float offsetV = 0, offsetMon = 0;

// Bộ lọc PPF bậc 2 (Tustin có hiệu chỉnh tần số), phương trình (8) của báo cáo
float a0, a1, a2, bn;
volatile bool ppfDirty = true;
void computePPF(float f_hz) {
  float wf = 2.0f * PI * f_hz, c = wf / tanf(wf * DT / 2.0f);
  a0 = c * c + 2 * ZETA_F * wf * c + wf * wf;
  a1 = -2 * c * c + 2 * wf * wf;
  a2 = c * c - 2 * ZETA_F * wf * c + wf * wf;
  bn = wf * wf;
}

// Bộ đệm nhận dạng
const int ID_TAP_N = 2000;            // 2 s tại 1 kHz
float idBuf[ID_TAP_N];
volatile int idN = 0;
volatile float idSs = 0, idSc = 0; volatile int idK = 0;
const float ID_F = 3.0f, ID_U = 0.5f; // sin 3 Hz, biên độ 0,5 V (Va ≈ ±8,6 V)

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Preferences prefs;
SemaphoreHandle_t i2cLock;

inline float readV(int pin) { return analogReadMilliVolts(pin) / 1000.0f; }
inline void writeDAC(float u) {
  int code = (int)((V_REF + u) / 3.3f * 255.0f + 0.5f);
  dacWrite(PIN_DAC, constrain(code, 0, 255));
}

// ================= VÒNG ĐIỀU KHIỂN THỜI GIAN THỰC 1 kHz (lõi 1) =================
void controlTask(void *) {
  float y1 = 0, y2 = 0, e1 = 0, e2 = 0, hp = 0, rawPrev = 0, vel = 0, excPh = 0;
  const float BETA = 1.0f / (1.0f + 2 * PI * 1.0f * DT);     // HPF 1 Hz
  const float ALPHA = 1.0f / (1.0f + 2 * PI * 100.0f * DT);  // LPF 100 Hz cho đạo hàm
  float acc2 = 0; int n = 0; int satN = 0, winN = 0, stuck = 0;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
    if (ppfDirty) { computePPF(F_PPF); ppfDirty = false; }
    float raw = readV(PIN_SENSOR) - offsetV;
    hp = BETA * (hp + raw - rawPrev);
    float y = hp, u = 0;

    if (state == ST_TEST) {                         // lệnh 't': sin 13 Hz toàn thang 3 s để đo Va bằng dao động ký
      u = U_MAX * 0.98f * sinf(2 * PI * 13.0f * idK * DT);
      if (++idK >= 3000) state = ST_RUN;
    } else if (state == ST_ID_SINE) {                      // nhận dạng H0 và SIGN
      float ph = 2 * PI * ID_F * idK * DT;
      u = ID_U * sinf(ph);
      if (idK >= 1000) { idSs += y * sinf(ph); idSc += y * cosf(ph); }
      if (++idK >= 3000) state = ST_RUN;            // 1 s ổn định + 2 s đo (6 chu kỳ)
      e1 = e2 = vel = 0;
    } else if (state == ST_ID_TAP) {                // ghi 2 s sau khi gõ để đo f1
      if (idN < ID_TAP_N) idBuf[idN++] = y; else state = ST_RUN;
    } else if (ctrlOn) {
      if (mode == MODE_PPF) {
        float eta = (bn * (y + 2 * y1 + y2) - a1 * e1 - a2 * e2) / a0;
        e2 = e1; e1 = eta;
        u = SIGN * (ALPHA_PPF[gainIdx] / H0) * eta;
      } else {
        vel = ALPHA * vel + (1 - ALPHA) * (y - y1) * FS;
        float gv = 2.0f * DZETA_DVF[gainIdx] / (K1_OVER_H0 * H0 * 2 * PI * F_PPF);
        u = -SIGN * gv * vel;
      }
    } else { e1 = e2 = 0; vel = 0; }

    // bảo vệ: loa tắt mà vẫn bão hòa nhiều -> dao động tự kích, tự tắt (va đập mạnh chỉ bão hòa ≈ 1,2 s)
    if (ctrlOn && state == ST_RUN && fabsf(u) >= U_MAX) satN++;
    if (++winN >= 1000) {
      if (ctrlOn && !excOn && satN > SAT_FRAC_TRIP * winN) stuck++; else stuck = 0;
      if (stuck >= SAT_WIN_TRIP) { ctrlOn = false; tripped = true; stuck = 0; }
      satN = 0; winN = 0;
    }

    u = constrain(u, -U_MAX, U_MAX);
    writeDAC(u);

    // tín hiệu kích rung trên DAC2
    if (excOn) {
      excPh += 2 * PI * excFreq * DT; if (excPh > 2 * PI) excPh -= 2 * PI;
      dacWrite(PIN_EXC, 128 + (int)(127.0f * excAmp * sinf(excPh)));
    } else dacWrite(PIN_EXC, 128);

    y2 = y1; y1 = y; rawPrev = raw;
    acc2 += y * y; if (++n >= 200) { g_rmsY = sqrtf(acc2 / n); acc2 = 0; n = 0; }
    g_y = y; g_u = u;
    g_mon = readV(PIN_MON) - offsetMon;
  }
}

// ================= ADXL345: đọc trực tiếp thanh ghi, FIFO 800 Hz =================
const uint8_t ADXL = 0x53;
bool hasAccel = false;
void adxlW(uint8_t r, uint8_t v) { Wire.beginTransmission(ADXL); Wire.write(r); Wire.write(v); Wire.endTransmission(); }
bool adxlInit() {
  Wire.beginTransmission(ADXL); Wire.write(0x00); if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(ADXL, (uint8_t)1); if (!Wire.available() || Wire.read() != 0xE5) return false;
  adxlW(0x2D, 0x00);   // standby
  adxlW(0x31, 0x0B);   // DATA_FORMAT: full-res, ±16 g (3,9 mg/LSB)
  adxlW(0x2C, 0x0D);   // BW_RATE: 800 Hz (băng thông 400 Hz)
  adxlW(0x38, 0x80);   // FIFO_CTL: stream
  adxlW(0x2D, 0x08);   // đo
  return true;
}
uint32_t accN = 0;                    // số mẫu gia tốc đã gửi (t = accN * 1,25 ms)
float accRms = 0, accAcc = 0; int accCnt = 0;
float accMean = 9.81f;
void adxlDrain() {
  Wire.beginTransmission(ADXL); Wire.write(0x39); Wire.endTransmission(false);
  Wire.requestFrom(ADXL, (uint8_t)1); int entries = Wire.available() ? (Wire.read() & 0x3F) : 0;
  for (int i = 0; i < entries; i++) {
    Wire.beginTransmission(ADXL); Wire.write(0x32); Wire.endTransmission(false);
    Wire.requestFrom(ADXL, (uint8_t)6);
    uint8_t b[6]; for (int k = 0; k < 6; k++) b[k] = Wire.read();
    float az = (int16_t)(b[4] | (b[5] << 8)) * 0.0039f * 9.80665f;
    accMean += 0.001f * (az - accMean);                    // bỏ thành phần trọng trường
    float a = az - accMean;
    accAcc += a * a; if (++accCnt >= 400) { accRms = sqrtf(accAcc / accCnt); accAcc = 0; accCnt = 0; }
    // CSV: t_ms, y(V), u(V), a_z(m/s^2), on, mode, f_exc, exc_on
    Serial.printf("%.2f,%.4f,%.4f,%.3f,%d,%d,%.2f,%d\n", accN * 1.25f, (float)g_y, (float)g_u, a,
                  (int)ctrlOn, (int)mode, (float)excFreq, (int)excOn);
    accN++;
  }
}

// ================= Nhận dạng =================
float estimateF1(const float *x, int n) {
  int start = 0; float pk = 0;
  for (int i = 0; i < n; i++) if (fabsf(x[i]) > pk) { pk = fabsf(x[i]); start = i; }
  if (pk < 0.05f) return -1;                               // gõ quá nhẹ
  int first = -1, lastZ = -1, cnt = 0;
  for (int i = start + 50; i < n - 1; i++) {               // bỏ 50 ms đầu (mode cao)
    if (fabsf(x[i]) < 0.1f * pk && cnt > 0 && i - lastZ > 400) break;
    if (x[i] <= 0 && x[i + 1] > 0) { if (first < 0) first = i; lastZ = i; cnt++; }
  }
  if (cnt < 4) return -1;
  return (cnt - 1) * FS / (float)(lastZ - first);
}
void oledMsg(const char *l1, const char *l2 = "") {
  xSemaphoreTake(i2cLock, portMAX_DELAY);
  oled.clearDisplay(); oled.setTextSize(1); oled.setCursor(0, 0);
  oled.println(l1); oled.println(l2); oled.display();
  xSemaphoreGive(i2cLock);
}
void runIdentification() {
  bool wasOn = ctrlOn; ctrlOn = false; excOn = false; delay(300);
  // 1) f1: chờ người dùng gõ nhẹ
  oledMsg("NHAN DANG 1/2", "Go nhe dau dam...");
  uint32_t t0 = millis(); float f1 = -1;
  while (millis() - t0 < 10000) {
    if (fabsf(g_y) > 0.1f) { idN = 0; state = ST_ID_TAP; while (state == ST_ID_TAP) delay(10); f1 = estimateF1(idBuf, idN); break; }
    delay(2);
  }
  if (f1 > 5 && f1 < 30) { F_PPF = f1; ppfDirty = true; }
  delay(1500);
  // 2) H0, SIGN: sin 3 Hz
  oledMsg("NHAN DANG 2/2", "Sin 3 Hz, giu yen");
  idSs = idSc = 0; idK = 0; state = ST_ID_SINE;
  while (state == ST_ID_SINE) delay(10);
  float ampY = 2.0f * sqrtf(idSs * idSs + idSc * idSc) / 2000.0f;
  float h = ampY / ID_U;
  if (h > 1e-4f && h < 0.2f) {
    H0 = h; SIGN = (idSs >= 0) ? +1 : -1; calibrated = true;
    prefs.putFloat("f1", F_PPF); prefs.putFloat("h0", H0); prefs.putInt("sign", SIGN);
  }
  char b[32]; snprintf(b, sizeof b, "f1=%.2f H0=%.4f S=%+d", F_PPF, H0, SIGN);
  oledMsg(calibrated ? "XONG" : "LOI NHAN DANG", b);
  Serial.printf("# ID f1=%.3f H0=%.5f SIGN=%d\n", F_PPF, H0, SIGN);
  delay(2500); ctrlOn = wasOn && calibrated;
}

// ================= NHIỆM VỤ NỀN (lõi 0): nút, OLED, Serial, ADXL =================
struct Btn {
  int pin; bool st; uint32_t tEdge, tDown; bool longFired;
  Btn(int p) : pin(p), st(true), tEdge(0), tDown(0), longFired(false) {}
};
Btn bOn(BTN_ONOFF), bMode(BTN_MODE), bGain(BTN_GAIN);
// trả về 1 = nhấn ngắn, 2 = giữ lâu
int pollBtn(Btn &b) {
  bool r = digitalRead(b.pin); uint32_t now = millis(); int ev = 0;
  if (r != b.st && now - b.tEdge > 30) {
    b.st = r; b.tEdge = now;
    if (!r) { b.tDown = now; b.longFired = false; }
    else if (!b.longFired) ev = 1;
  }
  if (!b.st && !b.longFired && now - b.tDown > 1500) { b.longFired = true; ev = 2; }
  return ev;
}
void handleSerial() {
  static char line[24]; static int k = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[k] = 0; float v = atof(line + 1);
      switch (line[0]) {
        case 'f': excFreq = constrain(v, 2, 120); excFreqFromPot = false; break;
        case 'a': excAmp = constrain(v, 0, 1); break;
        case 'e': excOn = v > 0; break;
        case 'c': ctrlOn = v > 0; tripped = false; break;
        case 'm': mode = v > 0 ? MODE_DVF : MODE_PPF; break;
        case 'g': gainIdx = constrain((int)v, 0, 2); break;
        case 'p': if (v > 5 && v < 30) { F_PPF = v; ppfDirty = true; } break;
        case 't': if (!ctrlOn) { idK = 0; state = ST_TEST; } break;
      }
      if (line[0] == 'i') runIdentification();
      k = 0;
    } else if (k < (int)sizeof line - 1) line[k++] = c;
  }
}
void uiTask(void *) {
  uint32_t lastOled = 0; float potF = -1;
  for (;;) {
    int e;
    if ((e = pollBtn(bOn)) == 1) { ctrlOn = !ctrlOn; tripped = false; }
    if ((e = pollBtn(bMode)) == 1) mode = (mode == MODE_PPF) ? MODE_DVF : MODE_PPF;
    else if (e == 2) excOn = !excOn;
    if ((e = pollBtn(bGain)) == 1) gainIdx = (gainIdx + 1) % 3;
    else if (e == 2) runIdentification();
    handleSerial();

    // biến trở tần số: 2–60 Hz, tinh chỉnh quanh f1 dễ dàng
    float f = 2.0f + 58.0f * analogRead(PIN_FPOT) / 4095.0f;
    if (potF < 0) potF = f;
    if (fabsf(f - potF) > 0.3f) excFreqFromPot = true;
    potF = 0.9f * potF + 0.1f * f;
    if (excFreqFromPot) excFreq = roundf(potF * 10) / 10.0f;

    xSemaphoreTake(i2cLock, portMAX_DELAY);
    if (hasAccel) adxlDrain();
    if (millis() - lastOled > 250) {
      lastOled = millis();
      oled.clearDisplay(); oled.setTextSize(1); oled.setCursor(0, 0);
      oled.printf("AVC %s %s G%d\n", mode == MODE_PPF ? "PPF" : "DVF", ctrlOn ? "ON " : "OFF", gainIdx + 1);
      oled.printf("f1=%.2f %s\n", F_PPF, calibrated ? "" : "CHUA HC");
      oled.printf("Loa:%s %.1fHz\n", excOn ? "ON " : "OFF", (float)excFreq);
      if (tripped) oled.print("!TU TAT: BAO HOA\n"); else oled.printf("y=%.3fV u=%+.2f\n", (float)g_rmsY, (float)g_u);
      oled.setTextSize(2); oled.setCursor(0, 44);
      oled.printf("%.2f", hasAccel ? accRms : (float)g_rmsY);
      oled.setTextSize(1); oled.print(hasAccel ? " m/s2" : " V");
      oled.display();
    }
    xSemaphoreGive(i2cLock);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setup() {
  Serial.begin(921600);
  pinMode(BTN_ONOFF, INPUT_PULLUP); pinMode(BTN_MODE, INPUT_PULLUP); pinMode(BTN_GAIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SENSOR, ADC_11db);
  analogSetPinAttenuation(PIN_MON, ADC_11db);
  analogSetPinAttenuation(PIN_FPOT, ADC_11db);
  writeDAC(0); dacWrite(PIN_EXC, 128);
  i2cLock = xSemaphoreCreateMutex();
  Wire.begin(21, 22); Wire.setClock(400000);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); oled.setTextColor(SSD1306_WHITE);
  hasAccel = adxlInit();
  prefs.begin("avc", false);
  if (prefs.isKey("h0")) { F_PPF = prefs.getFloat("f1", F_PPF); H0 = prefs.getFloat("h0", H0); SIGN = prefs.getInt("sign", SIGN); calibrated = true; }
  computePPF(F_PPF);
  oledMsg("AVC PIEZO", "Hieu chuan offset...");
  float s = 0, sm = 0;
  for (int i = 0; i < 1000; i++) { s += readV(PIN_SENSOR); sm += readV(PIN_MON); delay(1); }
  offsetV = s / 1000.0f; offsetMon = sm / 1000.0f;
  xTaskCreatePinnedToCore(controlTask, "ctrl", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(uiTask, "ui", 8192, NULL, 1, NULL, 0);
}
void loop() { vTaskDelay(portMAX_DELAY); }
