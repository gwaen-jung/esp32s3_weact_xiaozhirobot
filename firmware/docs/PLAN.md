# XIAOZHI Dog Robot: plan and spike findings (M0)

Fork base: `78/xiaozhi-esp32` @ `c0fe42ad` (2026-10-10). Everything new lives in
`main/boards/esp32s3-weact-dog/`; upstream files touched: `main/Kconfig.projbuild`
(board entry + one debug option) and `main/CMakeLists.txt` (board branch).

## Spike findings

| Question | Finding |
|---|---|
| ESP-IDF version | **v6.0.1 minimum, v6.1 recommended.** IDF 5.x is no longer supported upstream (`AGENTS.md`, `README.md`). Deviation from the brief (which said 5.x). |
| Custom board | Chain `config.json -> scripts/build.py -> Kconfig.projbuild -> CMakeLists.txt`. Board dir sources are globbed (`*.cc`, `*.c`, top level only). Exactly one `DECLARE_BOARD`. Board menu must stay naturally sorted (host test `test_board_menu_is_sorted_and_matches_cmake`). |
| Custom display | Subclass `Display` (`main/display/display.h`), implement `Lock/Unlock` and override `SetStatus`, `ShowNotification`, `SetEmotion`, `SetChatMessage`, `ClearChatMessages`. Core never casts to `LvglDisplay`. |
| State machine | States in `device_state.h`. No "thinking" state upstream: we derive it from Listening + an `stt` (user text) message. Board code polls `Application::GetDeviceState()` (no core change). |
| Emotions | Server `llm` message -> `display->SetEmotion(str)` scheduled on the main task. Core also sends `neutral`, `warning`, `cloud_off`, `cancel`, `gear`, `link`, `robot_2`, `download`, `cloud_download`. |
| Audio | `NoAudioCodecSimplex` (two I2S ports, 32-bit slots) already fits INMP441 + MAX98357-style amp. Input 16 kHz, output 24 kHz (server Opus). Slot-mask constructor lets us duplicate mono to both speaker channels. |
| Push-to-talk | Upstream boards use `Button::OnClick -> ToggleChatState()` (toggle). Kept, least change. Wake word: upstream default for S3 (ESP-SR WakeNet) stays enabled, no extra work. |
| Provisioning / activation | Upstream: hotspot provisioning page + 6-digit activation code spoken by voice and sent to `SetChatMessage("system", ...)`. We render system messages as a subtitle so the code is also visible. |
| RoboEyes | Confirmed: **FluxGarage RoboEyes V1.1.1**, GPL-3.0-or-later, single header `src/FluxGarage_RoboEyes.h`, template on an Adafruit GFX display. API matches the brief. |

## Design

- `board_pins.h` single source of truth for GPIOs; `config.h` audio/display settings;
  `dog_config.h` all eye + servo constants (no magic numbers elsewhere).
- `roboeyes_port.h`: RoboEyes with `drawEyes()` logic unchanged. Port changes:
  namespace, `#define` constants -> `constexpr`, RGB565 colours, Arduino helpers,
  member-order fix (upstream reads `spaceBetweenDefault` before initialising it; fatal
  under IDF's `-Werror`), `sweatScale` so drops are visible on 320x240.
- `Rgb565Canvas`: Adafruit-compatible `fillRect/fillRoundRect/fillTriangle` on a
  320x240 RGB565 PSRAM frame buffer, dirty-box tracking (clear and push only last
  frame + this frame), 8x16 ASCII + Vietnamese subtitle font.
- `EyesDisplay`: render task (prio 2, core 1, 30 fps target). Copies the dirty box
  from PSRAM into two internal-DMA ping-pong buffers (24 lines) and pushes with
  `esp_lcd_panel_draw_bitmap`, completion via `on_color_trans_done`.
- `LegServos`: own task, 50 Hz tick. LEDC timer 1, 14-bit, 50 Hz, channels 2..5.
  Keyframe motions; at most 2 servos ramping, 80 ms stagger, 90 deg/s slew, logical
  clamp 45..135 then trim/invert then clamp again, PWM stopped after 1 s hold.
  Servos attach one by one (300 ms apart) on the first Idle state (Wi-Fi + audio up).
- `DogBehavior`: one task, two tables (`kStateTable`, `kEmotionTable`) mapping to eye
  presets + leg motions. Unknown emotion -> neutral, never crashes.
- `DogAudioCodec`: `NoAudioCodecSimplex` + amp SD pin follows `EnableOutput`,
  default volume 60.

## Deviations from the brief

1. ESP-IDF 6.0.1+ instead of 5.x (upstream requirement).
2. Extra emotions not listed in the brief (`winking`, `cool`, `confident`, `kissy`,
   `silly`) map to HAPPY eyes without leg motion.
3. "Speaking: vertical flicker synced to audio" not done (not cheap without touching
   the audio service): eyes are static/centred while speaking, legs do a soft wiggle
   every 3.5 s.
4. RoboEyes is GPL-3.0: the combined firmware binary is GPL-3.0 (upstream is MIT).
   Fine for personal/educational use; keep `LICENSE-RoboEyes.txt` with any
   distribution.
5. Build in the cloud sandbox could not reach `components.espressif.com` (egress
   policy), so a full `idf.py build` was not possible there. See `TESTLOG.md` for what
   was verified instead. Full build must be run locally.

## Milestone status

| M | Status |
|---|---|
| M0 | Spike done. Our 5 translation units compile with `-Werror` for esp32s3 against real IDF 6.0.1 + component headers. Full `idf.py build` pending (local). |
| M1-M3 | Code complete; needs hardware. |
| M4 | Upstream flow, needs hardware + Xiaozhi account. |
| M5 | Servo controller verified in host simulation (rules hold); needs hardware. |
| M6-M7 | Integration code + README done; soak test needs hardware. |
