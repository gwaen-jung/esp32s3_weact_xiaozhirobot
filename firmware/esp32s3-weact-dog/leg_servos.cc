#include "leg_servos.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cmath>

#define TAG "LegServos"

// ---------------------------------------------------------------------------
// Motion library (logical degrees, 90 = neutral; 0 = keep this leg as is).
// Order of legs: FL, FR, RL, RR.
// ---------------------------------------------------------------------------
namespace {
struct Motion {
    const void* frames;
    int count;
    bool quick;
};
#define KF(fl, fr, rl, rr, hold) {{fl, fr, rl, rr}, hold}

struct KeyframeDef {
    uint8_t angle[LEG_COUNT];
    uint16_t hold_ms;
};

const KeyframeDef kStand[] = {KF(90, 90, 90, 90, 0)};
const KeyframeDef kSit[] = {KF(0, 0, 125, 125, 200), KF(100, 100, 0, 0, 0)};
const KeyframeDef kCrouch[] = {KF(65, 65, 65, 65, 0)};
const KeyframeDef kWiggle[] = {
    KF(90, 90, 90, 90, 0),  KF(0, 0, 80, 100, 40), KF(0, 0, 100, 80, 40),
    KF(0, 0, 80, 100, 40),  KF(0, 0, 100, 80, 40), KF(0, 0, 80, 100, 40),
    KF(0, 0, 100, 80, 40),  KF(0, 0, 90, 90, 0),
};
const KeyframeDef kSoftWiggle[] = {
    KF(96, 84, 0, 0, 60), KF(84, 96, 0, 0, 60), KF(96, 84, 0, 0, 60), KF(90, 90, 0, 0, 0),
};
const KeyframeDef kWave[] = {
    KF(90, 90, 90, 90, 0), KF(130, 0, 0, 0, 120), KF(110, 0, 0, 0, 40), KF(130, 0, 0, 0, 40),
    KF(110, 0, 0, 0, 40),  KF(130, 0, 0, 0, 120), KF(90, 0, 0, 0, 0),
};
const KeyframeDef kBreathe[] = {
    KF(94, 94, 0, 0, 400), KF(0, 0, 94, 94, 400), KF(90, 90, 0, 0, 300), KF(0, 0, 90, 90, 0),
};
#undef KF
#define MOTION(arr, quick) Motion{arr, (int)(sizeof(arr) / sizeof(arr[0])), quick}

Motion MotionFor(LegMotion m) {
    switch (m) {
        case LegMotion::kStand: return MOTION(kStand, false);
        case LegMotion::kQuickStand: return MOTION(kStand, true);
        case LegMotion::kSit: return MOTION(kSit, false);
        case LegMotion::kCrouch: return MOTION(kCrouch, false);
        case LegMotion::kWiggle: return MOTION(kWiggle, false);
        case LegMotion::kSoftWiggle: return MOTION(kSoftWiggle, false);
        case LegMotion::kWave: return MOTION(kWave, false);
        case LegMotion::kBreathe: return MOTION(kBreathe, false);
        default: return Motion{nullptr, 0, false};
    }
}
}  // namespace

const char* LegMotionName(LegMotion m) {
    switch (m) {
        case LegMotion::kNone: return "none";
        case LegMotion::kStand: return "stand";
        case LegMotion::kQuickStand: return "quick_stand";
        case LegMotion::kSit: return "sit";
        case LegMotion::kCrouch: return "crouch";
        case LegMotion::kWiggle: return "wiggle";
        case LegMotion::kSoftWiggle: return "soft_wiggle";
        case LegMotion::kWave: return "wave";
        case LegMotion::kBreathe: return "idle_breathe";
        case LegMotion::kZero: return "zero";
        case LegMotion::kRelax: return "relax";
    }
    return "?";
}

static_assert(sizeof(KeyframeDef) == sizeof(uint8_t) * LEG_COUNT + sizeof(uint16_t),
              "keyframe layout");

LegServos::LegServos() {
    for (int i = 0; i < LEG_COUNT; ++i) {
        legs_[i].channel = (ledc_channel_t)(SERVO_LEDC_FIRST_CHANNEL + i);
    }
}

int64_t LegServos::NowMs() { return esp_timer_get_time() / 1000; }

