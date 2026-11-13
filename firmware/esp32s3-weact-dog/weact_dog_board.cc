// XIAOZHI dog robot on a WeAct ESP32-S3 CoreBoard N16R8:
// ILI9341 eyes, INMP441 mic + WeAct I2S Speaker (PCM5100A) on one I2S bus.
// 4 servo legs (FL 7, FR 4 = 360 deg, RL 6, RR 5).
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_lcd_ili9341.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <mutex>
#include <vector>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "dog_behavior.h"
#include "dog_config.h"
#include "eyes_display.h"
#include "led/single_led.h"
#include "leg_servos.h"
#include "tof_sensor.h"
#include "wifi_board.h"

#define TAG "WeactDogBoard"

// INMP441 + PCM5100A share BCLK/WS: one I2S port, TX and RX at the same rate,
// 32-bit slots (INMP441 needs 64 BCLK per frame). Unlike NoAudioCodecDuplex the
// speaker side sends the mono stream to both slots (PCM5100A L+R outputs).
class DogAudioCodec : public NoAudioCodec {
public:
    // Exhibition hall is loud: never start below the default (a lower NVS value is raised).
    void Start() override {
        NoAudioCodec::Start();
        if (output_volume_ < AUDIO_DEFAULT_OUTPUT_VOLUME) SetOutputVolume(AUDIO_DEFAULT_OUTPUT_VOLUME);
    }

    DogAudioCodec() {
        duplex_ = true;
        input_sample_rate_ = AUDIO_INPUT_SAMPLE_RATE;
        output_sample_rate_ = AUDIO_OUTPUT_SAMPLE_RATE;
        output_volume_ = AUDIO_DEFAULT_OUTPUT_VOLUME;  // NVS value wins if the user changed it

        i2s_chan_config_t chan_cfg = {
            .id = XIAOZHI_I2S_PORT(0),
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = (uint32_t)AUDIO_OUTPUT_SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_MONO,
                .slot_mask = I2S_STD_SLOT_BOTH,  // speaker: same sample on L and R
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,  // PCM5100A uses its internal PLL (MC -> GND)
                .bclk = PIN_I2S_BCLK,
                .ws = PIN_I2S_WS,
                .dout = PIN_I2S_DOUT,
                .din = PIN_I2S_DIN,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
        // Mic: read both slots and use whichever carries the INMP441 (L/R -> GND
        // = left, L/R -> VDD or floating = right), so a wiring change cannot mute it.
        std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
        std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_LOGI(TAG, "Full-duplex I2S: BCLK %d, WS %d, DIN %d, DOUT %d @ %d Hz", PIN_I2S_BCLK,
                 PIN_I2S_WS, PIN_I2S_DIN, PIN_I2S_DOUT, AUDIO_OUTPUT_SAMPLE_RATE);
    }

protected:
    int Read(int16_t* dest, int samples) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        stereo_.resize((size_t)samples * 2);
        size_t bytes_read = 0;
        if (i2s_channel_read(rx_handle_, stereo_.data(), stereo_.size() * sizeof(int32_t),
                             &bytes_read, 200) != ESP_OK) {
            return 0;
        }
        int frames = (int)(bytes_read / (2 * sizeof(int32_t)));
        int64_t sum[2] = {0, 0};
        for (int i = 0; i < frames; ++i) {
            for (int c = 0; c < 2; ++c) {
                int32_t v = stereo_[i * 2 + c] >> 12;  // same gain as NoAudioCodec::Read
                sum[c] += v < 0 ? -v : v;
            }
            int32_t v = stereo_[i * 2 + mic_slot_] >> 12;
            dest[i] = (v > INT16_MAX) ? INT16_MAX : (v < -INT16_MAX) ? -INT16_MAX : (int16_t)v;
        }
        if (frames > 0) {
            for (int c = 0; c < 2; ++c) level_[c] += ((float)sum[c] / frames - level_[c]) * 0.05f;
            // Switch slot only when the other one is clearly louder.
            int other = mic_slot_ ^ 1;
            if (level_[other] > 4.0f * level_[mic_slot_] + 20.0f) {
                mic_slot_ = other;
                ESP_LOGW(TAG, "mic signal found on the %s slot, switching", other ? "RIGHT" : "LEFT");
            }
        }
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_log_ms_ > 3000) {
            last_log_ms_ = now;
            ESP_LOGI(TAG, "mic level L %d / R %d (using %s)%s", (int)level_[0], (int)level_[1],
                     mic_slot_ ? "R" : "L",
                     (level_[0] < 3.0f && level_[1] < 3.0f) ? "  <- NO SIGNAL: check mic SD/SCK/WS/VDD wiring" : "");
        }
        return frames;
    }

