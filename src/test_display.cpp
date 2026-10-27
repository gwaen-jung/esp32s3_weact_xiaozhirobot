/**
 * ============================================================================
 * XIAOZHI ROBO-EYES TEST SUITE (Adafruit_ILI9341 + Adafruit_GFX)
 * Chuyên test biểu cảm động 2 mắt cún robot Xiaozhi trên màn hình 2.4" SPI
 * ============================================================================
 */

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#define TFT_CS   39
#define TFT_DC   38
#define TFT_RST  47
#define TFT_MOSI 41
#define TFT_SCLK 40
#define TFT_MISO -1

// Tọa độ 2 mắt trên màn hình ngang 320x240
#define EYE_L_X  95
#define EYE_R_X  225
#define EYE_Y    105
#define EYE_W    80
#define EYE_H    95
#define EYE_R    20

static SPIClass tftSPI(FSPI);
static Adafruit_ILI9341 tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// Màu sắc cảm xúc chuẩn
#define COLOR_CYAN_EYE   0x07FF
#define COLOR_YELLOW_EYE 0xFFE0
#define COLOR_PINK_EYE   0xFCE0
#define COLOR_RED_EYE    0xF800
#define COLOR_BLUE_EYE   0x3A9F
#define COLOR_WHITE_EYE  0xFFFF
#define COLOR_GREEN_EYE  0x07E0
#define COLOR_PURPLE_EYE 0x981F

void updateStatus(const char* mood, const char* desc, uint16_t color) {
  tft.fillRect(0, 0, 320, 24, 0x10A2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 8);
  tft.print("XIAOZHI ROBOT DOG - ROBO-EYES DEMO");

  tft.fillRect(0, 204, 320, 36, 0x18C3);
  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(12, 212);
  tft.print(mood);

  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setTextSize(1);
  tft.setCursor(170, 218);
  tft.print(desc);
}

// Xóa vùng vẽ 2 mắt
void clearEyesArea() {
  tft.fillRect(30, 30, 260, 165, ILI9341_BLACK);
}

// Vẽ một mắt bo góc với đồng tử và ánh sáng phản chiếu
void drawSingleEye(int cx, int cy, int w, int h, int r, uint16_t color, int pupX = 0, int pupY = 0) {
  if (h <= 4) {
    // Nhắm mắt (đường kẻ)
    tft.fillRoundRect(cx - w/2, cy - 2, w, 4, 2, color);
    return;
  }
  // Thân mắt
  tft.fillRoundRect(cx - w/2, cy - h/2, w, h, r, color);

  // Đồng tử tối màu bên trong
  int pw = w / 2;
  int ph = h / 2;
  int px = cx + pupX;
  int py = cy + pupY;
  tft.fillRoundRect(px - pw/2, py - ph/2, pw, ph, 8, ILI9341_BLACK);

  // Điểm sáng phản chiếu (Catchlight)
  if (h > 30) {
    tft.fillCircle(px - pw/4, py - ph/4, 4, ILI9341_WHITE);
    tft.fillCircle(px + pw/4, py + ph/4, 2, ILI9341_WHITE);
  }
}

// Vẽ cặp mắt
void renderEyes(int w, int h, uint16_t color, int pupX = 0, int pupY = 0, int r = EYE_R) {
  clearEyesArea();
  drawSingleEye(EYE_L_X, EYE_Y, w, h, r, color, pupX, pupY);
  drawSingleEye(EYE_R_X, EYE_Y, w, h, r, color, pupX, pupY);
}

// Hiệu ứng chớp mắt mượt
void animBlink(uint16_t color, int pupX = 0, int pupY = 0) {
  const int steps[] = {70, 40, 12, 4, 12, 45, 95};
  for (int h : steps) {
    clearEyesArea();
    drawSingleEye(EYE_L_X, EYE_Y, EYE_W, h, min(EYE_R, h/2), color, pupX, pupY);
    drawSingleEye(EYE_R_X, EYE_Y, EYE_W, h, min(EYE_R, h/2), color, pupX, pupY);
    delay(20);
  }
}

// Vẽ trái tim nhỏ
void drawHeart(int x, int y, int size, uint16_t color) {
  tft.fillCircle(x - size/2, y - size/4, size/2, color);
  tft.fillCircle(x + size/2, y - size/4, size/2, color);
  tft.fillTriangle(x - size, y - size/6, x + size, y - size/6, x, y + size, color);
}

