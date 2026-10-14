#include "eyes_display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "dog_config.h"

#define TAG "EyesDisplay"

using namespace roboeyes;

namespace {
int64_t now_ms() { return esp_timer_get_time() / 1000; }

// Linear blend of two RGB565 colours, t in 0..256.
uint16_t blend565(uint16_t a, uint16_t b, int t) {
    int ar = a >> 11, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = b >> 11, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r = ar + ((br - ar) * t) / 256;
    int g = ag + ((bg - ag) * t) / 256;
    int bl = ab + ((bb - ab) * t) / 256;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Move each channel of `cur` at least one step towards `target`.
uint16_t fade565(uint16_t cur, uint16_t target, int step) {
    if (cur == target) return cur;
    uint16_t next = blend565(cur, target, step);
    if (next == cur) {  // integer blend stalled: nudge channels by 1
        int cr = cur >> 11, cg = (cur >> 5) & 0x3F, cb = cur & 0x1F;
        int tr = target >> 11, tg = (target >> 5) & 0x3F, tb = target & 0x1F;
        cr += (tr > cr) - (tr < cr);
        cg += (tg > cg) - (tg < cg);
        cb += (tb > cb) - (tb < cb);
        next = (uint16_t)((cr << 11) | (cg << 5) | cb);
    }
    return next;
}

// Triangle wave 0..1..0 over `period_ms`, shifted by `phase_ms`.
float tri(int64_t t, int period_ms, int phase_ms = 0) {
    float p = (float)((t + phase_ms) % period_ms) / (float)period_ms;
    return p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;
}

// Saw wave 0..1 over `period_ms`.
float saw(int64_t t, int period_ms, int phase_ms = 0) {
    return (float)((t + phase_ms) % period_ms) / (float)period_ms;
}
}  // namespace

EyesDisplay::EyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height)
    : panel_io_(panel_io), panel_(panel), canvas_(width, height), eyes_(canvas_) {
    width_ = width;
    height_ = height;
#if HAVE_LVGL
    // The core's asset loader builds LVGL font/image objects (LVGL builtin TLSF
    // allocator) even though this UI never draws with LVGL: initialise it once.
    if (!lv_is_initialized()) lv_init();
#endif

    mutex_ = xSemaphoreCreateRecursiveMutex();
    dma_free_ = xSemaphoreCreateCounting(2, 2);
    for (auto& b : dma_buf_) {
        b = (uint16_t*)heap_caps_malloc(width * EYES_PUSH_LINES * sizeof(uint16_t),
                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (b == nullptr) ESP_LOGE(TAG, "Push buffer allocation failed");
    }

    esp_lcd_panel_io_callbacks_t cbs = {};
    cbs.on_color_trans_done = &EyesDisplay::OnColorTransDone;
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, this));

    canvas_.setBackground(EYES_COLOR_BG);
    canvas_.fillScreen(EYES_COLOR_BG);

    // RoboEyes setup sized for 320x240 (constants in dog_config.h)
    current_color_ = target_color_ = EYES_COLOR_MAIN;
    eyes_.begin(width, height, EYES_FPS);
    eyes_.setDisplayColors(EYES_COLOR_BG, current_color_);
    SetEyeSize(EYES_WIDTH, EYES_HEIGHT);
    eyes_.setBorderradius(EYES_RADIUS, EYES_RADIUS);
    eyes_.setSpacebetween(EYES_SPACE);
    eyes_.eyelidsHeightMax = EYES_HEIGHT / 2;
    eyes_.eyelidsHappyBottomOffsetMax = EYES_HEIGHT / 2 + 3;
    // Start centred (the library's member defaults assume 128x64).
    eyes_.setPosition(DEFAULT);
    eyes_.eyeLx = eyes_.eyeLxNext;
    eyes_.eyeLy = eyes_.eyeLyNext;
    eyes_.eyeRx = eyes_.eyeLx + EYES_WIDTH + EYES_SPACE;
    eyes_.eyeRy = eyes_.eyeLy;
    eyes_.setAutoblinker(ON, 3, 3);
    eyes_.sweatScale = EYES_SWEAT_SCALE;

    esp_lcd_panel_disp_on_off(panel_, true);

    xTaskCreatePinnedToCore(&EyesDisplay::RenderTaskEntry, "eyes", EYES_TASK_STACK, this,
                            EYES_TASK_PRIORITY, &task_, EYES_TASK_CORE);
}

