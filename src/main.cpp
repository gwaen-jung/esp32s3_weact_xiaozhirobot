/**
 * main_xiaozhi.cpp
 * Xiaozhi AI Voice Assistant & Desktop Robot
 * Board: ESP32-S3 WeAct N16R8 (16MB Flash, 8MB PSRAM OPI)
 * Display: GMT147SPI (ST7789V3 172x320 IPS)
 * Env: esp32s3_xiaozhi
 *
 * Hardware Pinout Map:
 *   [GMT147SPI Display - SPI3/HSPI]
 *     SCL  -> GPIO40 (SCLK)
 *     SDA  -> GPIO41 (MOSI)
 *     CS   -> GPIO39
 *     DC   -> GPIO38
 *     RES  -> GPIO47
 *     BL   -> 3.3V
 *   [INMP441 MEMS Microphone - I2S0]
 *     SCK  -> GPIO15
 *     WS   -> GPIO16
 *     SD   -> GPIO17
 *     L/R  -> GND (Left channel)
 *   [MAX98357A I2S DAC / Speaker - I2S1]
 *     BCLK -> GPIO12
 *     LRC  -> GPIO13
 *     DIN  -> GPIO14
 *     GAIN -> GND (12dB)
 *   [VL53L0X / VL53L1X ToF Distance Sensor - I2C]
 *     SDA  -> GPIO8
 *     SCL  -> GPIO9
 *     XSHUT-> GPIO21
 *   [4x Servos (180 deg) - LEDC PWM]
 *     SERVO1 -> GPIO4
 *     SERVO2 -> GPIO5
 *     SERVO3 -> GPIO6
 *     SERVO4 -> GPIO7
 *   [Status & Controls]
 *     RGB LED -> GPIO48 (WS2812 onboard)
 *     BUTTON  -> GPIO1  (Wake / Push-to-talk)
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "Logo/Logo.h"

static TFT_eSPI tft = TFT_eSPI();

// Sprite 1bpp cho logo: 172x190 = 4,085 bytes SRAM/PSRAM (khong nhap nhay)
static constexpr int SPRITE_W = 172;
static constexpr int SPRITE_H = 190;
static constexpr int SPRITE_Y = 28;
static TFT_eSprite logoSprite = TFT_eSprite(&tft);

// Sprite 1bpp cho dong chu chay (marquee ticker): 172x20 = 430 bytes
static constexpr int MARQUEE_W = 172;
static constexpr int MARQUEE_H = 20;
static constexpr int MARQUEE_Y = 292;
static TFT_eSprite marqueeSprite = TFT_eSprite(&tft);

// Bang mau ReShape Lab & Cyberpunk
static const uint16_t COLOR_RESHAPE_ORANGE = tft.color565(217, 119, 87);  // #D97757 chinh hang
static const uint16_t COLOR_CYBER_CYAN     = tft.color565(0, 240, 255);
static const uint16_t COLOR_ELECTRIC_BLUE  = tft.color565(70, 160, 255);
static const uint16_t COLOR_NEON_GREEN     = tft.color565(40, 255, 160);
static const uint16_t COLOR_PURE_WHITE     = TFT_WHITE;
static const uint16_t COLOR_BG             = TFT_BLACK;

static const uint16_t THEME_COLORS[] = {
    COLOR_RESHAPE_ORANGE,
    COLOR_CYBER_CYAN,
    COLOR_ELECTRIC_BLUE,
    COLOR_PURE_WHITE,
    COLOR_NEON_GREEN
};
static constexpr size_t NUM_THEMES = sizeof(THEME_COLORS) / sizeof(THEME_COLORS[0]);

// Chuoi chay marquee o day man hinh
static const char kMarqueeText[] = "  ✦ RESHAPE LAB ✦ CINQ ✦ TRIAD UAV ECOSYSTEM ✦ ESP32-S3 N16R8 ✦ AUTOMATION & ROBOTICS ✦";
static int gMarqueeOffset = 0;
static int gMarqueeTextW = 0;

// Render 1 khung logo voi ty le 'scale' vao sprite (chong giat tuyet doi)
static void renderLogoFrame(float scale, uint16_t fgColor, int maxVisibleY = SPRITE_H) {
    logoSprite.fillSprite(0);
    logoSprite.setBitmapColor(fgColor, COLOR_BG);

    constexpr int kRowBytes = (kLogoTftW + 7) / 8;
    const float cx = (SPRITE_W - 1) / 2.0f;
    const float cy = (SPRITE_H - 1) / 2.0f;
    const float logo_cx = (kLogoTftW - 1) / 2.0f;
    const float logo_cy = (kLogoTftH - 1) / 2.0f;

    int srcCol[SPRITE_W];
    int srcRow[SPRITE_H];

    for (int x = 0; x < SPRITE_W; ++x) {
        const int sx = static_cast<int>(lroundf((x - cx) / scale + logo_cx));
        srcCol[x] = (sx >= 0 && sx < kLogoTftW) ? sx : -1;
    }
    for (int y = 0; y < SPRITE_H; ++y) {
        const int sy = static_cast<int>(lroundf((y - cy) / scale + logo_cy));
        srcRow[y] = (sy >= 0 && sy < kLogoTftH) ? sy : -1;
    }

    const int limitY = min(SPRITE_H, maxVisibleY);
    for (int y = 0; y < limitY; ++y) {
        if (srcRow[y] < 0) continue;
        const uint8_t *row = kLogoTft + srcRow[y] * kRowBytes;
        for (int x = 0; x < SPRITE_W; ++x) {
            const int sx = srcCol[x];
            if (sx < 0) continue;
            if (pgm_read_byte(row + (sx >> 3)) & (0x80 >> (sx & 7))) {
                logoSprite.drawPixel(x, y, 1);
            }
        }
    }

    logoSprite.pushSprite(0, SPRITE_Y);
}

// Phase 1: Laser Scanline Wipe Reveal (Tu tren xuong, lay cam hung tu OLED runIntro)
static void runLaserWipeIntro(uint32_t durationMs) {
    Serial.println("[ANIM] Phase 1: Laser Wipe Reveal starting...");
    const uint32_t start = millis();
    constexpr float kFixedScale = 0.82f;

    while (millis() - start < durationMs) {
        const uint32_t elapsed = millis() - start;
        const int scanY = static_cast<int>((elapsed * (SPRITE_H + 10)) / durationMs);

        renderLogoFrame(kFixedScale, COLOR_RESHAPE_ORANGE, scanY);

        // Ve tia laser quet ngang o vi tri scanY
        if (scanY < SPRITE_H) {
            tft.drawFastHLine(0, SPRITE_Y + scanY, SPRITE_W, TFT_WHITE);
            if (scanY > 0) tft.drawFastHLine(4, SPRITE_Y + scanY - 1, SPRITE_W - 8, COLOR_CYBER_CYAN);
        }
        delay(8);
    }

    // Xoa vet tia laser khung cuoi
    renderLogoFrame(kFixedScale, COLOR_RESHAPE_ORANGE, SPRITE_H);
}

// Phase 2: Breathing & Pulsing (To -> Nho -> To nhu Display_BootLogo cua js-controler)
static void runBreathingIntro(uint32_t durationMs) {
    Serial.println("[ANIM] Phase 2: Breathing Zoom starting...");
    constexpr float kMinScale = 0.58f;
    constexpr float kMaxScale = 0.88f;
    constexpr float kCycles   = 2.0f;  // 2 nhip tho

    const uint32_t start = millis();
    while (millis() - start < durationMs) {
        const uint32_t elapsed = millis() - start;
        const float phase = 2.0f * PI * kCycles * (static_cast<float>(elapsed) / durationMs);
        const float scale = kMinScale + (kMaxScale - kMinScale) * (0.5f + 0.5f * (1.0f - cosf(phase)));

        // Mau chuyen nhe tu ReShape Orange sang Cyan theo nhip
        const uint16_t color = (cosf(phase) > 0) ? COLOR_RESHAPE_ORANGE : COLOR_CYBER_CYAN;
        renderLogoFrame(scale, color);
        delay(12);
    }
}

// Ve khung tinh giao dien (HUD Top Bar + Brand Text + Separator)
static void drawStaticUI() {
    tft.setTextDatum(MC_DATUM);

    // --- Top Bar HUD ---
    tft.fillRect(0, 0, 172, 24, tft.color565(12, 16, 24));
    tft.drawFastHLine(0, 24, 172, COLOR_RESHAPE_ORANGE);
    tft.setTextColor(TFT_WHITE, tft.color565(12, 16, 24));
    tft.drawString("TRIAD // RESHAPE", 86, 12, 2);

    // --- Brand Text ---
    // RESHAPE LAB (Font 4)
    tft.setTextColor(COLOR_PURE_WHITE, COLOR_BG);
    tft.drawString("RESHAPE LAB", 86, 236, 4);

    // Subtitle (Font 2)
    tft.setTextColor(COLOR_CYBER_CYAN, COLOR_BG);
    tft.drawString("AUTONOMOUS SYSTEMS", 86, 260, 2);

    tft.setTextColor(tft.color565(120, 140, 160), COLOR_BG);
    tft.drawString("WEACT S3 // GMT147", 86, 276, 1);

    // --- Bottom Separator ---
    tft.drawFastHLine(10, 288, 152, tft.color565(40, 50, 70));
}

// Cap nhat dong Marquee Ticker chay muot o chan man hinh
static void updateMarquee(uint16_t color) {
    if (gMarqueeTextW == 0) {
        gMarqueeTextW = tft.textWidth(kMarqueeText, 2);
        if (gMarqueeTextW == 0) gMarqueeTextW = 1;
    }

    marqueeSprite.fillSprite(0);
    marqueeSprite.setBitmapColor(color, COLOR_BG);
    marqueeSprite.setTextDatum(TL_DATUM);

    // Ve text 2 lan de lap vo tan lien tuc
    marqueeSprite.drawString(kMarqueeText, gMarqueeOffset, 2, 2);
    marqueeSprite.drawString(kMarqueeText, gMarqueeOffset + gMarqueeTextW, 2, 2);

    marqueeSprite.pushSprite(0, MARQUEE_Y);

    gMarqueeOffset -= 2;
    if (gMarqueeOffset <= -gMarqueeTextW) {
        gMarqueeOffset = 0;
    }
}

// Cap nhat Uptime & Heap tren Top Bar
static void updateHudStats() {
    static uint32_t lastHudMs = 0;
    const uint32_t now = millis();
    if (now - lastHudMs < 500) return;
    lastHudMs = now;

    const uint32_t s = now / 1000;
    char buf[24];
    snprintf(buf, sizeof(buf), "UP %02lu:%02lu", static_cast<unsigned long>((s / 60) % 60),
             static_cast<unsigned long>(s % 60));

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COLOR_CYBER_CYAN, tft.color565(12, 16, 24));
    tft.drawString(buf, 168, 6, 1);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_RESHAPE_ORANGE, tft.color565(12, 16, 24));
    tft.drawString("ST7789V3", 4, 6, 1);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n==========================================");
    Serial.println("  ReShape Lab Logo Engine on GMT147SPI");
    Serial.println("  TRIAD Ecosystem - Cinq / Nguyen Trung");
    Serial.println("==========================================");

    tft.init();
    tft.setRotation(0);  // Portrait 172x320
    tft.fillScreen(COLOR_BG);

    // Khoi tao sprite 1bpp cho Logo & Marquee
    logoSprite.setColorDepth(1);
    if (!logoSprite.createSprite(SPRITE_W, SPRITE_H)) {
        Serial.println("[ERR] Khong the tao logoSprite!");
    } else {
        Serial.printf("[OK] logoSprite san sang (%dx%d 1bpp, %d bytes)\n", SPRITE_W, SPRITE_H, (SPRITE_W * SPRITE_H) / 8);
    }

    marqueeSprite.setColorDepth(1);
    if (!marqueeSprite.createSprite(MARQUEE_W, MARQUEE_H)) {
        Serial.println("[ERR] Khong the tao marqueeSprite!");
    } else {
        Serial.printf("[OK] marqueeSprite san sang (%dx%d 1bpp, %d bytes)\n", MARQUEE_W, MARQUEE_H, (MARQUEE_W * MARQUEE_H) / 8);
    }

    // Do do dai text marquee
    gMarqueeTextW = tft.textWidth(kMarqueeText, 2);

    // === CHAY BOOT ANIMATION (Lay tu js-controler) ===
    runLaserWipeIntro(900);    // Phase 1: Laser Scanline Wipe 900ms
    drawStaticUI();            // Ve khung UI tinh
    runBreathingIntro(2500);   // Phase 2: Nhip tho 2.5s (Display_BootLogo)

    Serial.println("[OK] Boot Animation hoan tat. Chuyen sang Loop mode.");
}

void loop() {
    static uint32_t lastFrameMs = 0;
    static uint32_t themeStartMs = 0;
    static size_t currentThemeIdx = 0;
    static float breathPhase = 0.0f;

    const uint32_t now = millis();

    // Duy tri toc do khung hinh ~35 FPS (28ms / frame)
    if (now - lastFrameMs >= 28) {
        lastFrameMs = now;

        // Chuyen doi chu de mau dinh ky moi 4 giay
        if (now - themeStartMs >= 4000) {
            themeStartMs = now;
            currentThemeIdx = (currentThemeIdx + 1) % NUM_THEMES;
            Serial.printf("[THEME] Doi mau logo index %u (0x%04X)\n", currentThemeIdx, THEME_COLORS[currentThemeIdx]);
        }

        const uint16_t currentColor = THEME_COLORS[currentThemeIdx];

        // Tinh toan nhip tho (Breathing oscillation)
        constexpr float kMinScale = 0.65f;
        constexpr float kMaxScale = 0.86f;
        breathPhase += 0.045f;
        if (breathPhase >= 2.0f * PI) breathPhase -= 2.0f * PI;

        const float scale = kMinScale + (kMaxScale - kMinScale) * (0.5f + 0.5f * (1.0f - cosf(breathPhase)));

        // Render logo len sprite va day ra man hinh
        renderLogoFrame(scale, currentColor);

        // Cap nhat Marquee ticker chay o chan man hinh
        updateMarquee(currentColor);

        // Cap nhat Uptime/Heap
        updateHudStats();
    }
}
