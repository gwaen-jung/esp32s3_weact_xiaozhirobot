// LegServos: 4 symbolic servo "legs" with hard power-safety rules:
//  - at most SERVO_MAX_MOVING servos in motion, start-ups staggered
//  - slew-rate limited ramps, never an instant jump (except the very first attach)
//  - PWM detached after a pose is held SERVO_DETACH_AFTER_MS
//  - runs in its own task; Play() never blocks the caller
#pragma once

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>

#include "dog_config.h"

enum class LegMotion : uint8_t {
    kNone,
    kStand,
    kQuickStand,  // stand, but with no pauses between keyframes
    kSit,
    kCrouch,
    kWiggle,      // alternating rear shake
    kSoftWiggle,  // small front shake (while speaking)
    kWave,        // front-left paw wave
    kBreathe,     // very small slow motion
    kRelax,       // stop PWM on all legs now
};

const char* LegMotionName(LegMotion m);

class LegServos {
public:
    LegServos();
    // Creates the task. Servos stay unpowered until AttachSequential().
    void Begin();
    // Sequentially attach every servo at neutral (call once Wi-Fi + audio are up).
    void AttachSequential();
    // Replace whatever is playing with this motion (non-blocking).
    void Play(LegMotion motion);
    bool attached() const { return initialized_; }
    bool busy() const { return busy_; }

private:
    struct Keyframe {
        uint8_t angle[LEG_COUNT];  // logical degrees; 0 = keep current
        uint16_t hold_ms;
    };
    struct Leg {
        float current = SERVO_NEUTRAL_DEG;
        float target = SERVO_NEUTRAL_DEG;
        bool attached = false;
        bool moving = false;
        int64_t settled_ms = 0;
        ledc_channel_t channel = LEDC_CHANNEL_0;
    };
    enum Cmd : uint8_t { kCmdAttach = 0xF0 };

    static void TaskEntry(void* arg);
    void Loop();
    void LoadMotion(LegMotion m);
    void WriteAngle(int leg, float logical_deg);
    void Detach(int leg);
    static int64_t NowMs();

    Leg legs_[LEG_COUNT];
    const Keyframe* frames_ = nullptr;
    int frame_count_ = 0;
    int frame_index_ = 0;
    bool frame_active_ = false;
    bool quick_ = false;
    int64_t frame_settled_ms_ = 0;
    int64_t last_start_ms_ = 0;
    volatile bool initialized_ = false;
    volatile bool busy_ = false;
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
};
