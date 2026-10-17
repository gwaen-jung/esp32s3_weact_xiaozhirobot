// Tunable constants for the eyes, behaviour, and servo legs. No magic numbers elsewhere.
#pragma once

#include <cstdint>
#include "board_pins.h"

// ============================================================================
// Eyes
// ============================================================================
// Colours are plain RGB565 (0xRRRR RGGG GGGB BBBB). The canvas swaps bytes itself.
#define EYES_COLOR_MAIN       0x07FF  // cyan (default look)
#define EYES_COLOR_BG         0x0000  // black
#define EYES_COLOR_TEXT       0xFFFF  // subtitle text (notifications, activation code)

// Per-mood eye colours. The eye colour fades smoothly between them.
#define EYES_COLOR_HAPPY      0xFFE0  // yellow
#define EYES_COLOR_LOVE       0xFB56  // pink
#define EYES_COLOR_SAD        0x3B9F  // soft blue
#define EYES_COLOR_ANGRY      0xF800  // red
#define EYES_COLOR_SURPRISED  0xFFFF  // white
#define EYES_COLOR_CONFUSED   0xB41F  // violet
#define EYES_COLOR_SLEEPY     0x6D7B  // dim blue-grey
#define EYES_COLOR_COOL       0x07E0  // green
#define EYES_COLOR_SILLY      0xFD20  // orange
#define EYES_COLOR_LISTEN     0x07FF  // cyan
#define EYES_COLOR_THINK      0x867F  // light blue
#define EYES_COLOR_ERROR      0xFD20  // orange

// Overlay colours (hearts, tears, blush, symbols)
#define FX_COLOR_HEART        0xF8B2  // red-pink
#define FX_COLOR_TEAR         0x5D9F  // light blue
#define FX_COLOR_BLUSH        0xF9F2  // pink
#define FX_COLOR_ANGER        0xF800  // red
#define FX_COLOR_ZZZ          0xAD7F  // pale blue
#define FX_COLOR_SYMBOL       0xFFFF  // "?" "!" text
#define FX_COLOR_SPARKLE      0xFFF0  // pale yellow
#define FX_COLOR_GLASSES      0x2104  // almost black (sunglasses)
#define FX_COLOR_NOTE         0xFFE0  // music note

#define EYES_COLOR_FADE_STEP  24      // 0..256 per frame (24 ~ 10 frames to fade)

#define EYES_FPS              30      // target frame rate (25..30)
#define EYES_TASK_PRIORITY    2       // low: must never starve audio
#define EYES_TASK_STACK       4096
#define EYES_TASK_CORE        1       // tskNO_AFFINITY is also fine

#define EYES_WIDTH            90
#define EYES_HEIGHT           80
#define EYES_RADIUS           20
#define EYES_SPACE            40
// "Wide open" variant used for listening / surprised
#define EYES_WIDE_WIDTH       96
#define EYES_WIDE_HEIGHT      104
// "Squint" variant used for cool / skeptical
#define EYES_SQUINT_HEIGHT    40

#define EYES_SWEAT_SCALE      3.0f    // sweat drops were sized for 128x64

// Push buffer: internal DMA RAM, two ping-pong chunks of N lines each
#define EYES_PUSH_LINES       24

// Bottom subtitle strip (ASCII + Vietnamese 8x16 font, scaled)
#define EYES_TEXT_SCALE       2
#define EYES_TEXT_MAX_LINES   2
// How long an assistant sentence stays on screen
#define EYES_CHAT_TEXT_MS     6000

// ============================================================================
// Behaviour
// ============================================================================
#define DOG_SURPRISE_MS          1500   // how long "surprised" curiosity lasts
#define DOG_EMOTION_HOLD_MS      8000   // an emotion fades back to neutral after this (idle)
#define DOG_IDLE_SLEEPY_AFTER_MS 180000 // idle this long -> sleepy eyes + Zzz
#define DOG_IDLE_FIDGET_MIN_MS   12000  // random idle micro-expression interval
#define DOG_IDLE_FIDGET_MAX_MS   25000
#define DOG_POLL_MS              50     // state polling period

