#include "dog_behavior.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include <cstring>

#include "application.h"
#include "dog_config.h"

#define TAG "DogBehavior"

// ============================================================================
// THE mapping table: server emotion -> eyes + legs.
// Unknown strings fall back to kNeutral (never crash).
// ============================================================================
namespace {
struct EmotionRule {
    const char* emotion;
    EyePreset eyes;
    LegMotion legs;
};

const EmotionRule kEmotionTable[] = {
    // server emotions (xiaozhi LLM emoji set)
    {"happy", EyePreset::kHappy, LegMotion::kWiggle},
    {"laughing", EyePreset::kLaugh, LegMotion::kWave},
    {"funny", EyePreset::kLaugh, LegMotion::kWave},
    {"loving", EyePreset::kLove, LegMotion::kWiggle},
    {"kissy", EyePreset::kKiss, LegMotion::kWave},
    {"delicious", EyePreset::kDelicious, LegMotion::kWiggle},
    {"sad", EyePreset::kSad, LegMotion::kCrouch},
    {"crying", EyePreset::kCry, LegMotion::kCrouch},
    {"embarrassed", EyePreset::kEmbarrassed, LegMotion::kCrouch},
    {"angry", EyePreset::kAngry, LegMotion::kQuickStand},
    {"surprised", EyePreset::kSurprised, LegMotion::kQuickStand},
    {"shocked", EyePreset::kShocked, LegMotion::kQuickStand},
    {"confused", EyePreset::kConfused, LegMotion::kSoftWiggle},
    {"thinking", EyePreset::kThinking, LegMotion::kNone},
    {"sleepy", EyePreset::kSleepy, LegMotion::kSit},
    {"relaxed", EyePreset::kRelaxed, LegMotion::kBreathe},
    {"winking", EyePreset::kWink, LegMotion::kSoftWiggle},
    {"cool", EyePreset::kCool, LegMotion::kStand},
    {"confident", EyePreset::kCool, LegMotion::kStand},
    {"silly", EyePreset::kSilly, LegMotion::kWiggle},
    // device-side emotions sent by the firmware core
    {"robot_2", EyePreset::kBoot, LegMotion::kNone},
    {"warning", EyePreset::kError, LegMotion::kNone},
    {"cloud_off", EyePreset::kError, LegMotion::kNone},
    {"cancel", EyePreset::kError, LegMotion::kNone},
    {"gear", EyePreset::kError, LegMotion::kNone},          // Wi-Fi provisioning
    {"link", EyePreset::kNeutral, LegMotion::kNone},        // activation code shown
    {"download", EyePreset::kThinking, LegMotion::kNone},   // OTA
    {"cloud_download", EyePreset::kThinking, LegMotion::kNone},
};

// ============================================================================
// Device state -> eyes
// ============================================================================
struct StateRule {
    DeviceState state;
    EyePreset eyes;
    LegMotion legs;
};

const StateRule kStateTable[] = {
    {kDeviceStateStarting, EyePreset::kBoot, LegMotion::kNone},
    {kDeviceStateIdle, EyePreset::kIdle, LegMotion::kStand},
    {kDeviceStateConnecting, EyePreset::kThinking, LegMotion::kNone},
    {kDeviceStateListening, EyePreset::kListening, LegMotion::kStand},
    {kDeviceStateSpeaking, EyePreset::kSpeaking, LegMotion::kSoftWiggle},
    {kDeviceStateWifiConfiguring, EyePreset::kError, LegMotion::kNone},
    {kDeviceStateActivating, EyePreset::kNeutral, LegMotion::kNone},
    {kDeviceStateUpgrading, EyePreset::kThinking, LegMotion::kNone},
    {kDeviceStateFatalError, EyePreset::kError, LegMotion::kRelax},
};
// "Thinking" is not an upstream state: Listening + user text received.
constexpr EyePreset kThinkingEyes = EyePreset::kThinking;

// Short random expressions while idle so the robot looks alive.
enum class Fidget : uint8_t { kLookLeft, kLookRight, kLookUp, kBlink, kWink, kHappy, kCurious };

int64_t now_ms() { return esp_timer_get_time() / 1000; }
int64_t rand_between(int64_t lo, int64_t hi) { return lo + (int64_t)(esp_random() % (uint32_t)(hi - lo + 1)); }
}  // namespace