private:
    std::vector<int32_t> stereo_;
    float level_[2] = {0, 0};
    int mic_slot_ = 0;
    int64_t last_log_ms_ = 0;
};

class WeactDogBoard : public WifiBoard {
private:
    Button boot_button_;
    EyesDisplay* display_ = nullptr;
    LegServos legs_;
    DogBehavior* behavior_ = nullptr;
    spi_device_handle_t touch_spi_ = nullptr;
    TaskHandle_t touch_task_ = nullptr;

    uint16_t Xpt2046Read(uint8_t cmd) {
        if (!touch_spi_) return 0;
        uint8_t tx[3] = {cmd, 0x00, 0x00};
        uint8_t rx[3] = {0, 0, 0};
        spi_transaction_t t = {};
        t.length = 24;
        t.tx_buffer = tx;
        t.rx_buffer = rx;
        if (spi_device_transmit(touch_spi_, &t) != ESP_OK) return 0;
        return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3);
    }

    static void TouchTaskEntry(void* arg) {
        static_cast<WeactDogBoard*>(arg)->TouchLoop();
    }

    void TouchLoop() {
        // Ensure PENIRQ (PD1:PD0 = 00) is enabled at startup
        Xpt2046Read(0xD0);

        bool armed = true;
        int press_count = 0;
        int release_count = 0;
        int64_t last_trigger_ms = 0;

        while (true) {
            vTaskDelay(pdMS_TO_TICKS(40));
            uint16_t z1 = Xpt2046Read(0xB1);
            uint16_t z2 = Xpt2046Read(0xC1);
            uint16_t raw_x = Xpt2046Read(0x91);
            uint16_t raw_y = Xpt2046Read(0xD0);  // PD=00 re-enables T_IRQ

            int z = (int)z1 + 4095 - (int)z2;
            bool pressed = (z1 > 60 && z1 < 4090 && z > 400 &&
                            raw_x > 100 && raw_x < 4000 &&
                            raw_y > 100 && raw_y < 4000);

            int64_t now = esp_timer_get_time() / 1000;
            if (pressed) {
                release_count = 0;
                if (armed && ++press_count >= 2 && (now - last_trigger_ms > 900)) {
                    armed = false;
                    press_count = 0;
                    last_trigger_ms = now;

                    auto& app = Application::GetInstance();
                    DeviceState state = app.GetDeviceState();
                    if (state == kDeviceStateIdle || state == kDeviceStateSpeaking) {
                        ESP_LOGI(TAG, "Touch screen (Z=%d, X=%u, Y=%u) -> listen", z, raw_x, raw_y);
                        app.ToggleChatState();
                    } else {
                        ESP_LOGI(TAG, "Touch screen in state %d -> wink", (int)state);
                        if (display_) display_->Wink();
                    }
                }
            } else {
                press_count = 0;
                if (!armed && ++release_count >= 4) {
                    armed = true;
                }
            }
        }
    }

    // Touch on TPM408-2.8inch (XPT2046): tap screen -> start listening (or interrupt speaking).
    void InitializeTouch() {
        spi_device_interface_config_t devcfg = {};
        devcfg.clock_speed_hz = 2 * 1000 * 1000;  // 2 MHz for XPT2046
        devcfg.mode = 0;
        devcfg.spics_io_num = PIN_TOUCH_CS;
        devcfg.queue_size = 1;
        ESP_ERROR_CHECK(spi_bus_add_device(DISPLAY_SPI_HOST, &devcfg, &touch_spi_));

        xTaskCreate(&WeactDogBoard::TouchTaskEntry, "touch_xpt", 4096, this, 2, &touch_task_);
        ESP_LOGI(TAG, "XPT2046 Touch initialized on SPI2 (CS=%d, IRQ=%d, MISO=%d)",
                 PIN_TOUCH_CS, PIN_TOUCH_IRQ, PIN_TFT_MISO);
    }

    static void LogResetReason() {
        const char* name = "unknown";
        switch (esp_reset_reason()) {
            case ESP_RST_POWERON: name = "power-on"; break;
            case ESP_RST_SW: name = "software"; break;
            case ESP_RST_PANIC: name = "panic"; break;
            case ESP_RST_INT_WDT: name = "interrupt watchdog"; break;
            case ESP_RST_TASK_WDT: name = "task watchdog"; break;
            case ESP_RST_WDT: name = "other watchdog"; break;
            case ESP_RST_DEEPSLEEP: name = "deep sleep"; break;
            case ESP_RST_BROWNOUT: name = "BROWNOUT (check USB cable / power supply)"; break;
            case ESP_RST_USB: name = "usb"; break;
            case ESP_RST_EXT: name = "external pin"; break;
            default: break;
        }
        if (esp_reset_reason() == ESP_RST_BROWNOUT) {
            ESP_LOGE(TAG, "Reset reason: %s", name);
        } else {
            ESP_LOGI(TAG, "Reset reason: %s", name);
        }
    }