EyesDisplay::~EyesDisplay() {
    if (debug_timer_) {
        esp_timer_stop(debug_timer_);
        esp_timer_delete(debug_timer_);
    }
    if (task_) vTaskDelete(task_);
    for (auto& b : dma_buf_) heap_caps_free(b);
    vSemaphoreDelete(dma_free_);
    vSemaphoreDelete(mutex_);
}

bool EyesDisplay::Lock(int timeout_ms) {
    TickType_t ticks = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(mutex_, ticks) == pdTRUE;
}

void EyesDisplay::Unlock() { xSemaphoreGiveRecursive(mutex_); }

// ---------------------------------------------------------------------------
// Display interface
// ---------------------------------------------------------------------------
void EyesDisplay::SetupUI() { Display::SetupUI(); }

void EyesDisplay::SetStatus(const char* status) {
    // No status bar on the eyes UI; keep it in the log.
    ESP_LOGI(TAG, "status: %s", status ? status : "");
}

void EyesDisplay::SetSubtitle(const char* text, uint16_t color, int duration_ms) {
    DisplayLockGuard lock(this);
    subtitle_ = text ? text : "";
    subtitle_color_ = color;
    subtitle_until_us_ = duration_ms > 0 ? esp_timer_get_time() + (int64_t)duration_ms * 1000 : 0;
    subtitle_dirty_ = true;
}

void EyesDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (notification == nullptr) return;
    ESP_LOGI(TAG, "notify: %s", notification);
    DisplayLockGuard lock(this);
    subtitle_sticky_ = false;
    SetSubtitle(notification, EYES_COLOR_TEXT, duration_ms);
}

void EyesDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) emotion = "neutral";
    ESP_LOGI(TAG, "emotion: %s", emotion);
    if (emotion_hook_) emotion_hook_(emotion);
}

void EyesDisplay::SetChatMessage(const char* role, const char* content) {
    if (role == nullptr) return;
    if (content == nullptr) content = "";
    if (chat_hook_) chat_hook_(role, content);
    DisplayLockGuard lock(this);
    if (strcmp(role, "system") == 0) {
        // Activation code, Wi-Fi provisioning hint, errors: stay until cleared.
        subtitle_sticky_ = content[0] != 0;
        SetSubtitle(content, EYES_COLOR_TEXT, 0);
        return;
    }
    if (subtitle_sticky_ || content[0] == 0) return;
    if (strcmp(role, "assistant") == 0) {
        SetSubtitle(content, EYES_COLOR_TEXT, EYES_CHAT_TEXT_MS);
    } else if (strcmp(role, "user") == 0) {
        SetSubtitle(content, EYES_COLOR_LISTEN, EYES_CHAT_TEXT_MS);
    }
}

void EyesDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    subtitle_sticky_ = false;
    if (subtitle_until_us_ == 0) {
        subtitle_.clear();
        subtitle_dirty_ = true;
    }
}

// ---------------------------------------------------------------------------
// Eyes control
// ---------------------------------------------------------------------------
void EyesDisplay::SetEyeSize(int w, int h) {
    eyes_.setWidth(w, w);
    eyes_.setHeight(h, h);
}

void EyesDisplay::SetCuriosity(bool on) {
    DisplayLockGuard lock(this);
    eyes_.setCuriosity(on);
}

void EyesDisplay::SetSweat(bool on) {
    DisplayLockGuard lock(this);
    eyes_.setSweat(on);
}

void EyesDisplay::SetSpeaking(bool on) {
    DisplayLockGuard lock(this);
    speaking_ = on;
    if (!on) eyes_.setPosition(DEFAULT);
}

