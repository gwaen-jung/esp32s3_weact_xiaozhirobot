// XIAOZHI dog robot on a WeAct ESP32-S3 CoreBoard N16R8:
// ILI9341 eyes, INMP441 mic + PCM5100A DAC on one full-duplex I2S bus.
// The 4 servo legs are disabled (pins held LOW).
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <driver/spi_common.h>
#include <esp_lcd_ili9341.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_system.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "dog_behavior.h"
#include "dog_config.h"
#include "eyes_display.h"
#include "led/single_led.h"
#include "leg_servos.h"
#include "mcp_server.h"
#include "assets/lang_config.h"
#include "tof_sensor.h"
#include "wifi_board.h"

#define TAG "WeactDogBoard"

// INMP441 + PCM5100A share BCLK/WS: one I2S port, TX and RX at the same rate,
// 32-bit slots (INMP441 needs 64 BCLK per frame). Unlike NoAudioCodecDuplex the
// speaker side sends the mono stream to both slots (PCM5100A L+R outputs).
class DogAudioCodec : public NoAudioCodec {
public:
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
        std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;  // mic: INMP441 L/R -> GND
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_LOGI(TAG, "Full-duplex I2S: BCLK %d, WS %d, DIN %d, DOUT %d @ %d Hz", PIN_I2S_BCLK,
                 PIN_I2S_WS, PIN_I2S_DIN, PIN_I2S_DOUT, AUDIO_OUTPUT_SAMPLE_RATE);
    }
};

class WeactDogBoard : public WifiBoard {
private:
    Button boot_button_;
    EyesDisplay* display_ = nullptr;
    DogBehavior* behavior_ = nullptr;
    LegServos legs_;
    TofSensor tof_{PIN_I2C_SDA, PIN_I2C_SCL, PIN_TOF_XSHUT};