// 1. Bình thường (Idle / Cyan)
void moodIdle() {
  Serial.println("[RoboEyes] Mood: IDLE (Binh thuong, chop mat, nhin quanh)");
  updateStatus("IDLE", "Cyan - Nhin quanh & chop mat", COLOR_CYAN_EYE);

  // Nhìn thẳng
  renderEyes(EYE_W, EYE_H, COLOR_CYAN_EYE, 0, 0);
  delay(1200);

  // Chớp mắt
  animBlink(COLOR_CYAN_EYE);
  delay(800);

  // Liếc sang trái
  renderEyes(EYE_W, EYE_H, COLOR_CYAN_EYE, -18, 0);
  delay(1000);

  // Liếc sang phải
  renderEyes(EYE_W, EYE_H, COLOR_CYAN_EYE, 18, 0);
  delay(1000);

  // Nhìn thẳng lại
  renderEyes(EYE_W, EYE_H, COLOR_CYAN_EYE, 0, 0);
  delay(1000);
}

// 2. Lắng nghe (Listening / White - Mắt mở to tò mò)
void moodListening() {
  Serial.println("[RoboEyes] Mood: LISTENING (Mo to mat, cham chu lang nghe)");
  updateStatus("LISTENING", "Trang - To mo lang nghe ban", COLOR_WHITE_EYE);

  for (int w = EYE_W; w <= 92; w += 3) {
    int h = map(w, EYE_W, 92, EYE_H, 108);
    renderEyes(w, h, COLOR_WHITE_EYE, 0, 0, 24);
    delay(25);
  }

  // Dấu hỏi hoặc chấm sáng tò mò
  tft.setTextColor(COLOR_CYAN_EYE);
  tft.setTextSize(3);
  tft.setCursor(290, 45);
  tft.print("?");

  delay(2000);
  animBlink(COLOR_WHITE_EYE);
  delay(1000);
}