void EyesDisplay::LookAt(unsigned char position) {
    DisplayLockGuard lock(this);
    eyes_.setIdleMode(OFF);
    eyes_.setPosition(position);
}

void EyesDisplay::Blink() {
    DisplayLockGuard lock(this);
    eyes_.blink();
}

void EyesDisplay::Wink() {
    DisplayLockGuard lock(this);
    eyes_.close(false, true);  // right eye
    wink_until_ms_ = now_ms() + 700;
}

void EyesDisplay::ApplyPreset(EyePreset p) {
    DisplayLockGuard lock(this);
    // Common reset for "where to look / how big"
    auto normal = [this]() {
        SetEyeSize(EYES_WIDTH, EYES_HEIGHT);
        eyes_.setCuriosity(OFF);
        eyes_.setHFlicker(OFF);
        eyes_.setVFlicker(OFF);
        eyes_.setAutoblinker(ON, 3, 3);
        eyes_.setSweat(OFF);
        shake_ = false;
    };
    auto set_fx = [this](uint16_t fx) {
        if (fx != fx_) fx_t0_ms_ = now_ms();
        fx_ = fx;
    };

    switch (p) {
        case EyePreset::kBoot:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_MAIN);
            eyes_.setMood(DEFAULT);
            eyes_.close();  // start closed, the library opens them smoothly
            eyes_.open();
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            break;
        case EyePreset::kIdle:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_MAIN);
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(ON, 2, 3);
            break;
        case EyePreset::kNeutral:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_MAIN);
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            break;
        case EyePreset::kListening:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_LISTEN);
            SetEyeSize(EYES_WIDE_WIDTH, EYES_WIDE_HEIGHT);
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            eyes_.setCuriosity(ON);
            break;
        case EyePreset::kThinking:
            normal();
            set_fx(kFxQuestion);
            SetTargetColor(EYES_COLOR_THINK);
            eyes_.setMood(DEFAULT);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(NW);
            eyes_.setAutoblinker(ON, 4, 3);
            break;
        case EyePreset::kSpeaking:
            // Keep the mood, colour and overlays set by the latest server emotion.
            eyes_.setCuriosity(OFF);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(DEFAULT);
            if (fx_ & kFxQuestion) set_fx(fx_ & ~kFxQuestion);
            break;
        case EyePreset::kHappy:
            normal();
            set_fx(kFxSparkle);
            SetTargetColor(EYES_COLOR_HAPPY);
            eyes_.setMood(HAPPY);
            break;
        case EyePreset::kLaugh:
            normal();
            set_fx(kFxNotes | kFxSparkle);
            SetTargetColor(EYES_COLOR_HAPPY);
            eyes_.setMood(HAPPY);
            eyes_.anim_laugh();
            break;
        case EyePreset::kLove:
            normal();
            set_fx(kFxHearts | kFxBlush);
            SetTargetColor(EYES_COLOR_LOVE);
            eyes_.setMood(HAPPY);
            break;
        case EyePreset::kKiss:
            normal();
            set_fx(kFxHearts | kFxBlush);
            SetTargetColor(EYES_COLOR_LOVE);
            eyes_.setMood(HAPPY);
            eyes_.close(false, true);
            wink_until_ms_ = now_ms() + 900;
            break;
        case EyePreset::kSad:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_SAD);
            eyes_.setMood(TIRED);
            eyes_.setPosition(S);
            break;
        case EyePreset::kCry:
            normal();
            set_fx(kFxTears);
            SetTargetColor(EYES_COLOR_SAD);
            eyes_.setMood(TIRED);
            eyes_.setPosition(DEFAULT);
            break;
        case EyePreset::kAngry:
            normal();
            set_fx(kFxAnger);
            SetTargetColor(EYES_COLOR_ANGRY);
            eyes_.setMood(ANGRY);
            eyes_.setPosition(DEFAULT);
            break;
        case EyePreset::kSurprised:
            normal();
            set_fx(kFxExclaim);
            SetTargetColor(EYES_COLOR_SURPRISED);
            SetEyeSize(EYES_WIDE_WIDTH, EYES_WIDE_HEIGHT);
            eyes_.setMood(DEFAULT);
            eyes_.setCuriosity(ON);
            break;
        case EyePreset::kShocked:
            normal();
            set_fx(kFxExclaim);
            SetTargetColor(EYES_COLOR_SURPRISED);
            SetEyeSize(EYES_WIDE_WIDTH, EYES_WIDE_HEIGHT);
            eyes_.setMood(DEFAULT);
            eyes_.setPosition(DEFAULT);
            shake_ = true;
            shake_until_ms_ = now_ms() + 600;
            break;
        case EyePreset::kConfused:
            normal();
            set_fx(kFxQuestion);
            SetTargetColor(EYES_COLOR_CONFUSED);
            eyes_.setMood(DEFAULT);
            eyes_.anim_confused();
            break;
        case EyePreset::kSleepy:
            normal();
            set_fx(kFxZzz);
            SetTargetColor(EYES_COLOR_SLEEPY);
            eyes_.setMood(TIRED);
            eyes_.setIdleMode(OFF);
            eyes_.setPosition(S);
            eyes_.setAutoblinker(ON, 6, 4);
            break;
        case EyePreset::kRelaxed:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_COOL);
            eyes_.setMood(HAPPY);
            eyes_.setAutoblinker(ON, 5, 3);
            break;
        case EyePreset::kEmbarrassed:
            normal();
            set_fx(kFxBlush);
            SetTargetColor(EYES_COLOR_LOVE);
            eyes_.setMood(TIRED);
            eyes_.setPosition(SW);
            eyes_.setSweat(ON);
            break;
        case EyePreset::kWink:
            normal();
            set_fx(kFxSparkle);
            SetTargetColor(EYES_COLOR_HAPPY);
            eyes_.setMood(HAPPY);
            eyes_.close(false, true);
            wink_until_ms_ = now_ms() + 900;
            break;
        case EyePreset::kCool:
            normal();
            set_fx(kFxGlasses);
            SetTargetColor(EYES_COLOR_COOL);
            eyes_.setMood(DEFAULT);
            eyes_.setPosition(DEFAULT);
            SetEyeSize(EYES_WIDTH, EYES_SQUINT_HEIGHT);
            break;
        case EyePreset::kSilly:
            normal();
            set_fx(kFxNotes);
            SetTargetColor(EYES_COLOR_SILLY);
            eyes_.setMood(HAPPY);
            eyes_.anim_confused();
            break;
        case EyePreset::kDelicious:
            normal();
            set_fx(kFxTongue | kFxSparkle);
            SetTargetColor(EYES_COLOR_HAPPY);
            eyes_.setMood(HAPPY);
            break;
        case EyePreset::kError:
            normal();
            set_fx(kFxNone);
            SetTargetColor(EYES_COLOR_ERROR);
            eyes_.setMood(DEFAULT);
            eyes_.setSweat(ON);
            break;
    }
}

