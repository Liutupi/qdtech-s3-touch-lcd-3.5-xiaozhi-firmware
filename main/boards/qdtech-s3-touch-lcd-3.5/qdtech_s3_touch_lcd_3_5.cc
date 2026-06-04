#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "desktop_ui.h"
#include "radio_service.h"
#include "time_weather_service.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st77922.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <wifi_station.h>

#define TAG "QDtech35"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

static const st77922_lcd_init_cmd_t qdtech_st77922_init_cmds[] = {
    {0xF1, (uint8_t[]){0x00}, 1, 0},
    {0x60, (uint8_t[]){0x00, 0x00, 0x00}, 3, 0},
    {0x65, (uint8_t[]){0x80}, 1, 0},
    {0x79, (uint8_t[]){0x06}, 1, 0},
    {0x7B, (uint8_t[]){0x00, 0x08, 0x08}, 3, 0},
    {0x80, (uint8_t[]){0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA}, 11, 0},
    {0x81, (uint8_t[]){0x26, 0x52, 0x72, 0x27}, 4, 0},
    {0x84, (uint8_t[]){0x92, 0x25}, 2, 0},
    {0x87, (uint8_t[]){0x10, 0x10, 0x58, 0x00, 0x02, 0x3A}, 6, 0},
    {0x88, (uint8_t[]){0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06}, 15, 0},
    {0x89, (uint8_t[]){0x00, 0x00, 0x00}, 3, 0},
    {0x8A, (uint8_t[]){0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19}, 11, 0},
    {0x8B, (uint8_t[]){0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E}, 9, 0},
    {0x8C, (uint8_t[]){0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09}, 13, 0},
    {0x8D, (uint8_t[]){0x0C}, 1, 0},
    {0x8E, (uint8_t[]){0x33, 0x01, 0x0C, 0x13, 0x01, 0x01}, 6, 0},
    {0xB3, (uint8_t[]){0x00, 0x30}, 2, 0},
    {0xF1, (uint8_t[]){0x00}, 1, 0},
    {0x71, (uint8_t[]){0xD0}, 1, 0},
    {0x66, (uint8_t[]){0x02, 0x3F}, 2, 0},
    {0xBE, (uint8_t[]){0x26, 0x00, 0x9D}, 3, 0},
    {0x70, (uint8_t[]){0x01, 0xA0, 0x11, 0x40, 0xE0, 0x00, 0x11, 0x69, 0x11, 0x00, 0x00, 0x1A}, 12, 0},
    {0x90, (uint8_t[]){0x04, 0x04, 0x55, 0x74, 0x00, 0x40, 0x43, 0x27, 0x27}, 9, 0},
    {0x91, (uint8_t[]){0x04, 0x04, 0x55, 0x75, 0x00, 0x40, 0x42, 0x27, 0x27}, 9, 0},
    {0x92, (uint8_t[]){0x04, 0x44, 0x55, 0xC0, 0x06, 0x00, 0x07, 0x05, 0x90, 0x27}, 10, 0},
    {0x93, (uint8_t[]){0x04, 0x43, 0x11, 0x00, 0x00, 0x00, 0x00, 0x05, 0x90, 0x27}, 10, 0},
    {0x94, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6, 0},
    {0x95, (uint8_t[]){0x96, 0x16, 0x00, 0x00, 0xFF}, 5, 0},
    {0x96, (uint8_t[]){0x44, 0x53, 0x03, 0x12, 0x23, 0x24, 0x06, 0x05, 0x94, 0x27, 0x00, 0x44}, 12, 0},
    {0x97, (uint8_t[]){0x44, 0x53, 0x47, 0x56, 0x20, 0x20, 0x02, 0x01, 0x94, 0x27, 0x00, 0x44}, 12, 0},
    {0xBA, (uint8_t[]){0x55, 0x94, 0x2D, 0x94, 0x27}, 5, 0},
    {0x9A, (uint8_t[]){0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9B, (uint8_t[]){0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9C, (uint8_t[]){0x5C, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00}, 13, 0},
    {0x9D, (uint8_t[]){0x8A, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 8, 0},
    {0x9E, (uint8_t[]){0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 7, 0},
    {0xB4, (uint8_t[]){0x1D, 0x1C, 0x1E, 0x0B, 0x14, 0x02, 0x13, 0x09, 0x1E, 0x00, 0x1E, 0x10}, 12, 0},
    {0xB5, (uint8_t[]){0x1D, 0x1C, 0x1E, 0x0A, 0x15, 0x03, 0x11, 0x08, 0x1E, 0x01, 0x1E, 0x12}, 12, 0},
    {0xB6, (uint8_t[]){0x77, 0x77, 0x00, 0x0A, 0xFF, 0x0A, 0xFF}, 7, 0},
    {0x86, (uint8_t[]){0xCD, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x0C, 0x13, 0x01, 0xA5, 0x00, 0xA5, 0xA5}, 14, 0},
    {0xB7, (uint8_t[]){0x07, 0x0A, 0x0E, 0x06, 0x05, 0x03, 0x2B, 0x03, 0x03, 0x42, 0x07, 0x10, 0x10, 0x2E, 0x3F, 0x0D}, 16, 0},
    {0xB8, (uint8_t[]){0x07, 0x0A, 0x0D, 0x05, 0x05, 0x02, 0x2B, 0x02, 0x03, 0x42, 0x06, 0x10, 0x0F, 0x2E, 0x3F, 0x0D}, 16, 0},
    {0xB9, (uint8_t[]){0x23, 0x23}, 2, 0},
    {0xBF, (uint8_t[]){0x10, 0x14, 0x14, 0x0B, 0x0B, 0x0B}, 6, 0},
    {0xF2, (uint8_t[]){0x00}, 1, 0},
    {0x73, (uint8_t[]){0x04, 0xDA, 0x12, 0x54, 0x47}, 5, 0},
    {0x77, (uint8_t[]){0x6B, 0x5B, 0xFD, 0xC3, 0xC5}, 5, 0},
    {0x7A, (uint8_t[]){0x15, 0x27}, 2, 0},
    {0x7B, (uint8_t[]){0x04, 0x57}, 2, 0},
    {0x7E, (uint8_t[]){0x01, 0x0E}, 2, 0},
    {0xBF, (uint8_t[]){0x36}, 1, 0},
    {0xE3, (uint8_t[]){0x40, 0x40}, 2, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xD0, (uint8_t[]){0x00}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x3F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x21, (uint8_t[]){0x00}, 0, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
    {0x2C, (uint8_t[]){0x00}, 0, 0},
    {0x3A, (uint8_t[]){0x01}, 1, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x35, (uint8_t[]){0x01}, 1, 20},
};

class QdtechLandscapeDisplay : public LcdDisplay {
public:
    QdtechLandscapeDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : LcdDisplay(panel_io, panel, fonts, DISPLAY_WIDTH, DISPLAY_HEIGHT) {
        uint16_t white_line[DISPLAY_PANEL_WIDTH];
        std::fill_n(white_line, DISPLAY_PANEL_WIDTH, 0xFFFF);
        for (int y = 0; y < DISPLAY_PANEL_HEIGHT; y++) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_, 0, y, DISPLAY_PANEL_WIDTH, y + 1, white_line));
        }

        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        lv_init();
        lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
        port_cfg.task_priority = 1;
        port_cfg.timer_period_ms = 50;
        ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

        transfer_done_ = xSemaphoreCreateCounting(1, 0);
        ESP_ERROR_CHECK(transfer_done_ ? ESP_OK : ESP_ERR_NO_MEM);

        const esp_lcd_panel_io_callbacks_t callbacks = {
            .on_color_trans_done = OnColorDone,
        };
        ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &callbacks, this));

        const size_t draw_pixels = DISPLAY_WIDTH * DISPLAY_HEIGHT;
        const size_t transfer_pixels = DISPLAY_WIDTH * DISPLAY_HEIGHT / 10;
        frame_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(draw_pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        transfer_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(transfer_pixels * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
        ESP_ERROR_CHECK((frame_buffer_ && transfer_buffer_) ? ESP_OK : ESP_ERR_NO_MEM);

        display_ = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
        ESP_ERROR_CHECK(display_ ? ESP_OK : ESP_ERR_NO_MEM);
        lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
        lv_display_set_buffers(display_, frame_buffer_, nullptr, draw_pixels * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display_, FlushCallback);
        lv_display_set_user_data(display_, this);

        // 在锁内创建桌面UI
        {
            DisplayLockGuard lock(this);
            desktop_ui_.Create();
            lv_obj_invalidate(lv_screen_active());
        }
    }

    ~QdtechLandscapeDisplay() {
        // 清理 lvgl_port_add_disp 资源
    }

    void SetEmotion(const char* emotion) override {
        DisplayLockGuard lock(this);
        LcdDisplay::SetEmotion(emotion);
        desktop_ui_.SetXiaozhiEmotion(emotion);
    }

    void SetChatMessage(const char* role, const char* content) override {
        LcdDisplay::SetChatMessage(role, content);

        DisplayLockGuard lock(this);
        const char* state = "System";
        if (role && strcmp(role, "user") == 0) {
            state = "You";
        } else if (role && strcmp(role, "assistant") == 0) {
            state = "XiaoZhi";
        }

        if (content && strlen(content) > 0) {
            desktop_ui_.SetXiaozhiState(state, content, nullptr);
        } else {
            desktop_ui_.SetXiaozhiState("Standby", "", nullptr);
        }
    }

    DesktopUI* GetDesktopUI() {
        return &desktop_ui_;
    }

private:
    DesktopUI desktop_ui_;

    static bool OnColorDone(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
        auto* self = static_cast<QdtechLandscapeDisplay*>(user_ctx);
        BaseType_t need_yield = pdFALSE;
        xSemaphoreGiveFromISR(self->transfer_done_, &need_yield);
        return need_yield == pdTRUE;
    }

    static void FlushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* color_map) {
        auto* self = static_cast<QdtechLandscapeDisplay*>(lv_display_get_user_data(display));
        self->Flush(area, reinterpret_cast<const uint16_t*>(color_map));
    }

    void Flush(const lv_area_t* area, const uint16_t* color_map) {
        const int x_start = area->x1;
        const int x_end = area->x2;
        const int y_start = area->y1;
        const int y_end = area->y2;
        const int width = x_end - x_start + 1;
        const int height = y_end - y_start + 1;
        constexpr int transfer_pixels = DISPLAY_WIDTH * DISPLAY_HEIGHT / 10;

        int max_width = transfer_pixels / height;
        if (max_width < 1) {
            max_width = 1;
        }

        int x_start_tmp = x_start;
        while (x_start_tmp <= x_end) {
            const int trans_width = std::min(max_width, x_end - x_start_tmp + 1);
            const int x_end_tmp = x_start_tmp + trans_width - 1;

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < trans_width; x++) {
                    uint16_t color = color_map[y * width + (x_start_tmp - x_start) + x];
                    color = (color << 8) | (color >> 8);
                    transfer_buffer_[x * height + (height - y - 1)] = color;
                }
            }

            const int draw_x_start = DISPLAY_HEIGHT - y_end - 1;
            const int draw_x_end = DISPLAY_HEIGHT - y_start - 1;
            const int draw_y_start = x_start_tmp;
            const int draw_y_end = x_end_tmp;

            xSemaphoreTake(transfer_done_, 0);
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_, draw_x_start, draw_y_start, draw_x_end + 1, draw_y_end + 1, transfer_buffer_));
            xSemaphoreTake(transfer_done_, pdMS_TO_TICKS(1000));
            x_start_tmp += trans_width;
        }

        lv_display_flush_ready(display_);
    }

    uint16_t* frame_buffer_ = nullptr;
    uint16_t* transfer_buffer_ = nullptr;
    SemaphoreHandle_t transfer_done_ = nullptr;
};

