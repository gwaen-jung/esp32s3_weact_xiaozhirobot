#include "tof_sensor.h"

#include <esp_log.h>
#include <esp_timer.h>

#include "dog_config.h"

#define TAG "TofSensor"

namespace {
constexpr uint8_t kAddr = 0x29;
constexpr int kI2cTimeoutMs = 50;

// VL53L0X registers (names from the ST API / Pololu library)
constexpr uint8_t SYSRANGE_START = 0x00;
constexpr uint8_t SYSTEM_SEQUENCE_CONFIG = 0x01;
constexpr uint8_t SYSTEM_INTERMEASUREMENT_PERIOD = 0x04;
constexpr uint8_t SYSTEM_INTERRUPT_CONFIG_GPIO = 0x0A;
constexpr uint8_t SYSTEM_INTERRUPT_CLEAR = 0x0B;
constexpr uint8_t RESULT_INTERRUPT_STATUS = 0x13;
constexpr uint8_t RESULT_RANGE_STATUS = 0x14;
constexpr uint8_t FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT = 0x44;
constexpr uint8_t MSRC_CONFIG_CONTROL = 0x60;
constexpr uint8_t GPIO_HV_MUX_ACTIVE_HIGH = 0x84;
constexpr uint8_t VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV = 0x89;
constexpr uint8_t GLOBAL_CONFIG_SPAD_ENABLES_REF_0 = 0xB0;
constexpr uint8_t GLOBAL_CONFIG_REF_EN_START_SELECT = 0xB6;
constexpr uint8_t DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD = 0x4E;
constexpr uint8_t DYNAMIC_SPAD_REF_EN_START_OFFSET = 0x4F;
constexpr uint8_t IDENTIFICATION_MODEL_ID = 0xC0;
constexpr uint8_t OSC_CALIBRATE_VAL = 0xF8;

struct RegVal {
    uint8_t reg, val;
};

// ST "DefaultTuningSettings" (vl53l0x_tuning.h), as used by Pololu's init().
const RegVal kTuning[] = {
    {0xFF, 0x01}, {0x00, 0x00}, {0xFF, 0x00}, {0x09, 0x00}, {0x10, 0x00}, {0x11, 0x00},
    {0x24, 0x01}, {0x25, 0xFF}, {0x75, 0x00}, {0xFF, 0x01}, {0x4E, 0x2C}, {0x48, 0x00},
    {0x30, 0x20}, {0xFF, 0x00}, {0x30, 0x09}, {0x54, 0x00}, {0x31, 0x04}, {0x32, 0x03},
    {0x40, 0x83}, {0x46, 0x25}, {0x60, 0x00}, {0x27, 0x00}, {0x50, 0x06}, {0x51, 0x00},
    {0x52, 0x96}, {0x56, 0x08}, {0x57, 0x30}, {0x61, 0x00}, {0x62, 0x00}, {0x64, 0x00},
    {0x65, 0x00}, {0x66, 0xA0}, {0xFF, 0x01}, {0x22, 0x32}, {0x47, 0x14}, {0x49, 0xFF},
    {0x4A, 0x00}, {0xFF, 0x00}, {0x7A, 0x0A}, {0x7B, 0x00}, {0x78, 0x21}, {0xFF, 0x01},
    {0x23, 0x34}, {0x42, 0x00}, {0x44, 0xFF}, {0x45, 0x26}, {0x46, 0x05}, {0x40, 0x40},
    {0x0E, 0x06}, {0x20, 0x1A}, {0x43, 0x40}, {0xFF, 0x00}, {0x34, 0x03}, {0x35, 0x44},
    {0xFF, 0x01}, {0x31, 0x04}, {0x4B, 0x09}, {0x4C, 0x05}, {0x4D, 0x04}, {0xFF, 0x00},
    {0x44, 0x00}, {0x45, 0x20}, {0x47, 0x08}, {0x48, 0x28}, {0x67, 0x00}, {0x70, 0x04},
    {0x71, 0x01}, {0x72, 0xFE}, {0x76, 0x00}, {0x77, 0x00}, {0xFF, 0x01}, {0x0D, 0x01},
    {0xFF, 0x00}, {0x80, 0x01}, {0x01, 0xF8}, {0xFF, 0x01}, {0x8E, 0x01}, {0x00, 0x01},
    {0xFF, 0x00}, {0x80, 0x00},
};
}  // namespace

TofSensor::TofSensor(gpio_num_t sda, gpio_num_t scl, gpio_num_t xshut)
    : sda_(sda), scl_(scl), xshut_(xshut) {}