// ============================================================================
// Servo legs
// ============================================================================
enum LegId : uint8_t { LEG_FL = 0, LEG_FR = 1, LEG_RL = 2, LEG_RR = 3, LEG_COUNT = 4 };

struct LegConfig {
    gpio_num_t gpio;
    int8_t trim_deg;  // added to the logical angle
    bool invert;      // mirror around 90 deg (for the right-hand side usually)
    bool is_continuous_360; // true = 360 continuous rotation servo (stop at 90 deg)
};

static constexpr LegConfig kLegConfig[LEG_COUNT] = {
    {PIN_SERVO_FL, 0, false, false}, // 180° front-left (GPIO 7)
    {PIN_SERVO_FR, 0, true,  true},  // 360° continuous rotation front-right (GPIO 4)
    {PIN_SERVO_RL, 0, false, true},  // 360° continuous rotation rear-left (GPIO 6)
    {PIN_SERVO_RR, 0, true,  false}, // 180° rear-right (GPIO 5)
};

#define SERVO_STAND_FL_DEG       8     // Chân trước trái (180°)
#define SERVO_STAND_FR_DEG       91    // Chân trước phải (360° deadband stop)
#define SERVO_STAND_RL_DEG       97    // Chân sau trái (360° deadband stop)
#define SERVO_STAND_RR_DEG       57    // Chân sau phải (180°)

#define SERVO_NEUTRAL_DEG        90
#define SERVO_MIN_DEG            0     // cho phép về 0 độ
#define SERVO_MAX_DEG            180   // cho phép lên tối đa 180 độ
#define SERVO_PULSE_MIN_US       500   // pulse at 0 deg
#define SERVO_PULSE_MAX_US       2400  // pulse at 180 deg
#define SERVO_PWM_HZ             50
#define SERVO_PWM_BITS           14
#define SERVO_LEDC_TIMER         LEDC_TIMER_1    // timer 0 is left for backlight
#define SERVO_LEDC_FIRST_CHANNEL LEDC_CHANNEL_2  // channels 2..5

#define SERVO_SLEW_DEG_PER_S     90    // ramp speed limit
#define SERVO_MAX_MOVING         2     // never more than 2 servos moving at once
#define SERVO_STAGGER_MS         80    // delay between two servo start-ups
#define SERVO_DETACH_AFTER_MS    700   // stop PWM after 700ms holding to prevent heat/brownout
#define SERVO_TICK_MS            20    // control loop period
#define SERVO_INIT_GAP_MS        300   // sequential attach at boot
#define SERVO_TASK_PRIORITY      3
#define SERVO_TASK_STACK         4096

#define DOG_IDLE_SIT_AFTER_MS    60000 // idle this long -> sit, then PWM off
#define DOG_SPEAK_WIGGLE_EVERY_MS 3500 // soft wiggle cadence while speaking

// ============================================================================
// ToF VL53L0X: hand close -> start listening
// ============================================================================
#define TOF_I2C_PORT             I2C_NUM_0
#define TOF_I2C_HZ               100000
#define TOF_PERIOD_MS            50     // continuous ranging period
#define TOF_NEAR_MM              50     // closer than this = "hand in front" (5 cm)
#define TOF_NEAR_SAMPLES         2      // consecutive near samples before triggering
#define TOF_NEAR_MAX_MM          70     // adaptive threshold upper clamp
#define TOF_DROP_MM              12     // trigger when this much closer than the baseline
                                        // (empty scene reads ~78-83 mm on this robot)
#define TOF_REARM_MARGIN_MM      8      // must go this far above the threshold to re-arm
#define TOF_REARM_SAMPLES        3
#define TOF_STALL_MS             1000   // no sample this long -> reset the sensor
#define TOF_COOLDOWN_MS          1500   // minimum time between two triggers
#define TOF_TASK_PRIORITY        2
