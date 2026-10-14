#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include "board_pins.h"

// Audio: mic and DAC share one I2S bus (same clock), so both run at 16 kHz.
// 16 kHz is what the AFE / wake word wants (no input resampler, less CPU) and
// the Opus decoder outputs 16 kHz directly.
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
// Default volume (0..100, changeable by voice; the NVS value wins once set).
#define AUDIO_DEFAULT_OUTPUT_VOLUME 70

#define BOOT_BUTTON_GPIO PIN_BUTTON_TALK
#define BUILTIN_LED_GPIO PIN_STATUS_LED

// Display: ILI9341 in landscape (same as Adafruit rotation 1, verified).
#define DISPLAY_WIDTH    320
#define DISPLAY_HEIGHT   240
#define DISPLAY_SPI_HOST SPI2_HOST
#define DISPLAY_SPI_HZ   (26 * 1000 * 1000)  // Arduino test was stable at 27 MHz
#define DISPLAY_SPI_MODE 0
#define DISPLAY_SWAP_XY      true
#define DISPLAY_MIRROR_X     false  // swap+no mirror = Adafruit rotation 1 (MADCTL 0x28)
#define DISPLAY_MIRROR_Y     false  // set both true for rotation 3 (upside down)
#define DISPLAY_INVERT_COLOR false
#define DISPLAY_RGB_ORDER    LCD_RGB_ELEMENT_ORDER_BGR

#endif  // _BOARD_CONFIG_H_