DogBehavior::DogBehavior(EyesDisplay* eyes, LegServos* legs) : eyes_(eyes), legs_(legs) {
    events_ = xQueueCreate(8, sizeof(Event));
}

void DogBehavior::Start() {
    eyes_->SetEmotionHook([this](const char* e) { OnEmotion(e); });
    eyes_->SetChatHook([this](const char* role, const char* content) { OnChat(role, content); });
    eyes_->ApplyPreset(EyePreset::kBoot);
    xTaskCreate(&DogBehavior::TaskEntry, "dog", 4096, this, 3, &task_);
}

void DogBehavior::OnEmotion(const char* emotion) {
    Event ev = {};
    ev.type = EventType::kEmotion;
    strncpy(ev.text, emotion ? emotion : "", sizeof(ev.text) - 1);
    xQueueSend(events_, &ev, 0);
}

void DogBehavior::OnChat(const char* role, const char* content) {
    if (role == nullptr || strcmp(role, "user") != 0 || content == nullptr || content[0] == 0) return;
    Event ev = {};
    ev.type = EventType::kUserText;
    xQueueSend(events_, &ev, 0);
}

void DogBehavior::TaskEntry(void* arg) { static_cast<DogBehavior*>(arg)->Loop(); }

void DogBehavior::ScheduleFidget(int64_t now) {
    next_fidget_ms_ = now + rand_between(DOG_IDLE_FIDGET_MIN_MS, DOG_IDLE_FIDGET_MAX_MS);
}

void DogBehavior::HandleState(DeviceState state, int64_t now) {
    ESP_LOGI(TAG, "state %d -> %d", (int)state_, (int)state);
    DeviceState prev = state_;
    state_ = state;
    state_since_ms_ = now;
    thinking_ = false;
    sleepy_ = false;
    surprise_until_ms_ = 0;
    fidget_end_ms_ = 0;
    ScheduleFidget(now);

    eyes_->SetSpeaking(state == kDeviceStateSpeaking);

    // Servos are powered only once Wi-Fi and audio are up (first Idle).
    if (state == kDeviceStateIdle && !servos_started_) {
        servos_started_ = true;
        legs_->AttachSequential();
    }

    // After a conversation, keep the last emotion on screen for a moment
    // instead of snapping straight back to the idle look.
    if (state == kDeviceStateIdle && prev == kDeviceStateSpeaking && emotion_active_) {
        emotion_since_ms_ = now;
        return;
    }
    if (state != kDeviceStateSpeaking) emotion_active_ = false;

    for (const auto& r : kStateTable) {
        if (r.state != state) continue;
        eyes_->ApplyPreset(r.eyes);
        if (r.legs == LegMotion::kStand) sat_down_ = false;
        legs_->Play(r.legs);
        if (r.legs == LegMotion::kSoftWiggle) last_wiggle_ms_ = now;
        return;
    }
}

void DogBehavior::HandleEmotion(const char* emotion, int64_t now) {
    if (strcmp(emotion, "neutral") == 0 || emotion[0] == 0) {
        // The core sends "neutral" on every idle/listening/connecting change;
        // the state table owns those looks. Only reset the mood while speaking.
        if (state_ == kDeviceStateSpeaking) {
            eyes_->ApplyPreset(EyePreset::kNeutral);
            emotion_active_ = false;
        }
        return;
    }
    for (const auto& r : kEmotionTable) {
        if (strcmp(r.emotion, emotion) != 0) continue;
        eyes_->ApplyPreset(r.eyes);
        if (r.eyes == EyePreset::kSurprised) surprise_until_ms_ = now + DOG_SURPRISE_MS;
        legs_->Play(r.legs);
        if (r.legs != LegMotion::kNone) last_wiggle_ms_ = now;  // don't overlap a soft wiggle
        sat_down_ = r.legs == LegMotion::kSit;
        emotion_active_ = true;
        emotion_since_ms_ = now;
        return;
    }
    ESP_LOGW(TAG, "unknown emotion '%s' -> neutral", emotion);
    eyes_->ApplyPreset(EyePreset::kNeutral);
}

