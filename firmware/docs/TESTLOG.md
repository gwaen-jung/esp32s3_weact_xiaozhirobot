# Test log

Legend: PASS (verified), SIM (verified off-target), HW (needs the real device).

## 2026-10-11, cloud sandbox (no hardware)

| Milestone | Test | Result |
|---|---|---|
| M0 | `python3 -m unittest discover -s scripts/tests` (100 host tests, incl. board menu/CMake consistency) | PASS |
| M0 | `scripts/build.py --list-boards` shows `esp32s3-weact-dog` | PASS |
| M0 | Compile `rgb565_canvas.cc`, `eyes_display.cc`, `leg_servos.cc`, `dog_behavior.cc`, `weact_dog_board.cc` with xtensa-esp32s3 GCC 15.2, IDF v6.0.1, real headers (esp-ml307, lvgl 9.5.0, esp-sr, esp_lcd_ili9341, button, led_strip, esp_audio_codec/effects, cJSON), `-Wall -Werror -Wextra` | PASS, 0 warnings |
| M0 | Full `idf.py build` | NOT RUN: `components.espressif.com` blocked by the sandbox egress policy. Run locally. |
| M2 | Host render of RoboEyes port into the canvas: default, happy, tired, angry, sweat, look NW, curious E, subtitle with Vietnamese text | SIM PASS (found + fixed a circle-fill bug: missing column in rounded corners) |
| M2 | Dirty box size worst case during large moves: 71,980 px (~29 ms at 40 MHz). Typical blink/mood change is much smaller | SIM (fps on target: HW) |
| M5 | Host simulation of `LegServos` (fake LEDC/FreeRTOS): attach + stand, wiggle, sit, wave, crouch, soft wiggle, breathe, quick stand | SIM PASS |
| M5 | Max servos ramping in the same tick | 2 (limit 2) |
| M5 | Min gap between start-ups | 80 ms (limit 50..100) |
| M5 | Max step per 20 ms tick | 1.85 deg (90 deg/s = 1.8, +0.05 from 14-bit duty quantisation) |
| M5 | Physical angles seen | 55..130 (clamp 45..135) |
| M5 | PWM on after each motion settles + 1 s | 0 channels (all detached) |

## To do on hardware (fill in)

| M | Test | Result / notes |
|---|---|---|
| M1 | Boot: screen black, eyes open from closed, colours cyan on black, landscape the right way up | |
| M2 | Enable `CONFIG_DOG_EYES_DEBUG_CYCLE`, watch moods cycle, log shows fps >= 25 | |
| M3 | Talk: mic level in log, speaker clean, no clipping at volume 60; amp SD pin mutes when idle | |
| M4 | Wi-Fi provisioning, 6-digit code shown + spoken, bind on xiaozhi.me, BOOT click -> listening, reply heard | |
| M5 | Each pose; no brownout (reset reason log stays "software/power-on") while cycling | |
| M6 | Emotions drive eyes + legs; 30 min soak without reset or audio glitch | |