void LegServos::Begin() {
    if (task_ != nullptr) return;
    LoadSettings();
    queue_ = xQueueCreate(4, sizeof(uint8_t));
    xTaskCreate(&LegServos::TaskEntry, "legs", SERVO_TASK_STACK, this, SERVO_TASK_PRIORITY, &task_);
}

void LegServos::AttachSequential() {
    if (queue_ == nullptr) return;
    uint8_t cmd = kCmdAttach;
    xQueueSend(queue_, &cmd, 0);
}

void LegServos::Play(LegMotion motion) {
    if (queue_ == nullptr || motion == LegMotion::kNone) return;
    uint8_t cmd = (uint8_t)motion;
    if (xQueueSend(queue_, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "motion queue full, dropping %s", LegMotionName(motion));
    }
}

void LegServos::WriteAngle(int leg, float logical) {
    const LegConfig& cfg = kLegConfig[leg];
    float phys;
    if (cfg.is_continuous_360) {
        // Continuous 360° servo: 90 deg = stop.
        // Cho phép tinh chỉnh vùng chết 80..100 độ để dừng tuyệt đối, không cho quay tít
        float clamped = std::fmin(std::fmax(logical, 80.0f), 100.0f);
        phys = clamped + cfg.trim_deg;
    } else {
        // 180° servo: clamp the logical angle, apply mount direction + trim, clamp again.
        logical = std::fmin(std::fmax(logical, (float)SERVO_MIN_DEG), (float)SERVO_MAX_DEG);
        if (logical == 0.0f) {
            phys = 0.0f + cfg.trim_deg; // 0 độ hiệu chuẩn tuyệt đối cho toàn bộ servo
        } else {
            phys = cfg.invert ? 180.0f - logical : logical;
            phys += cfg.trim_deg;
        }
        phys = std::fmin(std::fmax(phys, (float)SERVO_MIN_DEG), (float)SERVO_MAX_DEG);
    }

    float pulse_us = SERVO_PULSE_MIN_US + (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) * phys / 180.0f;
    uint32_t period_us = 1000000 / SERVO_PWM_HZ;
    uint32_t duty = (uint32_t)(pulse_us * (float)(1 << SERVO_PWM_BITS) / (float)period_us);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, legs_[leg].channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, legs_[leg].channel);  // also re-enables output
    legs_[leg].attached = true;
}

void LegServos::Detach(int leg) {
    ledc_stop(LEDC_LOW_SPEED_MODE, legs_[leg].channel, 0);
    legs_[leg].attached = false;
}

void LegServos::SetLegAngle(int leg, float deg) {
    if (!initialized_ || leg < 0 || leg >= LEG_COUNT) return;
    frames_ = nullptr;
    frame_count_ = 0;
    legs_[leg].target = deg;
    legs_[leg].moving = true;
    if (!legs_[leg].attached) WriteAngle(leg, legs_[leg].current);
    busy_ = true;
}

float LegServos::GetLegAngle(int leg) const {
    if (leg < 0 || leg >= LEG_COUNT) return 90.0f;
    return legs_[leg].target;
}

uint8_t LegServos::GetStandAngle(int leg) const {
    if (leg < 0 || leg >= LEG_COUNT) return 90;
    return stand_angle_[leg];
}

void LegServos::LoadSettings() {
    Settings settings("dog_legs");
    stand_angle_[0] = (uint8_t)settings.GetInt("stand_fl", SERVO_STAND_FL_DEG);
    stand_angle_[1] = (uint8_t)settings.GetInt("stand_fr", SERVO_STAND_FR_DEG);
    stand_angle_[2] = (uint8_t)settings.GetInt("stand_rl", SERVO_STAND_RL_DEG);
    stand_angle_[3] = (uint8_t)settings.GetInt("stand_rr", SERVO_STAND_RR_DEG);
    ESP_LOGI(TAG, "Loaded stand angles from NVS: FL=%d, FR=%d, RL=%d, RR=%d",
             stand_angle_[0], stand_angle_[1], stand_angle_[2], stand_angle_[3]);
}

void LegServos::SaveStandPose() {
    Settings settings("dog_legs", true);
    for (int i = 0; i < LEG_COUNT; ++i) {
        stand_angle_[i] = (uint8_t)std::round(legs_[i].target);
    }
    settings.SetInt("stand_fl", stand_angle_[0]);
    settings.SetInt("stand_fr", stand_angle_[1]);
    settings.SetInt("stand_rl", stand_angle_[2]);
    settings.SetInt("stand_rr", stand_angle_[3]);
    ESP_LOGI(TAG, "Saved stand angles to NVS: FL=%d, FR=%d, RL=%d, RR=%d",
             stand_angle_[0], stand_angle_[1], stand_angle_[2], stand_angle_[3]);
}