void DogBehavior::IdleFidget(int64_t now) {
    auto f = (Fidget)(esp_random() % 7);
    switch (f) {
        case Fidget::kLookLeft: eyes_->LookAt(roboeyes::W); break;
        case Fidget::kLookRight: eyes_->LookAt(roboeyes::E); break;
        case Fidget::kLookUp: eyes_->LookAt(roboeyes::N); break;
        case Fidget::kBlink: eyes_->Blink(); break;
        case Fidget::kWink: eyes_->Wink(); break;
        case Fidget::kHappy: eyes_->ApplyPreset(EyePreset::kHappy); break;
        case Fidget::kCurious: eyes_->ApplyPreset(EyePreset::kSurprised); break;
    }
    fidget_end_ms_ = now + 1800;
    ScheduleFidget(now);
}

void DogBehavior::Loop() {
    // Let Board/Application construction finish before touching the singleton.
    vTaskDelay(pdMS_TO_TICKS(500));
    auto& app = Application::GetInstance();
    TickType_t last = xTaskGetTickCount();
    while (true) {
        int64_t now = now_ms();
        DeviceState s = app.GetDeviceState();
        if (s != state_) HandleState(s, now);

        Event ev;
        while (xQueueReceive(events_, &ev, 0) == pdTRUE) {
            if (ev.type == EventType::kEmotion) {
                HandleEmotion(ev.text, now);
            } else if (ev.type == EventType::kUserText && state_ == kDeviceStateListening &&
                       !thinking_) {
                thinking_ = true;
                eyes_->ApplyPreset(kThinkingEyes);
            }
        }

        if (surprise_until_ms_ != 0 && now > surprise_until_ms_) {
            surprise_until_ms_ = 0;
            eyes_->SetCuriosity(false);
        }

        if (state_ == kDeviceStateSpeaking && !legs_->busy() &&
            now - last_wiggle_ms_ > DOG_SPEAK_WIGGLE_EVERY_MS) {
            last_wiggle_ms_ = now;
            legs_->Play(LegMotion::kSoftWiggle);
        }
        if (state_ == kDeviceStateIdle && !sat_down_ && legs_->attached() &&
            now - state_since_ms_ > DOG_IDLE_SIT_AFTER_MS) {
            sat_down_ = true;
            legs_->Play(LegMotion::kSit);  // PWM is cut automatically after the hold
        }

        if (state_ == kDeviceStateIdle) {
            if (emotion_active_ && now - emotion_since_ms_ > DOG_EMOTION_HOLD_MS / 2) {
                emotion_active_ = false;  // post-chat emotion has been shown long enough
                eyes_->ApplyPreset(EyePreset::kIdle);
                ScheduleFidget(now);
            } else if (!emotion_active_) {
                if (fidget_end_ms_ != 0 && now > fidget_end_ms_) {
                    fidget_end_ms_ = 0;
                    eyes_->ApplyPreset(sleepy_ ? EyePreset::kSleepy : EyePreset::kIdle);
                }
                if (!sleepy_ && now - state_since_ms_ > DOG_IDLE_SLEEPY_AFTER_MS) {
                    sleepy_ = true;
                    eyes_->ApplyPreset(EyePreset::kSleepy);
                } else if (!sleepy_ && fidget_end_ms_ == 0 && now > next_fidget_ms_) {
                    IdleFidget(now);
                }
            }
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(DOG_POLL_MS));
    }
}