    // Hand closer than 5 cm in front of the ToF = same as a BOOT click:
    // Idle -> open the channel and listen; Speaking -> interrupt and listen.
    void InitializeTof() {
        tof_.Start([](int distance_mm) {
            auto& app = Application::GetInstance();
            DeviceState state = app.GetDeviceState();
            if (state == kDeviceStateIdle || state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "ToF %d mm -> listen", distance_mm);
                app.ToggleChatState();
            }
        });
    }

    void InitializeMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        // 1. Tool control_dog_legs: điều khiển 4 chân thực hiện cử chỉ
        mcp_server.AddTool(
            "self.control_dog_legs",
            "Điều khiển 4 chân cún robot thực hiện các cử chỉ: 'stand' (đứng thẳng), 'sit' (ngồi), "
            "'crouch' (cúi/nằm), 'wiggle' (lắc mông mừng), 'wave' (chào/bắt tay), 'relax' (thả lỏng ngắt servo), "
            "'zero' (về góc 0 độ).",
            PropertyList({Property("action", kPropertyTypeString)}),
            [this](const PropertyList& properties) -> ToolResult {
                auto action = properties["action"].value<std::string>();
                if (action == "stand" || action == "dung_day") {
                    legs_.Play(LegMotion::kStand);
                    return std::string("Gâu gâu! Cún đã đứng thẳng dậy rồi nè chủ nhân!");
                } else if (action == "sit" || action == "ngoi_xuong") {
                    legs_.Play(LegMotion::kSit);
                    return std::string("Gâu! Cún đã ngồi ngoan rồi ạ!");
                } else if (action == "crouch" || action == "nam_xuong") {
                    legs_.Play(LegMotion::kCrouch);
                    return std::string("Cún nằm rạp xuống sát đất rồi nha!");
                } else if (action == "wiggle" || action == "lac_mong") {
                    legs_.Play(LegMotion::kWiggle);
                    return std::string("Gâu gâu gâu! Cún đang lắc mông mừng rỡ nè!");
                } else if (action == "wave" || action == "chao") {
                    legs_.Play(LegMotion::kWave);
                    return std::string("Gâu! Cún giơ chân chào bạn nha!");
                } else if (action == "relax" || action == "tha_long") {
                    legs_.Play(LegMotion::kRelax);
                    return std::string("Cún đã thả lỏng các khớp chân để nghỉ ngơi.");
                } else if (action == "zero" || action == "ve_0") {
                    legs_.Play(LegMotion::kZero);
                    return std::string("Đã đưa các chân về góc 0 độ.");
                }
                return std::unexpected("Hành động không hợp lệ: " + action);
            });

        // 2. Tool set_leg_angle: điều khiển góc quay từng chân
        mcp_server.AddTool(
            "self.set_leg_angle",
            "Điều khiển góc quay của một chân servo cụ thể (0 đến 180 độ). Chân gồm: 'FL', 'FR', 'RL', 'RR'.",
            PropertyList({Property("leg", kPropertyTypeString), Property("angle", kPropertyTypeInteger)}),
            [this](const PropertyList& properties) -> ToolResult {
                auto leg_str = properties["leg"].value<std::string>();
                int angle = properties["angle"].value<int>();
                int leg_idx = -1;
                if (leg_str == "FL" || leg_str == "fl") leg_idx = 0;
                else if (leg_str == "FR" || leg_str == "fr") leg_idx = 1;
                else if (leg_str == "RL" || leg_str == "rl") leg_idx = 2;
                else if (leg_str == "RR" || leg_str == "rr") leg_idx = 3;
                else return std::unexpected("Tên chân không hợp lệ (hãy dùng FL, FR, RL, RR): " + leg_str);

                legs_.SetLegAngle(leg_idx, (float)angle);
                char buf[64];
                snprintf(buf, sizeof(buf), "Đã chỉnh chân %s về góc %d độ", leg_str.c_str(), angle);
                return std::string(buf);
            });

        // 3. Tool save_stand_pose: lưu góc đứng hiện tại vào NVS
        mcp_server.AddTool(
            "self.save_stand_pose",
            "Lưu góc đứng hiện tại của 4 chân vào bộ nhớ Flash NVS vĩnh viễn.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                legs_.SaveStandPose();
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Đã lưu góc đứng vào Flash NVS: FL=%d°, FR=%d°, RL=%d°, RR=%d°",
                         legs_.GetStandAngle(0), legs_.GetStandAngle(1), legs_.GetStandAngle(2), legs_.GetStandAngle(3));
                return std::string(buf);
            });

        // 4. Tool get_leg_angles: xem góc chân hiện tại và góc đã lưu
        mcp_server.AddTool(
            "self.get_leg_angles",
            "Xem góc hiện tại và góc đứng đã lưu của 4 chân.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "Góc hiện tại: FL=%.0f°, FR=%.0f°, RL=%.0f°, RR=%.0f° | Góc đứng đã lưu: FL=%d°, FR=%d°, RL=%d°, RR=%d°",
                         legs_.GetLegAngle(0), legs_.GetLegAngle(1), legs_.GetLegAngle(2), legs_.GetLegAngle(3),
                         legs_.GetStandAngle(0), legs_.GetStandAngle(1), legs_.GetStandAngle(2), legs_.GetStandAngle(3));
                return std::string(buf);
            });

        // 5. Tool get_front_distance: đo khoảng cách ToF
        mcp_server.AddTool(
            "self.get_front_distance",
            "Đo khoảng cách vật cản phía trước mặt cún (tính bằng cm/mm) bằng cảm biến laser ToF VL53L0X.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                int dist_mm = tof_.last_distance_mm();
                if (dist_mm <= 0 || dist_mm > 2000) {
                    return std::string("Phía trước cún thông thoáng, không có vật cản gần (khoảng cách > 2 mét).");
                }
                char buf[64];
                snprintf(buf, sizeof(buf), "Khoảng cách vật cản trước mũi: %.1f cm (%d mm)", dist_mm / 10.0f, dist_mm);
                return std::string(buf);
            });

        // 6. Tool play_music: phát bài nhạc Canon in D qua loa
        mcp_server.AddTool(
            "self.play_music",
            "Phát một bài nhạc vui vẻ (Canon in D) qua loa của robot.",
            PropertyList(),
            [this](const PropertyList& /*properties*/) -> ToolResult {
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_MUSIC);
                if (display_) {
                    display_->SetEmotion("laugh");
                }
                return std::string("Đang phát bài nhạc Canon in D cho bạn nghe nè!");
            });

        ESP_LOGI(TAG, "Registered Dog MCP tools: self.control_dog_legs, self.set_leg_angle, self.save_stand_pose, self.get_leg_angles, self.get_front_distance, self.play_music");
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

    void InitializeSpi() {
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

        // Nhấp đúp nút BOOT: Phát bài nhạc Canon in D
        boot_button_.OnDoubleClick([this]() {
            ESP_LOGI(TAG, "BOOT double click -> Play music");
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_MUSIC);
            if (display_) {
                display_->SetEmotion("laugh");
            }
        });
    }

public:
    WeactDogBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        LogResetReason();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeMcpTools();
#ifdef CONFIG_DOG_EYES_DEBUG_CYCLE
        display_->StartDebugCycle(2500);  // bench test: cycle moods
#endif
        legs_.Begin();
        behavior_ = new DogBehavior(display_, &legs_);
        behavior_->Start();
        InitializeTof();
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
