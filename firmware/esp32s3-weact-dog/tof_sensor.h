// TofSensor: minimal VL53L0X driver (I2C master, continuous ranging) plus a
// "hand close" detector. Init/tuning sequence follows the Pololu VL53L0X
// Arduino library (itself derived from the ST API). I2C errors never abort:
// if the sensor is missing the task just logs and exits.
#pragma once

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <functional>

class TofSensor {
public:
    using NearCallback = std::function<void(int distance_mm)>;

    TofSensor(gpio_num_t sda, gpio_num_t scl, gpio_num_t xshut);
    // Creates the polling task. `on_near` runs in that task.
    void Start(NearCallback on_near);
    int last_distance_mm() const { return last_mm_; }

private:
    static void TaskEntry(void* arg);
    void Loop();

    bool Init();
    bool InitBus();
    bool GetSpadInfo(uint8_t* count, bool* type_is_aperture);
    bool PerformSingleRefCalibration(uint8_t vhv_init_byte);
    bool StartContinuous(uint32_t period_ms);
    bool Restart();  // XSHUT reset + Init + StartContinuous
    // Returns true when a new sample was read into *mm.
    bool ReadIfReady(uint16_t* mm);

    bool Write8(uint8_t reg, uint8_t value);
    bool Write16(uint8_t reg, uint16_t value);
    bool Write32(uint8_t reg, uint32_t value);
    bool WriteMulti(uint8_t reg, const uint8_t* src, size_t count);
    bool Read8(uint8_t reg, uint8_t* value);
    bool Read16(uint8_t reg, uint16_t* value);
    bool ReadMulti(uint8_t reg, uint8_t* dst, size_t count);
    bool WaitFor(uint8_t reg, uint8_t mask, bool want_nonzero, int timeout_ms);

    gpio_num_t sda_, scl_, xshut_;
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    uint8_t stop_variable_ = 0;
    bool ok_ = true;  // sticky I2C error flag during init
    volatile int last_mm_ = -1;
    uint8_t last_status_ = 0;
    NearCallback on_near_;
    TaskHandle_t task_ = nullptr;
};
