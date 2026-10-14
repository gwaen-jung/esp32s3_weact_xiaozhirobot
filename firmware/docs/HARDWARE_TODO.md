# Hardware checks for the user

Each item is marked `// TODO(hw-verify)` in the code. Defaults are in brackets.

1. **WeAct I2S dual-speaker amp** (`board_pins.h`, `config.h`)
   - Real chip and pinout? Assumed MAX98357A style: BCLK=15, LRC=16, DIN=17, SD=18.
   - SD pin polarity: high = on [assumed]. If the module has no SD pin, set
     `PIN_AMP_SD` to `GPIO_NUM_NC`.
   - Gain pin: leave at the module default; lower it if the 4 ohm speakers distort.
2. **Speakers**: one or two wired? Stereo amp or mono mix?
   `AUDIO_SPK_STEREO_DUP 1` sends the same mono audio to both I2S slots [1].
   Set 0 to drive only the left slot.
3. **TFT ILI9341 module** (`config.h`)
   - Colours wrong (red/blue swapped): toggle `DISPLAY_RGB_ORDER` RGB/BGR [BGR].
   - Colours inverted (white background): toggle `DISPLAY_INVERT_COLOR` [false].
   - Upside down: set both `DISPLAY_MIRROR_X` and `DISPLAY_MIRROR_Y` to true.
   - Mirrored: flip only one of them.
   - Snow/garbage: lower `DISPLAY_SPI_HZ` to 26 or 20 MHz [40 MHz].
   - Backlight: tied to 3.3 V [assumed]. If it goes to a GPIO, set `PIN_TFT_BL`.
4. **Servo directions and trims** (`dog_config.h`, `kLegConfig`)
   - Convention: logical angle > 90 = leg forward/down. Right legs are `invert=true` [assumed mirror mount].
   - Put the robot in `stand`; adjust `trim_deg` per leg until all legs are straight.
5. **Power**: boost converter peak current and battery protection rating.
   Every boot logs `Reset reason: ...`; `BROWNOUT` means the supply dipped (servo
   peaks). Mitigations: lower `SERVO_SLEW_DEG_PER_S`, set `SERVO_MAX_MOVING` to 1,
   add a 470-1000 uF capacitor on the servo 5 V rail.
