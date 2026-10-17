#include "dog_behavior.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include <cstring>

#include "application.h"
#include "dog_config.h"

#define TAG "DogBehavior"

// ============================================================================
// THE mapping table: server emotion -> eyes.
// Unknown strings fall back to kNeutral (never crash).
// ============================================================================
namespace {
struct EmotionRule {
    const char* emotion;
    EyePreset eyes;
};

const EmotionRule kEmotionTable[] = {
    // server emotions (xiaozhi LLM emoji set)
    {"happy", EyePreset::kHappy},
    {"laughing", EyePreset::kLaugh},
    {"funny", EyePreset::kLaugh},
    {"loving", EyePreset::kLove},
    {"kissy", EyePreset::kKiss},
    {"delicious", EyePreset::kDelicious},
    {"sad", EyePreset::kSad},
    {"crying", EyePreset::kCry},
    {"embarrassed", EyePreset::kEmbarrassed},
    {"angry", EyePreset::kAngry},
    {"surprised", EyePreset::kSurprised},
    {"shocked", EyePreset::kShocked},
    {"confused", EyePreset::kConfused},
    {"thinking", EyePreset::kThinking},
    {"sleepy", EyePreset::kSleepy},
    {"relaxed", EyePreset::kRelaxed},
    {"winking", EyePreset::kWink},
    {"cool", EyePreset::kCool},
    {"confident", EyePreset::kCool},
    {"silly", EyePreset::kSilly},
    // device-side emotions sent by the firmware core
    {"robot_2", EyePreset::kBoot},
    {"warning", EyePreset::kError},
    {"cloud_off", EyePreset::kError},
    {"cancel", EyePreset::kError},
    {"gear", EyePreset::kError},          // Wi-Fi provisioning
    {"link", EyePreset::kNeutral},        // activation code shown
    {"download", EyePreset::kThinking},   // OTA
    {"cloud_download", EyePreset::kThinking},
};

// ============================================================================
// Device state -> eyes
// ============================================================================
struct StateRule {
    DeviceState state;
    EyePreset eyes;
};

const StateRule kStateTable[] = {
    {kDeviceStateStarting, EyePreset::kBoot},
    {kDeviceStateIdle, EyePreset::kIdle},
    {kDeviceStateConnecting, EyePreset::kThinking},
    {kDeviceStateListening, EyePreset::kListening},
    {kDeviceStateSpeaking, EyePreset::kSpeaking},
    {kDeviceStateWifiConfiguring, EyePreset::kError},
    {kDeviceStateActivating, EyePreset::kNeutral},
    {kDeviceStateUpgrading, EyePreset::kThinking},
    {kDeviceStateFatalError, EyePreset::kError},
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

    if (legs_ != nullptr) {
        if (!servos_started_ && (state == kDeviceStateIdle || state == kDeviceStateListening || state == kDeviceStateSpeaking)) {
            servos_started_ = true;
            legs_->AttachSequential();
            legs_->Play(LegMotion::kStand);
        } else if (servos_started_) {
            if (sat_down_ && (state == kDeviceStateListening || state == kDeviceStateSpeaking)) {
                sat_down_ = false;
                legs_->Play(LegMotion::kStand);
            }
        }
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
        emotion_active_ = true;
        emotion_since_ms_ = now;
        if (legs_ != nullptr && legs_->attached()) {
            if (strcmp(emotion, "happy") == 0 || strcmp(emotion, "laughing") == 0 ||
                strcmp(emotion, "funny") == 0 || strcmp(emotion, "silly") == 0) {
                legs_->Play(LegMotion::kWiggle);
            } else if (strcmp(emotion, "winking") == 0 || strcmp(emotion, "cool") == 0 ||
                       strcmp(emotion, "confident") == 0) {
                legs_->Play(LegMotion::kWave);
            } else if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "crying") == 0) {
                legs_->Play(LegMotion::kCrouch);
            }
        }
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

        if (state_ == kDeviceStateIdle) {
            if (legs_ != nullptr && legs_->attached() && !sat_down_ && now - state_since_ms_ > DOG_IDLE_SIT_AFTER_MS) {
                sat_down_ = true;
                legs_->Play(LegMotion::kSit);
            }
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
