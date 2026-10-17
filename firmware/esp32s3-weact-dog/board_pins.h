// Single source of truth for every GPIO used (or reserved) by the
// XIAOZHI dog robot on a WeAct ESP32-S3 CoreBoard N16R8.
// Values match the hardware verified with the Arduino test suite
// (esp32s3_weact_xiaozhirobot/platformio.ini, 2026-10-12).
#pragma once

#include <driver/gpio.h>

// ---- TFT ILI9341 2.4" (SPI) ------------------------------------------------
#define PIN_TFT_DC    GPIO_NUM_38
#define PIN_TFT_CS    GPIO_NUM_39
#define PIN_TFT_SCLK  GPIO_NUM_40
#define PIN_TFT_MOSI  GPIO_NUM_41
#define PIN_TFT_MISO  GPIO_NUM_NC  // write-only panel
#define PIN_TFT_RST   GPIO_NUM_47
#define PIN_TFT_BL    GPIO_NUM_NC  // backlight tied to 3.3V

// ---- Audio: one full-duplex I2S bus -----------------------------------------
// INMP441 (mic, L/R -> GND = left slot) and PCM5100A (DAC, MC -> GND = internal
// PLL, XSMT -> 3.3V = always unmuted) share BCLK and WS.
#define PIN_I2S_BCLK  GPIO_NUM_16
#define PIN_I2S_WS    GPIO_NUM_17
#define PIN_I2S_DIN   GPIO_NUM_14  // INMP441 SD  -> ESP32
#define PIN_I2S_DOUT  GPIO_NUM_15  // ESP32 -> PCM5100A DIN

// ---- Servo legs: 4 PWM outputs (FL, FR, RL, RR) -----------------------------
#define PIN_SERVO_FL  GPIO_NUM_7  // Front-Left (180°)
#define PIN_SERVO_FR  GPIO_NUM_4  // Front-Right (360° continuous rotation)
#define PIN_SERVO_RL  GPIO_NUM_6  // Rear-Left (360° continuous rotation)
#define PIN_SERVO_RR  GPIO_NUM_5  // Rear-Right (180°)

// ---- I2C sensors -----------------------------------------------------------
// VL53L0X ToF at 0x29 (hand < 5 cm -> start listening). MPU6050 (0x68) shares
// the bus but is not used yet.
#define PIN_I2C_SDA   GPIO_NUM_8
#define PIN_I2C_SCL   GPIO_NUM_9
#define PIN_TOF_XSHUT GPIO_NUM_21

// ---- UI --------------------------------------------------------------------
#define PIN_BUTTON_TALK GPIO_NUM_0   // BOOT button, input only
#define PIN_STATUS_LED  GPIO_NUM_48  // onboard WS2812

// ---- Reserved for later (do not use) ---------------------------------------
// GPIO 1                        : wake button
// USB D- 19 / D+ 20               : native USB
// Never use: 0 as output, 3/45/46 (strapping), 26..32 (flash), 35..37 (OPI PSRAM)