void LegServos::ResetStandPose() {
    Settings settings("dog_legs", true);
    settings.EraseAll();
    stand_angle_[0] = SERVO_STAND_FL_DEG;
    stand_angle_[1] = SERVO_STAND_FR_DEG;
    stand_angle_[2] = SERVO_STAND_RL_DEG;
    stand_angle_[3] = SERVO_STAND_RR_DEG;
    ESP_LOGI(TAG, "Reset stand angles to defaults (FL=%d, FR=%d, RL=%d, RR=%d)",
             stand_angle_[0], stand_angle_[1], stand_angle_[2], stand_angle_[3]);
    Play(LegMotion::kStand);
}

void LegServos::LoadMotion(LegMotion m) {
    if (m == LegMotion::kStand || m == LegMotion::kQuickStand) {
        ESP_LOGI(TAG, "play stand (FL=%d, FR=%d, RL=%d, RR=%d)",
                 stand_angle_[0], stand_angle_[1], stand_angle_[2], stand_angle_[3]);
        frames_ = nullptr;
        frame_count_ = 0;
        for (int i = 0; i < LEG_COUNT; ++i) {
            legs_[i].target = stand_angle_[i];
            legs_[i].moving = true;
        }
        quick_ = (m == LegMotion::kQuickStand);
        busy_ = true;
        return;
    }
    if (m == LegMotion::kZero) {
        ESP_LOGI(TAG, "play zero (dua chan 180 ve 0 do, chan 360 giu dung)");
        frames_ = nullptr;
        frame_count_ = 0;
        for (int i = 0; i < LEG_COUNT; ++i) {
            legs_[i].target = kLegConfig[i].is_continuous_360 ? stand_angle_[i] : 0;
            legs_[i].moving = true;
        }
        busy_ = true;
        return;
    }
    if (m == LegMotion::kSit) {
        ESP_LOGI(TAG, "play sit (chan 360 giu dung, chan 180 nghieng ngoi)");
        frames_ = nullptr;
        frame_count_ = 0;
        legs_[0].target = std::fmin((float)stand_angle_[0] + 15.0f, 180.0f);
        legs_[1].target = stand_angle_[1];
        legs_[2].target = stand_angle_[2];
        legs_[3].target = std::fmin((float)stand_angle_[3] + 35.0f, 180.0f);
        for (int i = 0; i < LEG_COUNT; ++i) legs_[i].moving = true;
        busy_ = true;
        return;
    }
    if (m == LegMotion::kCrouch) {
        ESP_LOGI(TAG, "play crouch (chan 360 giu dung, chan 180 cui)");
        frames_ = nullptr;
        frame_count_ = 0;
        legs_[0].target = std::fmax((float)stand_angle_[0] - 8.0f, 0.0f);
        legs_[1].target = stand_angle_[1];
        legs_[2].target = stand_angle_[2];
        legs_[3].target = std::fmax((float)stand_angle_[3] - 25.0f, 0.0f);
        for (int i = 0; i < LEG_COUNT; ++i) legs_[i].moving = true;
        busy_ = true;
        return;
    }
    if (m == LegMotion::kRelax) {
        frames_ = nullptr;
        frame_count_ = 0;
        for (int i = 0; i < LEG_COUNT; ++i) {
            legs_[i].target = legs_[i].current;
            legs_[i].moving = false;
            if (legs_[i].attached) Detach(i);
        }
        busy_ = false;
        return;
    }
    Motion mo = MotionFor(m);
    if (mo.frames == nullptr) return;
    ESP_LOGI(TAG, "play %s", LegMotionName(m));
    frames_ = static_cast<const Keyframe*>(mo.frames);
    frame_count_ = mo.count;
    frame_index_ = 0;
    frame_active_ = false;
    frame_settled_ms_ = 0;
    quick_ = mo.quick;
    busy_ = true;
}

void LegServos::TaskEntry(void* arg) { static_cast<LegServos*>(arg)->Loop(); }