#if !DOG_SERVOS_ENABLED
    // Servo signal lines LOW: no pulses, servos stay limp and quiet.
    static void ParkServoPins() {
        gpio_config_t io = {};
        io.pin_bit_mask = (1ULL << PIN_SERVO_FL) | (1ULL << PIN_SERVO_FR) |
                          (1ULL << PIN_SERVO_RL) | (1ULL << PIN_SERVO_RR);
        io.mode = GPIO_MODE_OUTPUT;
        gpio_config(&io);
        for (auto pin : {PIN_SERVO_FL, PIN_SERVO_FR, PIN_SERVO_RL, PIN_SERVO_RR}) {
            gpio_set_level(pin, 0);
        }
        ESP_LOGI(TAG, "Servo legs disabled (DOG_SERVOS_ENABLED = 0)");
    }
#endif

    void InitializeSpi() {
        // Keep XPT2046 T_CS high while LCD initializes
        gpio_set_direction(PIN_TOUCH_CS, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_TOUCH_CS, 1);

        gpio_config_t irq_io = {};
        irq_io.pin_bit_mask = (1ULL << PIN_TOUCH_IRQ);
        irq_io.mode = GPIO_MODE_INPUT;
        irq_io.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&irq_io);

        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = PIN_TFT_MOSI;
        buscfg.miso_io_num = PIN_TFT_MISO;
        buscfg.sclk_io_num = PIN_TFT_SCLK;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * EYES_PUSH_LINES * sizeof(uint16_t) + 64;
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = PIN_TFT_CS;
        io_config.dc_gpio_num = PIN_TFT_DC;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_HZ;
        io_config.trans_queue_depth = 4;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = PIN_TFT_RST;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new EyesDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    void InitializeButtons() {
        // Push-to-talk: a click toggles listening (upstream behaviour).
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    WeactDogBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        LogResetReason();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
#ifdef CONFIG_DOG_EYES_DEBUG_CYCLE
        display_->StartDebugCycle(2500);  // bench test: cycle moods
#endif
#if DOG_SERVOS_ENABLED
        legs_.Begin();  // task only; servos attach after Wi-Fi + audio are up
#else
        ParkServoPins();  // legs disabled: Play()/AttachSequential() are no-ops
#endif
        behavior_ = new DogBehavior(display_, &legs_);
        behavior_->Start();
        InitializeTouch();
    }

    // USB powered desk robot: keep the radio fully awake. Modem sleep between
    // DTIM beacons adds latency/jitter to the voice stream and makes some
    // routers drop the station, which is the main cause of disconnects.
    void SetPowerSaveLevel(PowerSaveLevel level) override {
        (void)level;
        WifiBoard::SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
    }

    Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    AudioCodec* GetAudioCodec() override {
        static DogAudioCodec audio_codec;
        return &audio_codec;
    }

    Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(WeactDogBoard);
