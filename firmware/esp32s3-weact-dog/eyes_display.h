// EyesDisplay: replaces the stock LVGL chat UI with animated RoboEyes on an
// SPI RGB565 panel. Rendering runs in its own low-priority task.
// On top of the eyes it draws small animated overlays (hearts, tears, blush,
// Zzz, "?", "!", sparkles, sunglasses, music notes) and fades the eye colour
// per mood.
#pragma once

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "display/display.h"
#include "rgb565_canvas.h"
#include "roboeyes_port.h"

// High-level looks. The mapping from device state / server emotion to these
// presets lives in dog_behavior.cc (one table).
enum class EyePreset : uint8_t {
    kBoot,         // open from closed, DEFAULT
    kIdle,         // DEFAULT, autoblink, idle drift
    kListening,    // wide, centred, curiosity on
    kThinking,     // look up-left, "?" overlay
    kSpeaking,     // keep current mood, centred, gentle bob
    kNeutral,      // DEFAULT, centred, autoblink (no drift)
    kHappy,        // HAPPY, sparkles
    kLaugh,        // HAPPY + anim_laugh() + music notes
    kLove,         // HAPPY, pink, floating hearts
    kKiss,         // wink + hearts + blush
    kSad,          // TIRED (droopy), blue
    kCry,          // TIRED + falling tears
    kAngry,        // ANGRY, red, anger mark
    kSurprised,    // wide + curiosity, "!"
    kShocked,      // wide, white, "!" + shake
    kConfused,     // anim_confused(), "?"
    kSleepy,       // TIRED + slow blink + Zzz
    kRelaxed,      // HAPPY-ish half closed, slow blink
    kEmbarrassed,  // look down, blush + sweat
    kWink,         // one eye closes for a moment
    kCool,         // sunglasses
    kSilly,        // orange, wobble, music notes
    kDelicious,    // HAPPY + tongue
    kError,        // sweat on, orange
};

class EyesDisplay : public Display {
public:
    // Hook so the board's behaviour controller sees what the core sends.
    using EmotionHook = std::function<void(const char* emotion)>;
    using ChatHook = std::function<void(const char* role, const char* content)>;

    EyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                int height);
    ~EyesDisplay() override;

    // ---- Display interface ------------------------------------------------
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void ShowNotification(const std::string& notification, int duration_ms = 3000) override {
        ShowNotification(notification.c_str(), duration_ms);
    }
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetupUI() override;

    // ---- Eyes control (thread-safe) ---------------------------------------
    void ApplyPreset(EyePreset preset);
    void SetCuriosity(bool on);
    void SetSweat(bool on);
    void SetSpeaking(bool on);  // gentle eye bob while the robot talks
    void LookAt(unsigned char position);  // one of roboeyes N/NE/E/... DEFAULT
    void Blink();
    void Wink();
    void SetEmotionHook(EmotionHook hook) { emotion_hook_ = std::move(hook); }
    void SetChatHook(ChatHook hook) { chat_hook_ = std::move(hook); }

    // Debug: cycle through all presets on a timer (milestone M2 check).
    void StartDebugCycle(int period_ms);
    float measured_fps() const { return fps_; }

protected:
    bool Lock(int timeout_ms = 0) override;
    void Unlock() override;

private:
    // Overlay effects, combined as a bit mask.
    enum Fx : uint16_t {
        kFxNone = 0,
        kFxHearts = 1 << 0,
        kFxTears = 1 << 1,
        kFxBlush = 1 << 2,
        kFxAnger = 1 << 3,
        kFxZzz = 1 << 4,
        kFxQuestion = 1 << 5,
        kFxExclaim = 1 << 6,
        kFxSparkle = 1 << 7,
        kFxGlasses = 1 << 8,
        kFxNotes = 1 << 9,
        kFxTongue = 1 << 10,
    };

    static void RenderTaskEntry(void* arg);
    void RenderLoop();
    void DrawSubtitle();
    void LayoutSubtitle();
    void SetSubtitle(const char* text, uint16_t color, int duration_ms);
    void DrawFx(int64_t now_ms);
    void UpdateColor();
    void UpdateTimedAnims(int64_t now_ms);
    void Push(const DirtyBox& box);
    void SetEyeSize(int w, int h);
    void SetTargetColor(uint16_t color) { target_color_ = color; }

    // Overlay primitives
    void FillCircle(int cx, int cy, int r, uint16_t color);
    void DrawHeart(int cx, int cy, int s, uint16_t color);
    void DrawSparkle(int cx, int cy, int s, uint16_t color);
    void DrawNote(int x, int y, int s, uint16_t color);
    void DrawAngerMark(int cx, int cy, int s, uint16_t color);

    static bool OnColorTransDone(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t* edata, void* user_ctx);

    esp_lcd_panel_io_handle_t panel_io_;
    esp_lcd_panel_handle_t panel_;
    Rgb565Canvas canvas_;
    roboeyes::RoboEyes<Rgb565Canvas> eyes_;

    SemaphoreHandle_t mutex_ = nullptr;      // recursive: guards eyes_, canvas_, text
    SemaphoreHandle_t dma_free_ = nullptr;   // counting: free push buffers
    uint16_t* dma_buf_[2] = {nullptr, nullptr};
    int dma_next_ = 0;
    TaskHandle_t task_ = nullptr;

    // Subtitle (text is laid out once per change, not every frame)
    std::string subtitle_;
    uint16_t subtitle_color_ = 0xFFFF;
    int64_t subtitle_until_us_ = 0;  // 0 = no expiry
    bool subtitle_sticky_ = false;   // system message (activation code, Wi-Fi hint)
    std::vector<std::string> subtitle_lines_;
    int subtitle_scale_ = 1;
    bool subtitle_dirty_ = true;

    // Mood colour fade
    uint16_t current_color_ = 0;
    uint16_t target_color_ = 0;

    // Overlays and timed animations
    uint16_t fx_ = kFxNone;
    int64_t fx_t0_ms_ = 0;
    bool speaking_ = false;
    bool shake_ = false;
    int64_t wink_until_ms_ = 0;
    int64_t shake_until_ms_ = 0;

    EmotionHook emotion_hook_;
    ChatHook chat_hook_;
    esp_timer_handle_t debug_timer_ = nullptr;
    int debug_index_ = 0;
    volatile float fps_ = 0;
};