// 3. Đang suy nghĩ (Thinking / Light Blue - Liếc lên trên góc trái)
void moodThinking() {
  Serial.println("[RoboEyes] Mood: THINKING (Dang suy nghi, liec goc)");
  updateStatus("THINKING", "Xanh nhe - Dang suy nghi...", COLOR_BLUE_EYE);

  renderEyes(EYE_W, EYE_H - 15, COLOR_BLUE_EYE, -15, -15);

  // Vẽ bong bóng suy nghĩ
  tft.fillCircle(270, 75, 4, COLOR_BLUE_EYE);
  tft.fillCircle(282, 60, 7, COLOR_BLUE_EYE);
  tft.fillCircle(298, 42, 12, COLOR_BLUE_EYE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(293, 35);
  tft.print("?");

  delay(2500);
}

// 4. Vui vẻ / Cười híp mắt (Happy / Yellow)
void moodHappy() {
  Serial.println("[RoboEyes] Mood: HAPPY (Vui ve, cuoi hip mat, ma hong)");
  updateStatus("HAPPY", "Vang - Cuoi toe toet, ma hong", COLOR_YELLOW_EYE);

  clearEyesArea();

  // Mắt cong hình vòng cung cười híp mí
  for (int cx : {EYE_L_X, EYE_R_X}) {
    tft.fillRoundRect(cx - 38, EYE_Y - 20, 76, 40, 18, COLOR_YELLOW_EYE);
    tft.fillRoundRect(cx - 38, EYE_Y - 5, 76, 35, 12, ILI9341_BLACK); // Khoét dưới
  }

  // Má hồng 2 bên
  tft.fillCircle(EYE_L_X - 45, EYE_Y + 28, 12, 0xF94B);
  tft.fillCircle(EYE_R_X + 45, EYE_Y + 28, 12, 0xF94B);

  // Miệng cười nhỏ
  tft.drawCircle(160, EYE_Y + 30, 16, COLOR_YELLOW_EYE);
  tft.fillRect(140, EYE_Y + 14, 40, 17, ILI9341_BLACK);

  delay(2500);
}

// 5. Yêu thương / Đáng yêu (Love / Pink - Mắt hình trái tim)
void moodLove() {
  Serial.println("[RoboEyes] Mood: LOVE (Trai tim hong ngot ngao)");
  updateStatus("LOVE", "Hong - Mat hinh trai tim", COLOR_PINK_EYE);

  clearEyesArea();

  // Vẽ 2 mắt trái tim to
  drawHeart(EYE_L_X, EYE_Y + 5, 32, COLOR_PINK_EYE);
  drawHeart(EYE_R_X, EYE_Y + 5, 32, COLOR_PINK_EYE);

  // Má hồng
  tft.fillCircle(EYE_L_X - 42, EYE_Y + 35, 10, 0xF995);
  tft.fillCircle(EYE_R_X + 42, EYE_Y + 35, 10, 0xF995);

  // Trái tim bay nhỏ
  drawHeart(160, 50, 12, COLOR_PINK_EYE);

  delay(2500);
}

// 6. Nháy mắt tinh nghịch (Wink / Green)
void moodWink() {
  Serial.println("[RoboEyes] Mood: WINK (Nhay mat tinh nghich)");
  updateStatus("WINK", "Xanh la - Nhay mot ben mat", COLOR_GREEN_EYE);

  clearEyesArea();
  // Mắt trái mở to
  drawSingleEye(EYE_L_X, EYE_Y, EYE_W, EYE_H, EYE_R, COLOR_GREEN_EYE, 5, 0);
  // Mắt phải nháy (dạng cong vòm)
  tft.fillRoundRect(EYE_R_X - 35, EYE_Y - 8, 70, 24, 10, COLOR_GREEN_EYE);
  tft.fillRoundRect(EYE_R_X - 35, EYE_Y - 2, 70, 22, 10, ILI9341_BLACK);

  delay(1800);
}

// 7. Buồn ngủ / Buồn rười rượi (Sleepy / Purple)
void moodSleepy() {
  Serial.println("[RoboEyes] Mood: SLEEPY (Buon ngu, mat lim dan, Zzz)");
  updateStatus("SLEEPY", "Tim - Mip mat ngu, chu Zzz bay", COLOR_PURPLE_EYE);

  // Mắt híp dần
  renderEyes(EYE_W, 35, COLOR_PURPLE_EYE, 0, 5);

  // Chữ Zzz bay lên
  tft.setTextColor(COLOR_PURPLE_EYE);
  tft.setTextSize(2);
  tft.setCursor(240, 50);
  tft.print("z");
  delay(400);
  tft.setTextSize(3);
  tft.setCursor(265, 38);
  tft.print("Z");
  delay(400);
  tft.setTextSize(4);
  tft.setCursor(285, 24);
  tft.print("Z");

  delay(2500);
}

// 8. Tức giận / Hờn dỗi (Angry / Red - Mắt vát mí xéo)
void moodAngry() {
  Serial.println("[RoboEyes] Mood: ANGRY (Gian doi, mat vat cheo)");
  updateStatus("ANGRY", "Do - Mat xeo gian doi", COLOR_RED_EYE);

  clearEyesArea();
  // Vẽ nền mắt đỏ
  drawSingleEye(EYE_L_X, EYE_Y, EYE_W, EYE_H, EYE_R, COLOR_RED_EYE, 10, 5);
  drawSingleEye(EYE_R_X, EYE_Y, EYE_W, EYE_H, EYE_R, COLOR_RED_EYE, -10, 5);

  // Che mí xéo tạo nét mặt giận (tam giác đen che góc trên hướng vào giữa)
  tft.fillTriangle(EYE_L_X - 45, EYE_Y - 50, EYE_L_X + 45, EYE_Y - 50, EYE_L_X + 45, EYE_Y - 5, ILI9341_BLACK);
  tft.fillTriangle(EYE_R_X + 45, EYE_Y - 50, EYE_R_X - 45, EYE_Y - 50, EYE_R_X - 45, EYE_Y - 5, ILI9341_BLACK);

  // Ký hiệu gân giận tức
  tft.drawLine(150, 48, 170, 48, COLOR_RED_EYE);
  tft.drawLine(160, 38, 160, 58, COLOR_RED_EYE);

  delay(2200);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================================");
  Serial.println("  XIAOZHI ROBOT DOG - ROBO-EYES DISPLAY DEMO");
  Serial.println("  Thu vien: Adafruit_ILI9341 (Hardware SPI 27MHz)");
  Serial.println("========================================================");

  // Reset cứng màn hình
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(25);
  digitalWrite(TFT_RST, HIGH);
  delay(120);

  // Kích mức HIGH cho GPIO42 và GPIO1 phòng khi cắm LED vào đó
  pinMode(42, OUTPUT); digitalWrite(42, HIGH);
  pinMode(1, OUTPUT);  digitalWrite(1, HIGH);

  // Khởi động SPI bus cho ILI9341
  tftSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(27000000); // 27 MHz mượt mà
  tft.setRotation(1);  // Ngang 320x240

  // Chớp nhẹ khởi động
  tft.fillScreen(ILI9341_BLACK);

  Serial.println("[RoboEyes] Khoi tao hoan tat! Bat dau chu trinh bieu cam...");
}

void loop() {
  // Chu trình 8 cảm xúc sống động của cún
  moodIdle();       // 1. Bình thường nhìn quanh
  moodListening();  // 2. Lắng nghe bạn
  moodThinking();   // 3. Đang suy nghĩ
  moodHappy();      // 4. Vui mừng cười híp mắt
  moodLove();       // 5. Yêu thương (mắt trái tim)
  moodWink();       // 6. Nháy mắt
  moodSleepy();     // 7. Buồn ngủ Zzz
  moodAngry();      // 8. Hờn dỗi
}