void TofSensor::Start(NearCallback on_near) {
    on_near_ = std::move(on_near);
    xTaskCreate(&TofSensor::TaskEntry, "tof", 4096, this, TOF_TASK_PRIORITY, &task_);
}

void TofSensor::TaskEntry(void* arg) { static_cast<TofSensor*>(arg)->Loop(); }

// ---------------------------------------------------------------------------
// I2C helpers (never abort, set ok_ = false on error)
// ---------------------------------------------------------------------------
bool TofSensor::Write8(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    bool r = i2c_master_transmit(dev_, buf, 2, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    return r;
}

bool TofSensor::Write16(uint8_t reg, uint16_t value) {
    uint8_t buf[3] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
    bool r = i2c_master_transmit(dev_, buf, 3, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    return r;
}

bool TofSensor::Write32(uint8_t reg, uint32_t value) {
    uint8_t buf[5] = {reg, (uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8),
                      (uint8_t)value};
    bool r = i2c_master_transmit(dev_, buf, 5, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    return r;
}

bool TofSensor::WriteMulti(uint8_t reg, const uint8_t* src, size_t count) {
    uint8_t buf[8];
    if (count > sizeof(buf) - 1) return false;
    buf[0] = reg;
    for (size_t i = 0; i < count; ++i) buf[i + 1] = src[i];
    bool r = i2c_master_transmit(dev_, buf, count + 1, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    return r;
}

bool TofSensor::Read8(uint8_t reg, uint8_t* value) {
    bool r = i2c_master_transmit_receive(dev_, &reg, 1, value, 1, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    return r;
}

bool TofSensor::Read16(uint8_t reg, uint16_t* value) {
    uint8_t b[2] = {0, 0};
    bool r = i2c_master_transmit_receive(dev_, &reg, 1, b, 2, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    *value = (uint16_t)((b[0] << 8) | b[1]);
    return r;
}

bool TofSensor::ReadMulti(uint8_t reg, uint8_t* dst, size_t count) {
    bool r = i2c_master_transmit_receive(dev_, &reg, 1, dst, count, kI2cTimeoutMs) == ESP_OK;
    ok_ = ok_ && r;
    return r;
}

bool TofSensor::WaitFor(uint8_t reg, uint8_t mask, bool want_nonzero, int timeout_ms) {
    int64_t end = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    uint8_t v = 0;
    while (esp_timer_get_time() < end) {
        if (!Read8(reg, &v)) return false;
        if (((v & mask) != 0) == want_nonzero) return true;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    ESP_LOGW(TAG, "timeout waiting for reg 0x%02X", reg);
    return false;
}

// ---------------------------------------------------------------------------
// VL53L0X init (Pololu VL53L0X::init sequence)
// ---------------------------------------------------------------------------
bool TofSensor::InitBus() {
    if (xshut_ != GPIO_NUM_NC) {
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << xshut_;
        io.mode = GPIO_MODE_OUTPUT;
        gpio_config(&io);
        gpio_set_level(xshut_, 0);  // hard reset
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(xshut_, 1);
        vTaskDelay(pdMS_TO_TICKS(10));  // boot time 1.2 ms max
    }

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = TOF_I2C_PORT;
    bus_cfg.sda_io_num = sda_;
    bus_cfg.scl_io_num = scl_;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus_cfg, &bus_) != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
        return false;
    }
    if (i2c_master_probe(bus_, kAddr, 100) != ESP_OK) {
        ESP_LOGW(TAG, "No VL53L0X at 0x29 (SDA %d, SCL %d)", sda_, scl_);
        return false;
    }
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = kAddr;
    dev_cfg.scl_speed_hz = TOF_I2C_HZ;
    return i2c_master_bus_add_device(bus_, &dev_cfg, &dev_) == ESP_OK;
}

bool TofSensor::GetSpadInfo(uint8_t* count, bool* type_is_aperture) {
    uint8_t tmp = 0;
    Write8(0x80, 0x01);
    Write8(0xFF, 0x01);
    Write8(0x00, 0x00);
    Write8(0xFF, 0x06);
    Read8(0x83, &tmp);
    Write8(0x83, tmp | 0x04);
    Write8(0xFF, 0x07);
    Write8(0x81, 0x01);
    Write8(0x80, 0x01);
    Write8(0x94, 0x6B);
    Write8(0x83, 0x00);
    if (!WaitFor(0x83, 0xFF, true, 100)) return false;
    Write8(0x83, 0x01);
    Read8(0x92, &tmp);
    *count = tmp & 0x7F;
    *type_is_aperture = (tmp >> 7) & 0x01;
    Write8(0x81, 0x00);
    Write8(0xFF, 0x06);
    Read8(0x83, &tmp);
    Write8(0x83, tmp & ~0x04);
    Write8(0xFF, 0x01);
    Write8(0x00, 0x01);
    Write8(0xFF, 0x00);
    Write8(0x80, 0x00);
    return ok_;
}

bool TofSensor::PerformSingleRefCalibration(uint8_t vhv_init_byte) {
    Write8(SYSRANGE_START, 0x01 | vhv_init_byte);
    if (!WaitFor(RESULT_INTERRUPT_STATUS, 0x07, true, 200)) return false;
    Write8(SYSTEM_INTERRUPT_CLEAR, 0x01);
    Write8(SYSRANGE_START, 0x00);
    return ok_;
}

bool TofSensor::Init() {
    ok_ = true;
    uint8_t v = 0;
    if (!Read8(IDENTIFICATION_MODEL_ID, &v) || v != 0xEE) {
        ESP_LOGW(TAG, "Model id 0x%02X, expected 0xEE (VL53L1X is not supported)", v);
        return false;
    }

    // 2.8 V I/O mode
    Read8(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, &v);
    Write8(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, v | 0x01);
    // "Set I2C standard mode"
    Write8(0x88, 0x00);
    Write8(0x80, 0x01);
    Write8(0xFF, 0x01);
    Write8(0x00, 0x00);
    Read8(0x91, &stop_variable_);
    Write8(0x00, 0x01);
    Write8(0xFF, 0x00);
    Write8(0x80, 0x00);

    // Disable SIGNAL_RATE_MSRC and SIGNAL_RATE_PRE_RANGE limit checks
    Read8(MSRC_CONFIG_CONTROL, &v);
    Write8(MSRC_CONFIG_CONTROL, v | 0x12);
    // Signal rate limit 0.25 MCPS (Q9.7)
    Write16(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, (uint16_t)(0.25f * (1 << 7)));
    Write8(SYSTEM_SEQUENCE_CONFIG, 0xFF);
    if (!ok_) return false;

    // Reference SPADs
    uint8_t spad_count = 0;
    bool spad_aperture = false;
    if (!GetSpadInfo(&spad_count, &spad_aperture)) return false;
    uint8_t ref_spad_map[6];
    if (!ReadMulti(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6)) return false;
    Write8(0xFF, 0x01);
    Write8(DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    Write8(DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
    Write8(0xFF, 0x00);
    Write8(GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);
    uint8_t first_spad = spad_aperture ? 12 : 0;
    uint8_t enabled = 0;
    for (uint8_t i = 0; i < 48; i++) {
        if (i < first_spad || enabled == spad_count) {
            ref_spad_map[i / 8] &= ~(1 << (i % 8));
        } else if ((ref_spad_map[i / 8] >> (i % 8)) & 0x1) {
            enabled++;
        }
    }
    WriteMulti(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

    for (const auto& rv : kTuning) Write8(rv.reg, rv.val);

    // Interrupt on new sample ready, active low
    Write8(SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    Read8(GPIO_HV_MUX_ACTIVE_HIGH, &v);
    Write8(GPIO_HV_MUX_ACTIVE_HIGH, v & ~0x10);
    Write8(SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Sequence: DSS, pre-range, final-range (as Pololu)
    Write8(SYSTEM_SEQUENCE_CONFIG, 0xE8);
    if (!ok_) return false;

    // VHV and phase reference calibration
    Write8(SYSTEM_SEQUENCE_CONFIG, 0x01);
    if (!PerformSingleRefCalibration(0x40)) return false;
    Write8(SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (!PerformSingleRefCalibration(0x00)) return false;
    Write8(SYSTEM_SEQUENCE_CONFIG, 0xE8);
    return ok_;
}

bool TofSensor::StartContinuous(uint32_t period_ms) {
    Write8(0x80, 0x01);
    Write8(0xFF, 0x01);
    Write8(0x00, 0x00);
    Write8(0x91, stop_variable_);
    Write8(0x00, 0x01);
    Write8(0xFF, 0x00);
    Write8(0x80, 0x00);
    uint16_t osc = 0;
    Read16(OSC_CALIBRATE_VAL, &osc);
    if (osc != 0) period_ms *= osc;
    Write32(SYSTEM_INTERMEASUREMENT_PERIOD, period_ms);
    Write8(SYSRANGE_START, 0x04);  // timed continuous mode
    return ok_;
}

bool TofSensor::ReadIfReady(uint16_t* mm) {
    uint8_t st = 0;
    if (!Read8(RESULT_INTERRUPT_STATUS, &st) || (st & 0x07) == 0) return false;
    // RESULT_RANGE_STATUS + 10 holds the range in mm (big endian)
    uint8_t rs = 0;
    Read8(RESULT_RANGE_STATUS, &rs);
    last_status_ = (rs >> 3) & 0x0F;  // 11 = valid, others = sigma/signal/phase fail
    bool r = Read16(RESULT_RANGE_STATUS + 10, mm);
    Write8(SYSTEM_INTERRUPT_CLEAR, 0x01);
    return r;
}

// ---------------------------------------------------------------------------
// Polling + "hand close" detector with hysteresis
// ---------------------------------------------------------------------------
bool TofSensor::Restart() {
    if (xshut_ != GPIO_NUM_NC) {  // hard reset: clears a sensor stuck in standby
        gpio_set_level(xshut_, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(xshut_, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return Init() && StartContinuous(TOF_PERIOD_MS);
}

void TofSensor::Loop() {
    bool ready = false;
    for (int attempt = 1; attempt <= 3 && !ready; ++attempt) {
        if (bus_ == nullptr && !InitBus()) break;
        if (dev_ == nullptr) break;
        ready = Init() && StartContinuous(TOF_PERIOD_MS);
        if (!ready) {
            ESP_LOGW(TAG, "VL53L0X init failed (attempt %d)", attempt);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    if (!ready) {
        ESP_LOGE(TAG, "ToF disabled");
        task_ = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "VL53L0X ready, near < %d mm triggers listening", TOF_NEAR_MM);

    int near_count = 0, far_count = 0;
    bool armed = true;
    // Empty-scene baseline (slow average). This module reads ~79 mm with nothing
    // in front (offset / cover crosstalk), so a fixed 50 mm threshold is too
    // strict: trigger on a clear drop below the baseline, clamped to 50..70 mm.
    float baseline = -1.0f;
    int64_t now = esp_timer_get_time() / 1000;
    int64_t last_log_ms = 0, last_trigger_ms = 0, last_sample_ms = now;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(TOF_PERIOD_MS / 2));
        now = esp_timer_get_time() / 1000;
        uint16_t mm = 0;
        ok_ = true;
        if (!ReadIfReady(&mm)) {
            // No new sample for too long (sensor reset by a glitch and sitting in
            // standby, or I2C errors): hard reset and re-initialise.
            if (now - last_sample_ms > TOF_STALL_MS) {
                ESP_LOGW(TAG, "no sample for %d ms (%s), restarting sensor",
                         (int)(now - last_sample_ms), ok_ ? "stalled" : "I2C error");
                bool restarted = Restart();
                ESP_LOGI(TAG, "restart %s", restarted ? "ok" : "failed");
                last_sample_ms = esp_timer_get_time() / 1000;
                if (!restarted) vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }
        last_sample_ms = now;
        // 8190/8191 = no target. Touching the cover glass reads 0..20 mm,
        // which is "near" too.
        int d = mm >= 8190 ? 9999 : mm;
        last_mm_ = d;

        int threshold = TOF_NEAR_MAX_MM;
        if (baseline > 0) {
            threshold = (int)baseline - TOF_DROP_MM;
            if (threshold < TOF_NEAR_MM) threshold = TOF_NEAR_MM;
            if (threshold > TOF_NEAR_MAX_MM) threshold = TOF_NEAR_MAX_MM;
        }
        bool near = d < threshold;

        if (now - last_log_ms > (near ? 200 : 5000)) {
            last_log_ms = now;
            ESP_LOGI(TAG, "distance %d mm, baseline %d, threshold %d (status %u)", d,
                     (int)baseline, threshold, last_status_);
        }

        if (near) {
            far_count = 0;
            if (armed && ++near_count >= TOF_NEAR_SAMPLES &&
                now - last_trigger_ms > TOF_COOLDOWN_MS) {
                armed = false;
                near_count = 0;
                last_trigger_ms = now;
                ESP_LOGI(TAG, "hand detected at %d mm", d);
                if (on_near_) on_near_(d);
            }
        } else {
            near_count = 0;
            if (armed) {
                float v = d > 2000 ? 2000.0f : (float)d;  // "no target" counts as far
                baseline = baseline < 0 ? v : baseline + (v - baseline) / 32.0f;
            } else if (d > threshold + TOF_REARM_MARGIN_MM && ++far_count >= TOF_REARM_SAMPLES) {
                armed = true;
            }
        }
    }
}