class QdtechTouch {
public:
    void Init(i2c_master_bus_handle_t bus) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = TOUCH_I2C_ADDR,
            .scl_speed_hz = 400000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev_));

        gpio_config_t rst_cfg = {
            .pin_bit_mask = 1ULL << TOUCH_RST_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&rst_cfg));

        gpio_config_t int_cfg = {
            .pin_bit_mask = 1ULL << TOUCH_INT_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&int_cfg));

        gpio_set_level(TOUCH_RST_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(TOUCH_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));

        uint8_t max_touches = 0;
        if (ReadReg(kMaxTouches, &max_touches, 1) == ESP_OK && max_touches > 0 && max_touches <= kMaxTouchPoints) {
            max_points_ = max_touches;
        } else {
            max_points_ = 1;
        }
        ESP_LOGI(TAG, "Touch max points: %u", max_points_);
    }

    bool ReadFirstPoint(uint16_t& x, uint16_t& y) {
        uint8_t touch_info = 0;
        if (ReadReg(kTouchInfo, &touch_info, 1) != ESP_OK || !(touch_info & 0x08)) {
            return false;
        }

        uint8_t point_data[7 * kMaxTouchPoints] = {};
        if (ReadReg(kTouchPoint0, point_data, 7 * max_points_) != ESP_OK) {
            return false;
        }

        for (uint8_t i = 0; i < max_points_; i++) {
            const uint8_t base = i * 7;
            if (!(point_data[base] & 0x80)) {
                continue;
            }

            const uint16_t raw_x = ((point_data[base] & 0x3F) << 8) | point_data[base + 1];
            const uint16_t raw_y = ((point_data[base + 2] & 0x3F) << 8) | point_data[base + 3];

            x = raw_y;
            y = DISPLAY_HEIGHT - raw_x;
            if (x >= DISPLAY_WIDTH) {
                x = DISPLAY_WIDTH - 1;
            }
            if (y >= DISPLAY_HEIGHT) {
                y = DISPLAY_HEIGHT - 1;
            }
            return true;
        }
        return false;
    }

