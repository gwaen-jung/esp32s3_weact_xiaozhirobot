#include "rgb565_canvas.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "font_8x16.h"

#define TAG "Rgb565Canvas"

static inline uint16_t to_panel(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

void DirtyBox::add(int ax0, int ay0, int ax1, int ay1) {
    if (empty()) {
        x0 = ax0; y0 = ay0; x1 = ax1; y1 = ay1;
        return;
    }
    x0 = std::min(x0, ax0);
    y0 = std::min(y0, ay0);
    x1 = std::max(x1, ax1);
    y1 = std::max(y1, ay1);
}

Rgb565Canvas::Rgb565Canvas(int width, int height) : w_(width), h_(height) {
    size_t bytes = (size_t)w_ * h_ * sizeof(uint16_t);
    buf_ = (uint16_t*)heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf_ == nullptr) {
        ESP_LOGW(TAG, "No PSRAM for frame buffer, trying internal RAM");
        buf_ = (uint16_t*)heap_caps_calloc(1, bytes, MALLOC_CAP_8BIT);
    }
    if (buf_ == nullptr) {
        ESP_LOGE(TAG, "Frame buffer allocation failed (%u bytes)", (unsigned)bytes);
    }
}

Rgb565Canvas::~Rgb565Canvas() { heap_caps_free(buf_); }

void Rgb565Canvas::track(int x0, int y0, int x1, int y1) { cur_.add(x0, y0, x1, y1); }

void Rgb565Canvas::hline(int x, int y, int w, uint16_t color) {
    if (w < 0) {
        x += w + 1;
        w = -w;
    }
    if (y < 0 || y >= h_ || w <= 0) return;
    int xe = x + w - 1;
    if (x < 0) x = 0;
    if (xe >= w_) xe = w_ - 1;
    if (xe < x) return;
    uint16_t pc = to_panel(color);
    uint16_t* p = buf_ + (size_t)y * w_ + x;
    for (int i = x; i <= xe; ++i) *p++ = pc;
    track(x, y, xe, y);
}

void Rgb565Canvas::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (!buf_ || w <= 0 || h <= 0) return;
    int x0 = std::max(x, 0), y0 = std::max(y, 0);
    int x1 = std::min(x + w - 1, w_ - 1), y1 = std::min(y + h - 1, h_ - 1);
    if (x1 < x0 || y1 < y0) return;
    uint16_t pc = to_panel(color);
    for (int yy = y0; yy <= y1; ++yy) {
        uint16_t* p = buf_ + (size_t)yy * w_ + x0;
        for (int xx = x0; xx <= x1; ++xx) *p++ = pc;
    }
    track(x0, y0, x1, y1);
}

void Rgb565Canvas::fillScreen(uint16_t color) {
    if (!buf_) return;
    uint16_t pc = to_panel(color);
    for (size_t i = 0; i < (size_t)w_ * h_; ++i) buf_[i] = pc;
    all_dirty_ = true;
}

void Rgb565Canvas::clearDisplay() {
    // Only the area drawn in the previous frame can be non-background.
    if (!buf_ || prev_.empty()) return;
    uint16_t pc = to_panel(bg_);
    for (int y = prev_.y0; y <= prev_.y1; ++y) {
        uint16_t* p = buf_ + (size_t)y * w_ + prev_.x0;
        for (int x = prev_.x0; x <= prev_.x1; ++x) *p++ = pc;
    }
}

// Port of Adafruit_GFX::fillCircleHelper (BSD license, Adafruit Industries).
void Rgb565Canvas::fillCircleHelper(int x0, int y0, int r, uint8_t corners, int delta,
                                    uint16_t color) {
    int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r, px = x, py = y;
    delta++;
    auto vline = [&](int vx, int vy, int vh) { fillRect(vx, vh < 0 ? vy + vh + 1 : vy, 1, vh < 0 ? -vh : vh, color); };
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (x < (y + 1)) {
            if (corners & 1) vline(x0 + x, y0 - y, 2 * y + delta);
            if (corners & 2) vline(x0 - x, y0 - y, 2 * y + delta);
        }
        if (y != py) {
            if (corners & 1) vline(x0 + py, y0 - px, 2 * px + delta);
            if (corners & 2) vline(x0 - py, y0 - px, 2 * px + delta);
            py = y;
        }
        px = x;
    }
}

// Port of Adafruit_GFX::fillRoundRect.
void Rgb565Canvas::fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int max_radius = ((w < h) ? w : h) / 2;
    if (r > max_radius) r = max_radius;
    if (r < 0) r = 0;
    fillRect(x + r, y, w - 2 * r, h, color);
    if (r > 0) {
        fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
        fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
    }
}

// Port of Adafruit_GFX::fillTriangle.
void Rgb565Canvas::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    int a, b, y, last;
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y2, y1); std::swap(x2, x1); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }

    if (y0 == y2) {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        hline(a, y0, b - a + 1, color);
        return;
    }
    int dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1,
        dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;
    last = (y1 == y2) ? y1 : y1 - 1;
    for (y = y0; y <= last; y++) {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b) std::swap(a, b);
        hline(a, y, b - a + 1, color);
    }
    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b) std::swap(a, b);
        hline(a, y, b - a + 1, color);
    }
}

DirtyBox Rgb565Canvas::takeDirty() {
    DirtyBox out;
    if (all_dirty_) {
        out.add(0, 0, w_ - 1, h_ - 1);
        all_dirty_ = false;
    } else {
        out = prev_;
        out.add(cur_);
    }
    prev_ = cur_;
    cur_.reset();
    return out;
}

void Rgb565Canvas::markAllDirty() { all_dirty_ = true; }

// ---- text ------------------------------------------------------------------
static uint32_t next_codepoint(const char*& s) {
    const unsigned char* p = (const unsigned char*)s;
    uint32_t cp;
    int extra;
    if (p[0] < 0x80) { cp = p[0]; extra = 0; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; extra = 1; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; extra = 2; }
    else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07; extra = 3; }
    else { s++; return '?'; }
    s++;
    for (int i = 0; i < extra; ++i) {
        if ((((const unsigned char*)s)[0] & 0xC0) != 0x80) return '?';
        cp = (cp << 6) | (((const unsigned char*)s)[0] & 0x3F);
        s++;
    }
    return cp;
}

static const uint8_t* find_glyph(uint32_t cp) {
    int lo = 0, hi = (int)(sizeof(kFont8x16Codepoints) / sizeof(kFont8x16Codepoints[0])) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (kFont8x16Codepoints[mid] == cp) return kFont8x16Bitmaps[mid];
        if (kFont8x16Codepoints[mid] < cp) lo = mid + 1; else hi = mid - 1;
    }
    return nullptr;
}

int Rgb565Canvas::textWidth(const char* utf8, int scale) {
    int n = 0;
    while (*utf8) {
        next_codepoint(utf8);
        n++;
    }
    return n * 8 * scale;
}

int Rgb565Canvas::drawText(int x, int y, const char* utf8, uint16_t color, int scale) {
    int start = x;
    while (*utf8) {
        uint32_t cp = next_codepoint(utf8);
        const uint8_t* g = find_glyph(cp);
        if (!g) g = find_glyph('?');
        for (int row = 0; row < 16 && g; ++row) {
            uint8_t bits = g[row];
            for (int col = 0; col < 8; ++col) {
                if (bits & (0x80 >> col)) fillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
        x += 8 * scale;
        if (x >= w_) break;
    }
    return x - start;
}