void EyesDisplay::StartDebugCycle(int period_ms) {
    if (debug_timer_ != nullptr) return;
    esp_timer_create_args_t args = {};
    args.callback = [](void* arg) {
        auto* self = static_cast<EyesDisplay*>(arg);
        static const EyePreset order[] = {
            EyePreset::kIdle,      EyePreset::kHappy,    EyePreset::kLaugh,
            EyePreset::kLove,      EyePreset::kKiss,     EyePreset::kSad,
            EyePreset::kCry,       EyePreset::kAngry,    EyePreset::kSurprised,
            EyePreset::kShocked,   EyePreset::kConfused, EyePreset::kSleepy,
            EyePreset::kRelaxed,   EyePreset::kEmbarrassed, EyePreset::kWink,
            EyePreset::kCool,      EyePreset::kSilly,    EyePreset::kDelicious,
            EyePreset::kError,     EyePreset::kListening, EyePreset::kThinking,
        };
        int i = self->debug_index_++ % (int)(sizeof(order) / sizeof(order[0]));
        ESP_LOGI(TAG, "debug preset %d, %.1f fps", i, self->fps_);
        self->ApplyPreset(order[i]);
    };
    args.arg = this;
    args.name = "eyes_debug";
    esp_timer_create(&args, &debug_timer_);
    esp_timer_start_periodic(debug_timer_, (uint64_t)period_ms * 1000);
}

