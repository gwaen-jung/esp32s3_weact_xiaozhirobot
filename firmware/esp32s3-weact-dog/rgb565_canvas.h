// Minimal Adafruit_GFX-compatible RGB565 canvas (PSRAM frame buffer + dirty box).
// Only the primitives RoboEyes needs, plus a small bitmap text renderer.
#pragma once

#include <cstdint>

struct DirtyBox {
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1;  // inclusive; empty when x1 < x0
    bool empty() const { return x1 < x0 || y1 < y0; }
    void reset() { x0 = y0 = 0; x1 = y1 = -1; }
    void add(int ax0, int ay0, int ax1, int ay1);
    void add(const DirtyBox& o) {
        if (!o.empty()) add(o.x0, o.y0, o.x1, o.y1);
    }
};

class Rgb565Canvas {
public:
    Rgb565Canvas(int width, int height);
    ~Rgb565Canvas();
    bool ok() const { return buf_ != nullptr; }

    int width() const { return w_; }
    int height() const { return h_; }
    // Big-endian (panel order) pixels, row-major.
    const uint16_t* buffer() const { return buf_; }

    // ---- Adafruit_GFX-style API used by RoboEyes --------------------------
    // clearDisplay() only clears what was drawn in the previous frame.
    void clearDisplay();
    void display() {}  // pushing is done by the owner (EyesDisplay)
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
    void fillScreen(uint16_t color);
    void setBackground(uint16_t color) { bg_ = color; }

    // ---- Text (UTF-8, ASCII + Vietnamese; unknown glyphs drawn as '?') ----
    // Returns the number of pixels used horizontally.
    int drawText(int x, int y, const char* utf8, uint16_t color, int scale);
    static int textWidth(const char* utf8, int scale);
    static int glyphW() { return 8; }
    static int glyphH() { return 16; }

    // Area that must be pushed to the panel: last frame + this frame.
    // Call after drawing; resets the per-frame tracking.
    DirtyBox takeDirty();
    void markAllDirty();

private:
    void hline(int x, int y, int w, uint16_t color);
    void fillCircleHelper(int x0, int y0, int r, uint8_t corners, int delta, uint16_t color);
    void track(int x0, int y0, int x1, int y1);

    int w_, h_;
    uint16_t* buf_ = nullptr;
    DirtyBox cur_;   // drawn since last takeDirty()
    DirtyBox prev_;  // drawn in the frame before (needs clearing / re-push)
    bool all_dirty_ = true;
    uint16_t bg_ = 0;
};