void LegServos::Loop() {
    const float step = (float)SERVO_SLEW_DEG_PER_S * SERVO_TICK_MS / 1000.0f;
    TickType_t last = xTaskGetTickCount();
    while (true) {
        uint8_t cmd;
        while (xQueueReceive(queue_, &cmd, 0) == pdTRUE) {
            if (cmd == kCmdAttach) {
                if (initialized_) continue;
                ledc_timer_config_t timer = {};
                timer.speed_mode = LEDC_LOW_SPEED_MODE;
                timer.duty_resolution = (ledc_timer_bit_t)SERVO_PWM_BITS;
                timer.timer_num = SERVO_LEDC_TIMER;
                timer.freq_hz = SERVO_PWM_HZ;
                timer.clk_cfg = LEDC_AUTO_CLK;
                ESP_ERROR_CHECK(ledc_timer_config(&timer));
                for (int i = 0; i < LEG_COUNT; ++i) {
                    ledc_channel_config_t ch = {};
                    ch.gpio_num = kLegConfig[i].gpio;
                    ch.speed_mode = LEDC_LOW_SPEED_MODE;
                    ch.channel = legs_[i].channel;
                    ch.timer_sel = SERVO_LEDC_TIMER;
                    ch.duty = 0;
                    ch.hpoint = 0;
                    ESP_ERROR_CHECK(ledc_channel_config(&ch));
                    // Gắn servo trực tiếp tại góc đứng đã lưu trong NVS
                    legs_[i].current = legs_[i].target = stand_angle_[i];
                    WriteAngle(i, stand_angle_[i]);
                    legs_[i].settled_ms = NowMs();
                    ESP_LOGI(TAG, "attached leg %d on GPIO%d", i, (int)kLegConfig[i].gpio);
                    vTaskDelay(pdMS_TO_TICKS(SERVO_INIT_GAP_MS));
                }
                initialized_ = true;
            } else if (initialized_) {
                LoadMotion((LegMotion)cmd);
            }
        }

        int64_t now = NowMs();
        if (initialized_) {
            // 1) start the next keyframe
            if (frames_ != nullptr && !frame_active_ && frame_index_ < frame_count_) {
                const Keyframe& kf = frames_[frame_index_];
                for (int i = 0; i < LEG_COUNT; ++i) {
                    if (kf.angle[i] != 0) legs_[i].target = kf.angle[i];
                }
                frame_active_ = true;
                frame_settled_ms_ = 0;
            }

            // 2) start legs that need to move, max SERVO_MAX_MOVING, staggered
            int moving = 0;
            for (auto& l : legs_) moving += l.moving ? 1 : 0;
            for (int i = 0; i < LEG_COUNT; ++i) {
                Leg& l = legs_[i];
                if (l.moving || std::fabs(l.target - l.current) < 0.5f) continue;
                if (moving >= SERVO_MAX_MOVING || now - last_start_ms_ < SERVO_STAGGER_MS) break;
                l.moving = true;
                last_start_ms_ = now;
                moving++;
                if (!l.attached) WriteAngle(i, l.current);  // re-attach at known position
            }

            // 3) ramp moving legs
            bool all_settled = true;
            for (int i = 0; i < LEG_COUNT; ++i) {
                Leg& l = legs_[i];
                if (l.moving) {
                    float d = l.target - l.current;
                    if (std::fabs(d) <= step) {
                        l.current = l.target;
                        l.moving = false;
                        l.settled_ms = now;
                    } else {
                        l.current += d > 0 ? step : -step;
                    }
                    WriteAngle(i, l.current);
                }
                if (l.moving || std::fabs(l.target - l.current) >= 0.5f) all_settled = false;
            }

            // 4) keyframe hold / advance
            if (frame_active_ && all_settled) {
                if (frame_settled_ms_ == 0) frame_settled_ms_ = now;
                uint16_t hold = quick_ ? 0 : frames_[frame_index_].hold_ms;
                if (now - frame_settled_ms_ >= hold) {
                    frame_active_ = false;
                    if (++frame_index_ >= frame_count_) {
                        frames_ = nullptr;
                        busy_ = false;
                    }
                }
            }

            // 5) cut PWM on legs holding a pose for too long
            for (int i = 0; i < LEG_COUNT; ++i) {
                Leg& l = legs_[i];
                if (l.attached && !l.moving && std::fabs(l.target - l.current) < 0.5f &&
                    now - l.settled_ms >= SERVO_DETACH_AFTER_MS) {
                    Detach(i);
                }
            }
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SERVO_TICK_MS));
    }
}