// ---------------------------------------------------------------------------
// Overlay primitives
// ---------------------------------------------------------------------------
void EyesDisplay::FillCircle(int cx, int cy, int r, uint16_t color) {
    if (r <= 0) return;
    canvas_.fillRoundRect(cx - r, cy - r, 2 * r + 1, 2 * r + 1, r, color);
}

void EyesDisplay::DrawHeart(int cx, int cy, int s, uint16_t color) {
    if (s < 3) return;
    int r = s / 2;
    FillCircle(cx - r, cy, r, color);
    FillCircle(cx + r, cy, r, color);
    canvas_.fillTriangle(cx - 2 * r, cy + r / 3, cx + 2 * r, cy + r / 3, cx, cy + 2 * r + r / 2,
                         color);
}

void EyesDisplay::DrawSparkle(int cx, int cy, int s, uint16_t color) {
    if (s < 2) return;
    int t = std::max(1, s / 4);
    canvas_.fillTriangle(cx - t, cy, cx + t, cy, cx, cy - s, color);
    canvas_.fillTriangle(cx - t, cy, cx + t, cy, cx, cy + s, color);
    canvas_.fillTriangle(cx, cy - t, cx, cy + t, cx - s, cy, color);
    canvas_.fillTriangle(cx, cy - t, cx, cy + t, cx + s, cy, color);
}

void EyesDisplay::DrawNote(int x, int y, int s, uint16_t color) {
    // Eighth note: head at bottom-left, stem, flag.
    int r = s / 3;
    FillCircle(x, y, r, color);
    canvas_.fillRect(x + r - 2, y - s - s / 2, 3, s + s / 2, color);
    canvas_.fillTriangle(x + r, y - s - s / 2, x + r + s / 2, y - s, x + r, y - s, color);
}

