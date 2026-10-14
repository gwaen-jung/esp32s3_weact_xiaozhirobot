# esp32s3-weact-dog: XIAOZHI desk dog robot

WeAct ESP32-S3 CoreBoard N16R8 + ILI9341 2.4" TFT (animated RoboEyes) + INMP441 mic
+ PCM5100A I2S DAC. Chats through the Xiaozhi cloud. Servo legs are disabled.

## Wiring

| Function | ESP32-S3 GPIO | Module pin |
|---|---|---|
| TFT SCLK | 40 | SCK |
| TFT MOSI | 41 | SDI (MOSI) |
| TFT CS | 39 | CS |
| TFT DC | 38 | DC / RS |
| TFT RST | 47 | RESET |
| TFT LED, VCC / GND | 3.3 V / GND | |
| I2S BCLK (shared) | 16 | INMP441 SCK + PCM5100A BCK |
| I2S WS (shared) | 17 | INMP441 WS + PCM5100A LCK |
| Mic data in | 14 | INMP441 SD (L/R -> GND) |
| DAC data out | 15 | PCM5100A DIN (SCK/MC -> GND, XSMT -> 3.3 V) |
| Servo FL / FR / RL / RR | 7 / 4 / 6 / 5 | **disabled**, held LOW |
| Push-to-talk | 0 | BOOT button |
| Status LED | 48 | onboard WS2812 |
| Reserved | 8 SDA / 9 SCL / 21 XSHUT (ToF, IMU), 1 (wake button) | not used yet |

All pins are defined only in `board_pins.h`. Mic and DAC share one full-duplex
I2S port at 16 kHz, 32-bit slots.

## Build (ESP-IDF v6.0.1 or newer)

```sh
# Windows: ESP-IDF PowerShell / CMD from the ESP-IDF installer (v6.0.1+)
# Linux/macOS: source <esp-idf>/export.sh
cd xiaozhi-esp32
python scripts/build.py esp32s3-weact-dog --language vi-VN
idf.py -p COM5 flash monitor        # replace COM5 with your port
```

`--language` picks the voice prompts/UI strings (`en-US`, `zh-CN`, ...).
The first build downloads managed components from components.espressif.com (needs
internet), then takes several minutes.

Bench test of the eyes alone: `idf.py menuconfig` -> enable
"XIAOZHI Dog: cycle eye moods on a debug timer", rebuild. The log prints the fps.

## First start

1. With no saved Wi-Fi the robot opens its own hotspot. The hotspot name and the setup
   page address are shown at the bottom of the screen; connect with a phone, open the
   page and enter your Wi-Fi.
2. After it connects, it speaks a 6-digit code and shows it at the bottom of the screen.
   Add the device with that code on the Xiaozhi console (xiaozhi.me).
3. Click BOOT to start/stop listening. If the wake word is enabled, say it instead.

Press BOOT while it is still starting to re-enter Wi-Fi setup. No credentials are
compiled into the firmware.

## Configuration

| What | Where |
|---|---|
| Pins | `board_pins.h` |
| Panel orientation, colour order, SPI speed, audio rate, default volume | `config.h` |
| Eye colours per mood, sizes, fps, overlay colours, idle timings | `dog_config.h` |
| State/emotion -> eyes | tables at the top of `dog_behavior.cc` |
| Wi-Fi buffers (PSRAM) | `config.json` `sdkconfig_append` |

Volume can also be changed by voice ("giam am luong").

## Behaviour

| Trigger | Eyes |
|---|---|
| Boot | open from closed |
| Idle | cyan, blink, drift; random look-around / wink / smile every 12-25 s; sleepy + Zzz after 3 min |
| Listening | wide, centred, curious |
| Thinking (your words received) / connecting | look up-left, light blue, "?" |
| Speaking | keeps the emotion, gentle bob; reply text shown as subtitle |
| happy | yellow, sparkles |
| laughing / funny | yellow, laugh shake, music notes |
| loving | pink, floating hearts, blush |
| kissy | wink + hearts + blush |
| delicious | happy + tongue |
| sad | blue, droopy, looks down |
| crying | blue, droopy, falling tears |
| embarrassed | pink, looks away, blush + sweat |
| angry | red, angry lids, anger mark |
| surprised | white, wide, "!" |
| shocked | white, wide, shake, "!" |
| confused | violet, shake, "?" |
| sleepy | grey-blue, droopy, slow blink, Zzz |
| relaxed | green, half-closed |
| winking | wink + sparkles |
| cool / confident | sunglasses with moving glint |
| silly | orange, wobble, music notes |
| Wi-Fi setup / errors | orange, sweat |

## Wi-Fi stability

- Radio power save is forced off (`SetPowerSaveLevel` -> PERFORMANCE): no modem sleep,
  lower latency, fewer router-side disconnects.
- Wi-Fi/LWIP buffers moved to PSRAM and enlarged (more RX/TX buffers, bigger BA window
  and TCP window) so the voice stream does not stall on packet bursts.
- Servos removed: no more current spikes / brownouts that used to drop Wi-Fi.

## License

The eye animation is a port of FluxGarage RoboEyes (GPL-3.0-or-later,
`LICENSE-RoboEyes.txt`), so a firmware binary containing this board is GPL-3.0.
The subtitle font bitmaps are generated from DejaVu Sans Mono (`tools/gen_font_8x16.py`).