private:
    static constexpr uint8_t kMaxTouchPoints = 10;
    static constexpr uint16_t kMaxTouches = 0x0009;
    static constexpr uint16_t kTouchInfo = 0x0010;
    static constexpr uint16_t kTouchPoint0 = 0x0014;

    esp_err_t ReadReg(uint16_t reg, uint8_t* data, size_t len) {
        uint8_t addr[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
        return i2c_master_transmit_receive(dev_, addr, sizeof(addr), data, len, pdMS_TO_TICKS(100));
    }

    i2c_master_dev_handle_t dev_ = nullptr;
    uint8_t max_points_ = 1;
};

class QdtechS3TouchLcd35Board : public WifiBoard {
public:
    QdtechS3TouchLcd35Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        InitializeRadio();
        GetBacklight()->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, true, AUDIO_CODEC_PA_INVERTED);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    void StartNetwork() override {
        WifiBoard::StartNetwork();
        
        // WiFi 连接后启动时间天气服务
        if (!time_weather_started_ && display_) {
            auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
            time_weather_service_.Start(qd_display->GetDesktopUI());
            time_weather_started_ = true;
            ESP_LOGI(TAG, "Time weather service started");
        }
    }

private:
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.data0_io_num = DISPLAY_MOSI_PIN;
        buscfg.data1_io_num = DISPLAY_MISO_PIN;
        buscfg.data2_io_num = DISPLAY_DATA2_PIN;
        buscfg.data3_io_num = DISPLAY_DATA3_PIN;
        buscfg.data4_io_num = GPIO_NUM_NC;
        buscfg.data5_io_num = GPIO_NUM_NC;
        buscfg.data6_io_num = GPIO_NUM_NC;
        buscfg.data7_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * 80 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_QSPI_CONFIG(DISPLAY_CS_PIN, nullptr, nullptr);
        io_config.pclk_hz = DISPLAY_PIXEL_CLOCK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DISPLAY_SPI_HOST, &io_config, &panel_io));

        st77922_vendor_config_t vendor_config = {};
        vendor_config.init_cmds = qdtech_st77922_init_cmds;
        vendor_config.init_cmds_size = sizeof(qdtech_st77922_init_cmds) / sizeof(qdtech_st77922_init_cmds[0]);
        vendor_config.flags.use_qspi_interface = 1;

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));

        display_ = new QdtechLandscapeDisplay(panel_io, panel, {
            .text_font = &font_puhui_16_4,
            .icon_font = &font_awesome_16_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
            .emoji_font = font_emoji_32_init(),
#else
            .emoji_font = font_emoji_64_init(),
#endif
        });
    }

    void InitializeTouch() {
        touch_.Init(i2c_bus_);

        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<QdtechS3TouchLcd35Board*>(arg)->PollTouch();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "qdtech_touch",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touch_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(touch_timer_, TOUCH_POLL_MS * 1000));
    }

    void PollTouch() {
        uint16_t x = 0;
        uint16_t y = 0;
        const bool touched = touch_.ReadFirstPoint(x, y);
        const int64_t now_ms = esp_timer_get_time() / 1000;

        if (touched && !touch_down_) {
            touch_down_ = true;
            touch_start_ms_ = now_ms;
            touch_start_x_ = x;
            touch_start_y_ = y;
            touch_last_x_ = x;
            touch_last_y_ = y;
        } else if (touched && touch_down_) {
            touch_last_x_ = x;
            touch_last_y_ = y;
        } else if (!touched && touch_down_) {
            touch_down_ = false;
            int16_t dx = (int16_t)touch_last_x_ - (int16_t)touch_start_x_;
            int16_t dy = (int16_t)touch_last_y_ - (int16_t)touch_start_y_;
            int64_t duration = now_ms - touch_start_ms_;

            if (duration < TOUCH_TAP_THRESHOLD_MS && abs(dx) < 30 && abs(dy) < 30) {
                HandleTap(touch_last_x_, touch_last_y_);
            } else if (duration < 500) {
                HandleSwipe(dx, dy);
            }
        }
    }

    void HandleTap(uint16_t x, uint16_t y) {
        if (display_ && lvgl_port_lock(0)) {
            static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI()->HandleTap(x, y);
            lvgl_port_unlock();
        }
    }

    void HandleSwipe(int16_t dx, int16_t dy) {
        if (display_ && lvgl_port_lock(0)) {
            static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI()->HandleSwipe(dx, dy);
            lvgl_port_unlock();
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "Reboot the device and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList& properties) {
                ResetWifiConfiguration();
                return true;
            });
        mcp_server.AddTool("self.weather.set_location",
            "Set the weather location and immediately refresh weather. "
            "Use decimal degrees for latitude and longitude, for example Zhongshan: 22.5176, 113.3928.",
            PropertyList({
                Property("city", kPropertyTypeString),
                Property("latitude", kPropertyTypeString),
                Property("longitude", kPropertyTypeString),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto city = properties["city"].value<std::string>();
                const auto latitude = properties["latitude"].value<std::string>();
                const auto longitude = properties["longitude"].value<std::string>();
                if (!time_weather_service_.SetLocation(city, latitude, longitude)) {
                    return std::string("Invalid weather location. Latitude must be -90..90 and longitude must be -180..180.");
                }
                return std::string("Weather location updated to ") + city + " (" + latitude + ", " + longitude + ").";
            });
        mcp_server.AddTool("self.radio.get_status",
            "Get the current network radio status, including station name and playback state.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                return radio_service_.GetStatusJson();
            });
        mcp_server.AddTool("self.radio.play",
            "Start or resume the network radio. Optionally select a station by name first.",
            PropertyList({
                Property("station", kPropertyTypeString, std::string("")),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto station = properties["station"].value<std::string>();
                if (!station.empty()) {
                    radio_service_.SelectStation(station);
                }
                radio_service_.Play();
                return true;
            });
        mcp_server.AddTool("self.radio.stop",
            "Stop network radio playback.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Stop();
                return true;
            });
        mcp_server.AddTool("self.radio.next",
            "Switch to the next network radio station and start playback.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Next();
                return true;
            });
        mcp_server.AddTool("self.radio.previous",
            "Switch to the previous network radio station and start playback.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Prev();
                return true;
            });
        
        // WiFi管理工具
        mcp_server.AddTool("self.wifi.list_saved",
            "List all saved WiFi networks. Returns a JSON array of SSID names.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                auto wifi_list = GetSavedWifiList();
                std::string json = "[";
                for (size_t i = 0; i < wifi_list.size(); i++) {
                    json += "\"" + wifi_list[i] + "\"";
                    if (i < wifi_list.size() - 1) {
                        json += ",";
                    }
                }
                json += "]";
                return json;
            });
        mcp_server.AddTool("self.wifi.switch",
            "Switch to a different WiFi network. The device will disconnect from current WiFi and connect to the new one.",
            PropertyList({
                Property("ssid", kPropertyTypeString),
                Property("password", kPropertyTypeString, std::string("")),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto ssid = properties["ssid"].value<std::string>();
                const auto password = properties["password"].value<std::string>();
                if (ssid.empty()) {
                    return std::string("SSID cannot be empty.");
                }
                bool success = SwitchToWifi(ssid, password);
                if (success) {
                    return std::string("Successfully connected to ") + ssid;
                } else {
                    return std::string("Failed to connect to ") + ssid;
                }
            });
        mcp_server.AddTool("self.wifi.remove",
            "Remove a saved WiFi network by index (0-based).",
            PropertyList({
                Property("index", kPropertyTypeInteger),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int index = properties["index"].value<int>();
                bool success = RemoveSavedWifi(index);
                if (success) {
                    return std::string("WiFi network removed successfully.");
                } else {
                    return std::string("Invalid index. Please provide a valid index.");
                }
            });
        mcp_server.AddTool("self.wifi.set_default",
            "Set a saved WiFi network as default by index (0-based). This network will be tried first when connecting.",
            PropertyList({
                Property("index", kPropertyTypeInteger),
            }), [this](const PropertyList& properties) -> ReturnValue {
                int index = properties["index"].value<int>();
                bool success = SetDefaultWifi(index);
                if (success) {
                    return std::string("Default WiFi network set successfully.");
                } else {
                    return std::string("Invalid index. Please provide a valid index.");
                }
            });
    }

    void InitializeRadio() {
        if (!display_) {
            return;
        }
        auto* desktop_ui = static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
        desktop_ui->SetRadioActions(
            [this]() { radio_service_.PlayPause(); },
            [this]() { radio_service_.Stop(); },
            [this]() { radio_service_.Next(); },
            [this]() { radio_service_.Prev(); });
        radio_service_.Start(desktop_ui);
    }

    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    QdtechTouch touch_;
    esp_timer_handle_t touch_timer_ = nullptr;
    bool touch_down_ = false;
    int64_t touch_start_ms_ = 0;
    uint16_t touch_start_x_ = 0;
    uint16_t touch_start_y_ = 0;
    uint16_t touch_last_x_ = 0;
    uint16_t touch_last_y_ = 0;
    TimeWeatherService time_weather_service_;
    RadioService radio_service_;
    bool time_weather_started_ = false;
};

DECLARE_BOARD(QdtechS3TouchLcd35Board);