void EyesDisplay::DrawAngerMark(int cx, int cy, int s, uint16_t color) {
    // Four bent strokes around a centre point (manga "vein" mark).
    int g = s / 4, l = s / 2, w = std::max(3, s / 6);
    canvas_.fillRect(cx - g - l, cy - g - w, l, w, color);
    canvas_.fillRect(cx - g - w, cy - g - l, w, l, color);
    canvas_.fillRect(cx + g, cy - g - w, l, w, color);
    canvas_.fillRect(cx + g, cy - g - l, w, l, color);
    canvas_.fillRect(cx - g - l, cy + g, l, w, color);
    canvas_.fillRect(cx - g - w, cy + g, w, l, color);
    canvas_.fillRect(cx + g, cy + g, l, w, color);
    canvas_.fillRect(cx + g, cy + g, w, l, color);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void EyesDisplay::RenderTaskEntry(void* arg) { static_cast<EyesDisplay*>(arg)->RenderLoop(); }

void EyesDisplay::UpdateColor() {
    if (current_color_ == target_color_) return;
    current_color_ = fade565(current_color_, target_color_, EYES_COLOR_FADE_STEP);
    eyes_.setDisplayColors(EYES_COLOR_BG, current_color_);
}

void EyesDisplay::UpdateTimedAnims(int64_t now) {
    if (wink_until_ms_ != 0 && now > wink_until_ms_) {
        wink_until_ms_ = 0;
        eyes_.open(true, true);
    }
    if (shake_) {
        if (now > shake_until_ms_) {
            shake_ = false;
            eyes_.setHFlicker(OFF);
        } else {
            eyes_.setHFlicker(ON, 6);
        }
    }
    // Gentle bob while talking: eyes float a few pixels up and down.
    if (speaking_ && !eyes_.idle) {
        int base_y = eyes_.getScreenConstraint_Y() / 2;
        int bob = (int)(tri(now, 700) * 8.0f) - 4;
        eyes_.eyeLyNext = base_y + bob;
    }
}

void EyesDisplay::DrawFx(int64_t now) {
    if (fx_ == kFxNone) return;
    const int t = (int)(now - fx_t0_ms_);
    const int lx = eyes_.eyeLx, ly = eyes_.eyeLy;
    const int lw = eyes_.eyeLwidthCurrent, lh = eyes_.eyeLheightCurrent;
    const int rx = eyes_.eyeRx, ry = eyes_.eyeRy;
    const int rw = eyes_.eyeRwidthCurrent, rh = eyes_.eyeRheightCurrent;
    const int eyes_top = std::min(ly, ry);
    const int eyes_bottom = std::max(ly + lh, ry + rh);
    const int cx = (lx + rx + rw) / 2;

    if (fx_ & kFxGlasses) {
        // Frames a bit larger than the eyes, dark lenses, white glint, bridge.
        const int pad = 8;
        int gy = eyes_top - pad;
        int gh = std::max(lh, rh) + 2 * pad;
        canvas_.fillRoundRect(lx - pad, gy, lw + 2 * pad, gh, 14, 0x7BEF);
        canvas_.fillRoundRect(rx - pad, gy, rw + 2 * pad, gh, 14, 0x7BEF);
        canvas_.fillRoundRect(lx - pad + 4, gy + 4, lw + 2 * pad - 8, gh - 8, 11, FX_COLOR_GLASSES);
        canvas_.fillRoundRect(rx - pad + 4, gy + 4, rw + 2 * pad - 8, gh - 8, 11, FX_COLOR_GLASSES);
        canvas_.fillRect(lx + lw + pad, gy + 10, rx - lx - lw - 2 * pad, 6, 0x7BEF);
        int glint = (int)(saw(t, 3000) * 160.0f) - 30;  // sweeping glint
        if (glint > 0 && glint < lw) {
            canvas_.fillTriangle(lx + glint, gy + 6, lx + glint + 10, gy + 6, lx + glint - 8,
                                 gy + gh - 6, 0xFFFF);
        }
    }

    if (fx_ & kFxBlush) {
        int by = eyes_bottom + 6;
        canvas_.fillRoundRect(lx - 4, by, 40, 12, 6, FX_COLOR_BLUSH);
        canvas_.fillRoundRect(rx + rw - 36, by, 40, 12, 6, FX_COLOR_BLUSH);
    }

    if (fx_ & kFxTears) {
        // Two drops falling from the inner bottom of each eye.
        for (int i = 0; i < 2; ++i) {
            float p = saw(t, 1200, i * 600);
            int ex = i == 0 ? lx + lw / 2 + 10 : rx + rw / 2 - 10;
            int ey = (i == 0 ? ly + lh : ry + rh) + (int)(p * 60.0f);
            int sz = 6 + (int)(p * 4.0f);
            canvas_.fillTriangle(ex - sz, ey, ex + sz, ey, ex, ey - sz * 2, FX_COLOR_TEAR);
            FillCircle(ex, ey, sz, FX_COLOR_TEAR);
        }
    }

    if (fx_ & kFxHearts) {
        // Three hearts floating up beside the eyes, pulsing.
        static const int kx[3] = {-40, 0, 40};
        for (int i = 0; i < 3; ++i) {
            float p = saw(t, 2400, i * 800);
            int hx = (i == 1 ? cx : (kx[i] < 0 ? lx - 18 : rx + rw + 18));
            int hy = eyes_bottom + 10 - (int)(p * (eyes_bottom + 10));
            int s = 10 + (int)(tri(t, 600, i * 200) * 5.0f);
            if (i == 1) {
                hy = eyes_top - 14 - (int)(p * 20.0f);
                s = 7 + (int)(tri(t, 600) * 3.0f);
            }
            DrawHeart(hx, hy, s, FX_COLOR_HEART);
        }
    }

    if (fx_ & kFxAnger) {
        int s = 26 + (int)(tri(t, 500) * 8.0f);
        DrawAngerMark(rx + rw + 4, eyes_top - 8, s, FX_COLOR_ANGER);
    }

    if (fx_ & kFxZzz) {
        // Three Z's of growing size drifting up-right.
        for (int i = 0; i < 3; ++i) {
            float p = saw(t, 3000, i * 1000);
            int scale = 1 + (int)(p * 3.0f);
            int zx = rx + rw - 10 + (int)(p * 40.0f);
            int zy = eyes_top - 10 - (int)(p * 50.0f);
            if (zy < 0) continue;
            canvas_.drawText(zx, zy, "Z", FX_COLOR_ZZZ, scale);
        }
    }

    if (fx_ & kFxQuestion) {
        int qy = std::max(0, eyes_top - 60) + (int)(tri(t, 900) * 8.0f);
        canvas_.drawText(rx + rw + 6, qy, "?", FX_COLOR_SYMBOL, 3);
    }

    if (fx_ & kFxExclaim) {
        int jitter = (t / 60) % 2 == 0 ? -2 : 2;
        int ey = std::max(0, eyes_top - 60);
        canvas_.drawText(rx + rw + 8 + (t < 1500 ? jitter : 0), ey, "!", FX_COLOR_SYMBOL, 3);
    }

    if (fx_ & kFxSparkle) {
        // Twinkling stars around the eyes.
        static const int kPos[4][2] = {{-26, -10}, {24, -18}, {-20, 30}, {30, 34}};
        for (int i = 0; i < 4; ++i) {
            int s = (int)(tri(t, 900, i * 230) * 12.0f);
            int px = i % 2 == 0 ? lx + kPos[i][0] : rx + rw + kPos[i][0];
            int py = eyes_top + kPos[i][1];
            DrawSparkle(px, py, s, FX_COLOR_SPARKLE);
        }
    }

    if (fx_ & kFxNotes) {
        for (int i = 0; i < 2; ++i) {
            float p = saw(t, 1800, i * 900);
            int nx = (i == 0 ? lx - 22 : rx + rw + 22) + (int)(sinf(p * 6.28f) * 6.0f);
            int ny = eyes_bottom + 10 - (int)(p * (eyes_bottom - 10));
            DrawNote(nx, ny, 14, FX_COLOR_NOTE);
        }
    }

    if (fx_ & kFxTongue) {
        int ty = eyes_bottom + 14;
        int wob = (int)(tri(t, 800) * 6.0f);
        canvas_.fillRect(cx - 30, ty, 60, 4, FX_COLOR_BLUSH);
        canvas_.fillRoundRect(cx - 12, ty, 24, 18 + wob, 10, FX_COLOR_BLUSH);
    }
}

void EyesDisplay::LayoutSubtitle() {
    subtitle_dirty_ = false;
    subtitle_lines_.clear();
    if (subtitle_.empty()) return;

    // Split into UTF-8 code points, wrap by character count.
    std::vector<std::string> cps;
    cps.reserve(subtitle_.size());
    for (size_t i = 0; i < subtitle_.size();) {
        unsigned char c = subtitle_[i];
        size_t n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        if (c == '\n') {
            cps.emplace_back(" ");
            i++;
            continue;
        }
        cps.emplace_back(subtitle_.substr(i, n));
        i += n;
    }
    int scale = EYES_TEXT_SCALE;
    int per_line = width_ / (Rgb565Canvas::glyphW() * scale);
    int max_lines = EYES_TEXT_MAX_LINES;
    if ((int)cps.size() > per_line * max_lines) {  // too long: use the small font
        scale = 1;
        per_line = width_ / Rgb565Canvas::glyphW();
        max_lines = EYES_TEXT_MAX_LINES * 2;
    }
    // Word wrap: break at the last space of a full line when there is one.
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < cps.size()) {
        size_t end = std::min(cps.size(), start + per_line);
        if (end < cps.size()) {
            size_t sp = end;
            while (sp > start && cps[sp] != " ") --sp;
            if (sp > start) end = sp;
        }
        std::string line;
        for (size_t k = start; k < end; ++k) line += cps[k];
        lines.push_back(line);
        start = end;
        while (start < cps.size() && cps[start] == " ") ++start;
    }
    // Keep the tail: the activation code is at the end of the message.
    size_t first = lines.size() > (size_t)max_lines ? lines.size() - max_lines : 0;
    subtitle_lines_.assign(lines.begin() + first, lines.end());
    subtitle_scale_ = scale;
}

void EyesDisplay::DrawSubtitle() {
    if (subtitle_until_us_ != 0 && esp_timer_get_time() > subtitle_until_us_) {
        subtitle_.clear();
        subtitle_until_us_ = 0;
        subtitle_dirty_ = true;
    }
    if (subtitle_dirty_) LayoutSubtitle();
    if (subtitle_lines_.empty()) return;

    int lh = Rgb565Canvas::glyphH() * subtitle_scale_;
    int nlines = (int)subtitle_lines_.size();
    int y = height_ - nlines * lh - 2;
    canvas_.fillRect(0, y - 2, width_, nlines * lh + 4, EYES_COLOR_BG);
    for (const auto& line : subtitle_lines_) {
        int w = Rgb565Canvas::textWidth(line.c_str(), subtitle_scale_);
        canvas_.drawText((width_ - w) / 2, y, line.c_str(), subtitle_color_, subtitle_scale_);
        y += lh;
    }
}

bool EyesDisplay::OnColorTransDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*,
                                   void* user_ctx) {
    auto* self = static_cast<EyesDisplay*>(user_ctx);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(self->dma_free_, &woken);
    return woken == pdTRUE;
}

void EyesDisplay::Push(const DirtyBox& box) {
    if (box.empty() || dma_buf_[0] == nullptr || dma_buf_[1] == nullptr) return;
    const uint16_t* fb = canvas_.buffer();
    int w = box.x1 - box.x0 + 1;
    int lines_per_chunk = (width_ * EYES_PUSH_LINES) / w;
    for (int y = box.y0; y <= box.y1; y += lines_per_chunk) {
        int ye = std::min(y + lines_per_chunk - 1, box.y1);
        xSemaphoreTake(dma_free_, portMAX_DELAY);
        uint16_t* dst = dma_buf_[dma_next_];
        dma_next_ ^= 1;
        for (int yy = y; yy <= ye; ++yy) {
            memcpy(dst + (yy - y) * w, fb + (size_t)yy * width_ + box.x0, w * sizeof(uint16_t));
        }
        if (esp_lcd_panel_draw_bitmap(panel_, box.x0, y, box.x1 + 1, ye + 1, dst) != ESP_OK) {
            xSemaphoreGive(dma_free_);  // no transfer queued, no callback will come
        }
    }
}

void EyesDisplay::RenderLoop() {
    const TickType_t period = pdMS_TO_TICKS(1000 / EYES_FPS);
    TickType_t last = xTaskGetTickCount();
    int frames = 0;
    int64_t fps_t0 = esp_timer_get_time();
    if (!canvas_.ok()) {
        ESP_LOGE(TAG, "No frame buffer, eyes disabled");
        vTaskDelete(nullptr);
    }
    while (true) {
        DirtyBox box;
        if (Lock(1000)) {
            int64_t now = now_ms();
            UpdateColor();
            UpdateTimedAnims(now);
            eyes_.drawEyes();
            DrawFx(now);
            DrawSubtitle();
            box = canvas_.takeDirty();
            Unlock();
        }
        Push(box);

        if (++frames == 60) {
            int64_t now = esp_timer_get_time();
            fps_ = 60.0f * 1e6f / (float)(now - fps_t0);
            fps_t0 = now;
            frames = 0;
        }
        // If a frame overran, do not try to catch up (no busy loop).
        if (xTaskGetTickCount() - last >= period) last = xTaskGetTickCount() - period + 1;
        vTaskDelayUntil(&last, period);
    }
}
