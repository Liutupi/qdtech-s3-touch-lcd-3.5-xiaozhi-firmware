#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "desktop_ui.h"
#include "fc_emulator_service.h"
#include "firmware_update_service.h"
#include "photo_service.h"
#include "podcast_service.h"
#include "qd_esp_mqtt.h"
#include "qd_wifi_config_server.h"
#include "radio_service.h"
#include "settings.h"
#include "time_weather_service.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <cJSON.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st77922.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <nvs.h>
#include <wifi_station.h>

#define TAG "QDtech35"

static constexpr int kLyricUdpPort = 45678;
static constexpr int kMusicCommandHttpPort = 45679;
static constexpr size_t kLyricUdpPacketMax = 2048;
static constexpr size_t kMusicCommandBodyMax = 4096;
static constexpr int64_t kHourglassStartupHoldoffMs = 75000;
static constexpr int64_t kBmi270LowRatePollMs = 250;
static constexpr int64_t kShakeLabSampleIntervalMs = 30;
static constexpr int64_t kShakeLabUiUpdateIntervalMs = 80;
static constexpr int64_t kShakeLabTouchSkipLogIntervalMs = 2000;
static constexpr int64_t kShakeLabI2cErrorLogIntervalMs = 2000;
extern "C" {
extern const uint8_t bmi270_context_config_file[];
extern const int bmi270_context_config_file_size;
}
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
        // Keep FULL mode: LVGL's PARTIAL and DIRECT renderers do not preserve
        // translucent animation composition through this software-rotated
        // panel path, which corrupts the weather/card regions.
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
        const bool is_user = role && strcmp(role, "user") == 0;
        const bool is_assistant = role && strcmp(role, "assistant") == 0;
        if (!is_user && !is_assistant) {
            if (!content || strlen(content) == 0) {
                desktop_ui_.SetXiaozhiState("Standby", "", nullptr);
            }
            return;
        }

        const char* state = "XiaoZhi";
        if (is_user) {
            state = "You";
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

    lv_display_t* GetLvDisplay() {
        return display_;
    }

    bool DrawLandscapeRgb565(int x, int y, int width, int height, const uint16_t* pixels,
                             uint32_t timeout_ms = 20) {
        if (!pixels || width <= 0 || height <= 0 || x < 0 || y < 0 ||
            x + width > DISPLAY_WIDTH || y + height > DISPLAY_HEIGHT) {
            return false;
        }
        if (!lvgl_port_lock(timeout_ms)) {
            static int64_t last_lock_fail_log_us = 0;
            const int64_t now_us = esp_timer_get_time();
            if (now_us - last_lock_fail_log_us > 1000000) {
                last_lock_fail_log_us = now_us;
                ESP_LOGW(TAG, "direct draw skipped: lvgl busy");
            }
            return false;
        }
        DrawLandscapeRgb565Locked(x, y, width, height, pixels);
        lvgl_port_unlock();
        return true;
    }

    bool DrawLandscapeRgb565PreSwapped(int x, int y, int width, int height, const uint16_t* pixels,
                                        uint32_t timeout_ms = 20) {
        if (!pixels || width <= 0 || height <= 0 || x < 0 || y < 0 ||
            x + width > DISPLAY_WIDTH || y + height > DISPLAY_HEIGHT) {
            return false;
        }
        if (!lvgl_port_lock(timeout_ms)) {
            return false;
        }
        DrawLandscapeRgb565LockedPreSwapped(x, y, width, height, pixels);
        lvgl_port_unlock();
        return true;
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
        const int64_t flush_start_us = esp_timer_get_time();
        const int x_start = area->x1;
        const int y_start = area->y1;
        const int width = area->x2 - area->x1 + 1;
        const int height = area->y2 - area->y1 + 1;

        DrawLandscapeRgb565Locked(x_start, y_start, width, height, color_map);

        lv_display_flush_ready(display_);
        const int64_t elapsed_us = esp_timer_get_time() - flush_start_us;
        static int64_t last_perf_log_us = 0;
        if (elapsed_us > 30000 && flush_start_us - last_perf_log_us > 5000000) {
            last_perf_log_us = flush_start_us;
            ESP_LOGI(TAG, "flush area=%dx%d at %d,%d took %d us", width, height, x_start, y_start, static_cast<int>(elapsed_us));
        }
    }

    void DrawLandscapeRgb565Locked(int x_start, int y_start, int width, int height,
                                   const uint16_t* color_map) {
        const int x_end = x_start + width - 1;
        const int y_end = y_start + height - 1;
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
    }

    void DrawLandscapeRgb565LockedPreSwapped(int x_start, int y_start, int width, int height,
                                              const uint16_t* color_map) {
        const int x_end = x_start + width - 1;
        const int y_end = y_start + height - 1;
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
    }

    uint16_t* frame_buffer_ = nullptr;
    uint16_t* transfer_buffer_ = nullptr;
    SemaphoreHandle_t transfer_done_ = nullptr;
};

class QdtechTouch {
public:
    struct TouchPoint {
        uint16_t x;
        uint16_t y;
    };

    void Init(i2c_master_bus_handle_t bus) {
        bus_ = bus;
        initialized_ = false;
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
        initialized_ = true;
    }

    size_t ReadPoints(TouchPoint* points, size_t max_points) {
        if (!points || max_points == 0) {
            return 0;
        }
        if (esp_timer_get_time() < read_backoff_until_us_) {
            return 0;
        }
        uint8_t touch_info = 0;
        if (ReadReg(kTouchInfo, &touch_info, 1) != ESP_OK) {
            return 0;
        }
        // Any successful status read proves the bus/controller recovered.
        // Previously the counter was cleared only after a complete touch-point
        // read, so three unrelated transient errors over many minutes caused a
        // disruptive whole-bus reset even when no finger was down.
        read_failures_ = 0;
        if (!(touch_info & 0x08)) {
            return 0;
        }

        uint8_t point_data[7 * kMaxTouchPoints] = {};
        if (ReadReg(kTouchPoint0, point_data, 7 * max_points_) != ESP_OK) {
            return 0;
        }
        read_failures_ = 0;

        size_t count = 0;
        for (uint8_t i = 0; i < max_points_; i++) {
            if (count >= max_points) {
                break;
            }
            const uint8_t base = i * 7;
            if (!(point_data[base] & 0x80)) {
                continue;
            }

            const uint16_t raw_x = ((point_data[base] & 0x3F) << 8) | point_data[base + 1];
            const uint16_t raw_y = ((point_data[base + 2] & 0x3F) << 8) | point_data[base + 3];

            uint16_t x = raw_y;
            uint16_t y = raw_x >= DISPLAY_HEIGHT ? 0 : (DISPLAY_HEIGHT - 1 - raw_x);
            if (x >= DISPLAY_WIDTH) {
                x = DISPLAY_WIDTH - 1;
            }
            if (y >= DISPLAY_HEIGHT) {
                y = DISPLAY_HEIGHT - 1;
            }
            points[count++] = {
                .x = x,
                .y = y,
            };
        }
        return count;
    }

    bool ReadFirstPoint(uint16_t& x, uint16_t& y) {
        TouchPoint point = {};
        if (ReadPoints(&point, 1) == 0) {
            return false;
        }
        x = point.x;
        y = point.y;
        return true;
    }

private:
    static constexpr uint8_t kMaxTouchPoints = 10;
    static constexpr uint16_t kMaxTouches = 0x0009;
    static constexpr uint16_t kTouchInfo = 0x0010;
    static constexpr uint16_t kTouchPoint0 = 0x0014;

    esp_err_t ReadReg(uint16_t reg, uint8_t* data, size_t len) {
        uint8_t addr[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
        esp_err_t err = i2c_master_transmit_receive(dev_, addr, sizeof(addr), data, len, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            NoteReadFailure();
        }
        return err;
    }

    void NoteReadFailure() {
        if (!initialized_) {
            return;
        }
        ++read_failures_;
        const int64_t now_us = esp_timer_get_time();
        if (last_read_failure_us_ == 0 || now_us - last_read_failure_us_ > 5000000) {
            read_failures_ = 1;
        }
        last_read_failure_us_ = now_us;
        read_backoff_until_us_ = now_us + 750000;
        if (read_failures_ >= 3 && bus_ && now_us - last_bus_reset_us_ > 3000000) {
            last_bus_reset_us_ = now_us;
            ESP_LOGW(TAG, "touch i2c reset after repeated failures");
            i2c_master_bus_reset(bus_);
            read_backoff_until_us_ = now_us + 1000000;
        }
    }

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    uint8_t max_points_ = 1;
    uint8_t read_failures_ = 0;
    bool initialized_ = false;
    int64_t read_backoff_until_us_ = 0;
    int64_t last_bus_reset_us_ = 0;
    int64_t last_read_failure_us_ = 0;
};

class QdtechBatteryMonitor {
public:
    ~QdtechBatteryMonitor() {
        if (timer_) {
            esp_timer_stop(timer_);
            esp_timer_delete(timer_);
        }
        if (cali_handle_) {
            adc_cali_delete_scheme_curve_fitting(cali_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    void Start(DesktopUI* ui) {
        ui_ = ui;
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = BATTERY_ADC_UNIT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "battery adc init failed err=%s", esp_err_to_name(err));
            return;
        }

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = BATTERY_ADC_ATTEN,
            .bitwidth = BATTERY_ADC_BITWIDTH,
        };
        err = adc_oneshot_config_channel(adc_handle_, BATTERY_ADC_CHANNEL, &chan_config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "battery adc channel config failed err=%s", esp_err_to_name(err));
            return;
        }

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = BATTERY_ADC_UNIT,
            .chan = BATTERY_ADC_CHANNEL,
            .atten = BATTERY_ADC_ATTEN,
            .bitwidth = BATTERY_ADC_BITWIDTH,
        };
        err = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle_);
        cali_enabled_ = err == ESP_OK;
        if (!cali_enabled_) {
            ESP_LOGW(TAG, "battery adc calibration unavailable err=%s", esp_err_to_name(err));
        }

        ReadAndPublish();
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<QdtechBatteryMonitor*>(arg)->ReadAndPublish();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "qd_battery",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_, 5000000));
    }

    bool GetLevel(int& level, bool& charging, bool& discharging) const {
        if (!valid_) {
            return false;
        }
        level = level_;
        charging = false;
        discharging = true;
        return true;
    }

private:
    static int VoltageToLevel(int mv) {
        const struct {
            int mv;
            int level;
        } points[] = {
            {3300, 0},
            {3600, 20},
            {3700, 40},
            {3800, 60},
            {3950, 80},
            {4200, 100},
        };
        if (mv <= points[0].mv) {
            return 0;
        }
        if (mv >= points[5].mv) {
            return 100;
        }
        for (int i = 0; i < 5; ++i) {
            if (mv >= points[i].mv && mv < points[i + 1].mv) {
                const float ratio = static_cast<float>(mv - points[i].mv) /
                    static_cast<float>(points[i + 1].mv - points[i].mv);
                return points[i].level + static_cast<int>(ratio * (points[i + 1].level - points[i].level));
            }
        }
        return 0;
    }

    void ReadAndPublish() {
        if (!adc_handle_) {
            return;
        }
        int raw_sum = 0;
        int mv_sum = 0;
        int samples = 0;
        for (int i = 0; i < 8; ++i) {
            int raw = 0;
            if (adc_oneshot_read(adc_handle_, BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
                continue;
            }
            int mv = 0;
            if (cali_enabled_) {
                if (adc_cali_raw_to_voltage(cali_handle_, raw, &mv) != ESP_OK) {
                    continue;
                }
            } else {
                mv = raw * 3100 / 4095;
            }
            raw_sum += raw;
            mv_sum += mv;
            ++samples;        }
        if (samples <= 0) {
            return;
        }
        const int raw_avg = raw_sum / samples;
        const int adc_mv = mv_sum / samples;
        voltage_mv_ = adc_mv * BATTERY_VOLTAGE_DIVIDER;
        level_ = VoltageToLevel(voltage_mv_);
        valid_ = true;
        ESP_LOGI(TAG, "battery adc raw=%d adc_mv=%d battery_mv=%d level=%d%% gpio=%d cal=%d",
                 raw_avg, adc_mv, voltage_mv_, level_, BATTERY_ADC_GPIO, cali_enabled_);

        if (ui_ && lvgl_port_lock(0)) {
            ui_->SetBatteryStatus(level_, false, true);
            lvgl_port_unlock();
        }
    }

    DesktopUI* ui_ = nullptr;
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t cali_handle_ = nullptr;
    esp_timer_handle_t timer_ = nullptr;
    bool cali_enabled_ = false;
    bool valid_ = false;
    int voltage_mv_ = 0;
    int level_ = 0;
};

class QdtechS3TouchLcd35Board : public WifiBoard {
public:
    QdtechS3TouchLcd35Board() : boot_button_(BOOT_BUTTON_GPIO, false, 2000, 0) {
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTouch();
        InitializeBmi270();
        InitializeButtons();
        InitializeTools();
        InitializeRadio();
        InitializePodcast();
        InitializePhotos();
        InitializeFcEmulator();
        InitializeFirmwareUpdate();
        InitializeBattery();
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

    Mqtt* CreateMqtt() override {
        return new QdEspMqtt();
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        return battery_monitor_.GetLevel(level, charging, discharging);
    }

    int GetStartupWifiConnectTimeoutMs() const override {
#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_FAST_PROVISIONING_FALLBACK)
        return 8 * 1000;
#else
        return WifiBoard::GetStartupWifiConnectTimeoutMs();
#endif
    }

    void StartNetwork() override {
#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_FAST_PROVISIONING_FALLBACK)
        ESP_LOGI(TAG, "QDTech fast provisioning fallback enabled timeout=%d ms",
                 GetStartupWifiConnectTimeoutMs());
#endif
        EnsureStrongestBssidDefault();
        WifiBoard::StartNetwork();
        const bool open_phone_web_after_setup = ConsumePhoneWebAfterSetupFlag();
        ESP_LOGI(TAG, "Lyric UDP kept off by default; phone web config starts automatically");
        
        // WiFi 连接后启动时间天气服�?
        if (!time_weather_started_ && display_) {
            auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
            time_weather_service_.Start(qd_display->GetDesktopUI());
            time_weather_started_ = true;
            ESP_LOGI(TAG, "Time weather service started");
        }

        InitializeWifiConfigServer();
        if (wifi_config_server_.IsRunning()) {
            network_services_started_ = true;
            ESP_LOGI(TAG, "Phone web config available: %s",
                     wifi_config_server_.Status().c_str());
        } else {
            ScheduleDeferredNetworkServices(30000000);
            ESP_LOGW(TAG, "Phone web config start deferred: %s",
                     wifi_config_server_.Status().c_str());
        }
        if (open_phone_web_after_setup) {
            StartPhoneWebFromSettings();
        }
    }

private:
    struct ScheduledLyricLine {
        int time_ms = 0;
        std::string text;
    };

    struct LyricTaskArgs {
        QdtechS3TouchLcd35Board* board = nullptr;
        int generation = 0;
        std::string title;
        std::string artist;
        std::vector<ScheduledLyricLine> lines;
    };

    void SetCurrentLyricSong(const std::string& title, const std::string& artist, int scheduled_generation) {
        portENTER_CRITICAL(&lyric_lock_);
        strlcpy(current_lyric_title_, title.c_str(), sizeof(current_lyric_title_));
        strlcpy(current_lyric_artist_, artist.c_str(), sizeof(current_lyric_artist_));
        scheduled_lyric_generation_ = scheduled_generation;
        portEXIT_CRITICAL(&lyric_lock_);
    }

    bool IsCurrentLyricSong(const std::string& title, const std::string& artist) {
        portENTER_CRITICAL(&lyric_lock_);
        const bool title_matches = title.empty() || current_lyric_title_[0] == '\0' ||
            strncmp(title.c_str(), current_lyric_title_, sizeof(current_lyric_title_)) == 0;
        const bool artist_matches = artist.empty() || current_lyric_artist_[0] == '\0' ||
            strncmp(artist.c_str(), current_lyric_artist_, sizeof(current_lyric_artist_)) == 0;
        portEXIT_CRITICAL(&lyric_lock_);
        return title_matches && artist_matches;
    }

    bool ShouldIgnoreExternalLyric(const std::string& title, const std::string& artist) {
        portENTER_CRITICAL(&lyric_lock_);
        const bool stale_title = !title.empty() && current_lyric_title_[0] != '\0' &&
            strncmp(title.c_str(), current_lyric_title_, sizeof(current_lyric_title_)) != 0;
        const bool stale_artist = !artist.empty() && current_lyric_artist_[0] != '\0' &&
            strncmp(artist.c_str(), current_lyric_artist_, sizeof(current_lyric_artist_)) != 0;
        const bool duplicate_scheduled =
            scheduled_lyric_generation_ != 0 && scheduled_lyric_generation_ == lyric_generation_.load();
        portEXIT_CRITICAL(&lyric_lock_);
        return stale_title || stale_artist || duplicate_scheduled;
    }

    void ClearScheduledLyricsIfCurrent(int generation) {
        portENTER_CRITICAL(&lyric_lock_);
        if (scheduled_lyric_generation_ == generation) {
            scheduled_lyric_generation_ = 0;
        }
        portEXIT_CRITICAL(&lyric_lock_);
    }

    static cJSON* GetFirstObjectItem(cJSON* object, const char* const* keys, size_t key_count) {
        if (!cJSON_IsObject(object)) {
            return nullptr;
        }
        for (size_t i = 0; i < key_count; ++i) {
            cJSON* item = cJSON_GetObjectItemCaseSensitive(object, keys[i]);
            if (item) {
                return item;
            }
        }
        return nullptr;
    }

    static int ParseLyricTimeStringMs(const char* text) {
        if (!text || !text[0]) {
            return 0;
        }
        int minutes = 0;
        int seconds = 0;
        int fraction = 0;
        int fraction_digits = 0;
        const char* p = text;

        while (*p && std::isdigit(static_cast<unsigned char>(*p))) {
            minutes = minutes * 10 + (*p - '0');
            ++p;
        }
        if (*p != ':') {
            return std::max(0, atoi(text));
        }
        ++p;
        while (*p && std::isdigit(static_cast<unsigned char>(*p))) {
            seconds = seconds * 10 + (*p - '0');
            ++p;
        }
        if (*p == '.' || *p == ',') {
            ++p;
            while (*p && std::isdigit(static_cast<unsigned char>(*p)) && fraction_digits < 3) {
                fraction = fraction * 10 + (*p - '0');
                ++fraction_digits;
                ++p;
            }
        }
        while (fraction_digits > 0 && fraction_digits < 3) {
            fraction *= 10;
            ++fraction_digits;
        }
        return std::max(0, (minutes * 60 + seconds) * 1000 + fraction);
    }

    static int LyricNumberToMs(double value, bool explicit_ms) {
        if (value < 0) {
            return 0;
        }
        if (!explicit_ms && value > 0 && value < 1000.0) {
            value *= 1000.0;
        }
        return static_cast<int>(value + 0.5);
    }

    static void AppendLyricLineFromJson(cJSON* item, std::vector<ScheduledLyricLine>& lines) {
        if (!item || lines.size() >= 90) {
            return;
        }

        cJSON* time_item = nullptr;
        cJSON* text_item = nullptr;
        bool explicit_ms = false;

        if (cJSON_IsArray(item)) {
            time_item = cJSON_GetArrayItem(item, 0);
            text_item = cJSON_GetArrayItem(item, 1);
            if (!cJSON_IsString(text_item) && cJSON_GetArraySize(item) > 2) {
                text_item = cJSON_GetArrayItem(item, 2);
            }
        } else if (cJSON_IsObject(item)) {
            static const char* const time_ms_keys[] = {"time_ms", "timeMs", "timeMillis", "start_ms", "startMs"};
            static const char* const time_keys[] = {"time", "start", "offset", "timestamp", "startTime", "start_time"};
            static const char* const text_keys[] = {
                "text", "line", "lyric", "content", "words", "word", "originalLyric",
                "original", "sentence", "value"
            };
            time_item = GetFirstObjectItem(item, time_ms_keys, sizeof(time_ms_keys) / sizeof(time_ms_keys[0]));
            explicit_ms = time_item != nullptr;
            if (!time_item) {
                time_item = GetFirstObjectItem(item, time_keys, sizeof(time_keys) / sizeof(time_keys[0]));
            }
            text_item = GetFirstObjectItem(item, text_keys, sizeof(text_keys) / sizeof(text_keys[0]));
        }

        if (!cJSON_IsString(text_item) || text_item->valuestring[0] == '\0') {
            return;
        }

        ScheduledLyricLine line;
        if (cJSON_IsNumber(time_item)) {
            line.time_ms = LyricNumberToMs(time_item->valuedouble, explicit_ms);
        } else if (cJSON_IsString(time_item)) {
            line.time_ms = ParseLyricTimeStringMs(time_item->valuestring);
        }
        line.text = text_item->valuestring;
        lines.push_back(line);
    }

    static void AppendLyricsRecursive(cJSON* node, std::vector<ScheduledLyricLine>& lines, int depth) {
        if (!node || lines.size() >= 90 || depth > 5) {
            return;
        }

        if (cJSON_IsString(node)) {
            AppendLrcText(node->valuestring, lines);
            return;
        }

        const size_t before = lines.size();
        AppendLyricLineFromJson(node, lines);
        if (lines.size() > before) {
            return;
        }

        if (cJSON_IsArray(node)) {
            const int count = cJSON_GetArraySize(node);
            for (int i = 0; i < count && lines.size() < 90; ++i) {
                AppendLyricsRecursive(cJSON_GetArrayItem(node, i), lines, depth + 1);
            }
            return;
        }

        if (cJSON_IsObject(node)) {
            for (cJSON* child = node->child; child && lines.size() < 90; child = child->next) {
                AppendLyricsRecursive(child, lines, depth + 1);
            }
        }
    }

    static void SortAndDedupLyrics(std::vector<ScheduledLyricLine>& lines) {
        std::sort(lines.begin(), lines.end(), [](const ScheduledLyricLine& a, const ScheduledLyricLine& b) {
            if (a.time_ms != b.time_ms) {
                return a.time_ms < b.time_ms;
            }
            return a.text < b.text;
        });
        lines.erase(std::unique(lines.begin(), lines.end(), [](const ScheduledLyricLine& a, const ScheduledLyricLine& b) {
            return a.time_ms == b.time_ms && a.text == b.text;
        }), lines.end());
    }

    static void AppendLrcText(const char* lrc, std::vector<ScheduledLyricLine>& lines) {
        if (!lrc) {
            return;
        }
        const char* p = lrc;
        while (*p && lines.size() < 90) {
            const char* line_end = strpbrk(p, "\r\n");
            const char* next = line_end ? line_end : p + strlen(p);
            const char* text_start = p;
            int last_time_ms = -1;

            while (text_start < next && *text_start == '[') {
                const char* close = static_cast<const char*>(memchr(text_start, ']', next - text_start));
                if (!close) {
                    break;
                }
                std::string tag(text_start + 1, close);
                if (tag.find(':') != std::string::npos && std::isdigit(static_cast<unsigned char>(tag[0]))) {
                    last_time_ms = ParseLyricTimeStringMs(tag.c_str());
                }
                text_start = close + 1;
            }

            while (text_start < next && (*text_start == ' ' || *text_start == '\t')) {
                ++text_start;
            }
            if (last_time_ms >= 0 && text_start < next) {
                ScheduledLyricLine line;
                line.time_ms = last_time_ms;
                line.text.assign(text_start, next);
                if (!line.text.empty()) {
                    lines.push_back(std::move(line));
                }
            }

            p = next;
            while (*p == '\r' || *p == '\n') {
                ++p;
            }
        }
    }

    // MCP song providers sometimes return a JSON-shaped string containing
    // literal "\\n" escapes or unescaped newlines.  cJSON rejects the latter
    // and the regular LRC parser cannot see timestamps hidden behind the JSON
    // prefix.  Recover the embedded LRC without requiring another network
    // request.
    static void AppendLooseLrcText(const std::string& input,
                                   std::vector<ScheduledLyricLine>& lines) {
        std::string normalized;
        normalized.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\\' && i + 1 < input.size()) {
                const char next = input[i + 1];
                if (next == 'n' || next == 'r') {
                    normalized.push_back('\n');
                    ++i;
                    continue;
                }
                if (next == '\\' || next == '"' || next == '/') {
                    normalized.push_back(next);
                    ++i;
                    continue;
                }
            }
            normalized.push_back(input[i]);
        }

        size_t lrc_start = std::string::npos;
        for (size_t i = 0; i + 3 < normalized.size(); ++i) {
            if (normalized[i] == '[' &&
                std::isdigit(static_cast<unsigned char>(normalized[i + 1])) &&
                normalized.find(':', i + 2) != std::string::npos) {
                lrc_start = i;
                break;
            }
        }
        if (lrc_start != std::string::npos) {
            AppendLrcText(normalized.c_str() + lrc_start, lines);
        } else {
            AppendLrcText(normalized.c_str(), lines);
        }
    }

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
        i2c_mutex_ = xSemaphoreCreateMutex();
        ESP_ERROR_CHECK(i2c_mutex_ ? ESP_OK : ESP_ERR_NO_MEM);
    }

    bool LockSharedI2c(TickType_t timeout = pdMS_TO_TICKS(200)) {
        if (i2c_mutex_) {
            return xSemaphoreTake(i2c_mutex_, timeout) == pdTRUE;
        }
        return true;
    }

    void UnlockSharedI2c() {
        if (i2c_mutex_) {
            xSemaphoreGive(i2c_mutex_);
        }
    }

    bool Bmi270ReadReg(uint8_t reg, uint8_t* data, size_t len) {
        if (!bmi270_dev_ || !data || len == 0) {
            return false;
        }
        // Touch polling starts just before BMI270 probing and may briefly own
        // the shared bus.  A zero-timeout lock made that harmless overlap look
        // like a missing sensor during boot.  Twenty milliseconds remains
        // short enough for motion sampling while allowing one touch transfer
        // to finish.
        if (!LockSharedI2c(pdMS_TO_TICKS(20))) {
            return false;
        }
        esp_err_t err = i2c_master_transmit_receive(bmi270_dev_, &reg, 1, data, len, pdMS_TO_TICKS(100));
        UnlockSharedI2c();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "BMI270 read reg=0x%02x failed: %s", reg, esp_err_to_name(err));
            return false;
        }
        return true;
    }

    bool Bmi270WriteReg(uint8_t reg, uint8_t value) {
        if (!bmi270_dev_) {
            return false;
        }
        const uint8_t data[] = {reg, value};
        if (!LockSharedI2c()) {
            return false;
        }
        esp_err_t err = i2c_master_transmit(bmi270_dev_, data, sizeof(data), pdMS_TO_TICKS(100));
        UnlockSharedI2c();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "BMI270 write reg=0x%02x failed: %s", reg, esp_err_to_name(err));
            return false;
        }
        return true;
    }

    bool Bmi270WriteData(uint8_t reg, const uint8_t* data, size_t len) {
        if (!bmi270_dev_ || !data || len == 0) {
            return false;
        }
        std::vector<uint8_t> buffer(len + 1);
        buffer[0] = reg;
        std::memcpy(buffer.data() + 1, data, len);
        if (!LockSharedI2c(pdMS_TO_TICKS(1200))) {
            return false;
        }
        esp_err_t err = i2c_master_transmit(bmi270_dev_, buffer.data(), buffer.size(), pdMS_TO_TICKS(1000));
        UnlockSharedI2c();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "BMI270 write data reg=0x%02x len=%u failed: %s",
                     reg, static_cast<unsigned>(len), esp_err_to_name(err));
            return false;
        }
        return true;
    }

    bool ConfigureBmi270() {
        Bmi270WriteReg(0x7E, 0xB6);
        vTaskDelay(pdMS_TO_TICKS(50));

        uint8_t chip_id = 0;
        if (!Bmi270ReadReg(0x00, &chip_id, 1) || chip_id != 0x24) {
            ESP_LOGW(TAG, "BMI270 vanished after reset chip_id=0x%02x", chip_id);
            return false;
        }

        Bmi270WriteReg(0x7C, 0x00);
        vTaskDelay(pdMS_TO_TICKS(10));
        Bmi270WriteReg(0x59, 0x00);
        if (!Bmi270WriteData(0x5E, bmi270_context_config_file,
                             static_cast<size_t>(bmi270_context_config_file_size))) {
            return false;
        }
        Bmi270WriteReg(0x59, 0x01);
        vTaskDelay(pdMS_TO_TICKS(50));

        uint8_t internal_status = 0;
        if (!Bmi270ReadReg(0x21, &internal_status, 1)) {
            return false;
        }
        ESP_LOGI(TAG, "BMI270 init status=0x%02x", internal_status);
        if ((internal_status & 0x0F) != 0x01) {
            ESP_LOGW(TAG, "BMI270 config load not ready status=0x%02x", internal_status);
            return false;
        }

        Bmi270WriteReg(0x7D, 0x0E);
        Bmi270WriteReg(0x40, 0xA6);
        Bmi270WriteReg(0x41, 0x03);
        Bmi270WriteReg(0x42, 0xA6);
        Bmi270WriteReg(0x43, 0x00);
        Bmi270WriteReg(0x7C, 0x02);
        vTaskDelay(pdMS_TO_TICKS(50));
        return true;
    }

    bool Bmi270ReadAccel(int16_t& x, int16_t& y, int16_t& z) {
        uint8_t data[6] = {};
        if (!Bmi270ReadReg(0x0C, data, sizeof(data))) {
            return false;
        }
        x = static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) | data[0]);
        y = static_cast<int16_t>((static_cast<uint16_t>(data[3]) << 8) | data[2]);
        z = static_cast<int16_t>((static_cast<uint16_t>(data[5]) << 8) | data[4]);
        return true;
    }

    bool Bmi270ReadSample(int16_t& accel_x, int16_t& accel_y, int16_t& accel_z,
                          int16_t& gyro_x, int16_t& gyro_y, int16_t& gyro_z) {
        if (!bmi270_dev_) {
            return false;
        }
        constexpr uint8_t kAccelStartRegister = 0x0C;
        uint8_t data[12] = {};
        if (!LockSharedI2c(0)) {
            return false;
        }
        const esp_err_t err = i2c_master_transmit_receive(bmi270_dev_, &kAccelStartRegister, 1, data,
                                                           sizeof(data), pdMS_TO_TICKS(100));
        UnlockSharedI2c();
        if (err != ESP_OK) {
            return false;
        }
        accel_x = static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) | data[0]);
        accel_y = static_cast<int16_t>((static_cast<uint16_t>(data[3]) << 8) | data[2]);
        accel_z = static_cast<int16_t>((static_cast<uint16_t>(data[5]) << 8) | data[4]);
        gyro_x = static_cast<int16_t>((static_cast<uint16_t>(data[7]) << 8) | data[6]);
        gyro_y = static_cast<int16_t>((static_cast<uint16_t>(data[9]) << 8) | data[8]);
        gyro_z = static_cast<int16_t>((static_cast<uint16_t>(data[11]) << 8) | data[10]);
        return true;
    }
    bool Bmi270ReadGyro(int16_t& x, int16_t& y, int16_t& z) {
        uint8_t data[6] = {};
        if (!Bmi270ReadReg(0x12, data, sizeof(data))) {
            return false;
        }
        x = static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) | data[0]);
        y = static_cast<int16_t>((static_cast<uint16_t>(data[3]) << 8) | data[2]);
        z = static_cast<int16_t>((static_cast<uint16_t>(data[5]) << 8) | data[4]);
        return true;
    }

    bool ProbeBmi270Address(uint8_t address) {
        LockSharedI2c();
        esp_err_t probe_err = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(100));
        UnlockSharedI2c();
        if (probe_err != ESP_OK) {
            return false;
        }
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 400000,
            .scl_wait_us = 0,
            .flags = {},
        };
        esp_err_t err = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &bmi270_dev_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "BMI270 add device addr=0x%02x failed: %s", address, esp_err_to_name(err));
            return false;
        }

        uint8_t chip_id = 0;
        if (!Bmi270ReadReg(0x00, &chip_id, 1)) {
            i2c_master_bus_rm_device(bmi270_dev_);
            bmi270_dev_ = nullptr;
            return false;
        }
        ESP_LOGI(TAG, "BMI270 probe addr=0x%02x chip_id=0x%02x", address, chip_id);
        if (chip_id != 0x24) {
            i2c_master_bus_rm_device(bmi270_dev_);
            bmi270_dev_ = nullptr;
            return false;
        }

        bmi270_addr_ = address;
        if (!ConfigureBmi270()) {
            i2c_master_bus_rm_device(bmi270_dev_);
            bmi270_dev_ = nullptr;
            return false;
        }
        bmi270_present_ = true;
        ESP_LOGI(TAG, "BMI270 detected addr=0x%02x; hourglass rotation enabled", bmi270_addr_);
        return true;
    }

    void InitializeBmi270() {
        if (display_) {
            auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
            qd_display->GetDesktopUI()->SetShakeLabSamplingCallback([this](bool active) {
                shake_lab_sampling_active_.store(active, std::memory_order_relaxed);
            });
        }
        if (!i2c_bus_) {
            return;
        }
        if (!ProbeBmi270Address(0x68) && !ProbeBmi270Address(0x69)) {
            ESP_LOGW(TAG, "BMI270 not detected on I2C addr 0x68/0x69");
            return;
        }
        BaseType_t ok = xTaskCreate(
            [](void* arg) { static_cast<QdtechS3TouchLcd35Board*>(arg)->Bmi270Task(); },
            "bmi270", 4096, this, 1, &bmi270_task_);
        if (ok != pdPASS) {
            ESP_LOGW(TAG, "BMI270 task create failed");
        }
    }

    bool EnterHourglassFromBmi270() {
        if (!display_) {
            return false;
        }
        auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
        if (lvgl_port_lock(pdMS_TO_TICKS(200))) {
            if (auto* desktop_ui = qd_display->GetDesktopUI()) {
                desktop_ui->EnterHourglassMode();
                lvgl_port_unlock();
                return true;
            }
            lvgl_port_unlock();
        }
        ESP_LOGW(TAG, "BMI270 hourglass enter deferred, LVGL busy");
        return false;
    }

    bool PublishShakeLabUpdate(const ShakeDetector::Result& result) {
        if (!display_) {
            return false;
        }
        auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
        if (!lvgl_port_lock(0)) {
            return false;
        }
        qd_display->GetDesktopUI()->UpdateShakeLabDetector(result);
        lvgl_port_unlock();
        return true;
    }
    bool ExitHourglassFromBmi270() {
        if (!display_) {
            return false;
        }
        auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
        if (lvgl_port_lock(pdMS_TO_TICKS(200))) {
            if (auto* desktop_ui = qd_display->GetDesktopUI()) {
                desktop_ui->ExitHourglassMode();
                lvgl_port_unlock();
                return true;
            }
            lvgl_port_unlock();
        }
        ESP_LOGW(TAG, "BMI270 hourglass exit deferred, LVGL busy");
        return false;
    }

    void Bmi270Task() {
        int stable_baseline_count = 0;
        int stable_portrait_count = 0;
        int stable_landscape_count = 0;
        int64_t last_log_ms = 0;
        int64_t last_shake_touch_skip_log_ms = 0;
        int64_t last_shake_i2c_error_log_ms = 0;
        int64_t last_shake_ui_update_ms = 0;
        bool hourglass_active = false;
        bool shake_sampling_was_active = false;

        while (true) {
            const int64_t loop_ms = esp_timer_get_time() / 1000;
            const bool shake_sampling_active = shake_lab_sampling_active_.load(std::memory_order_relaxed);
            if (shake_sampling_active) {
                if (!shake_sampling_was_active) {
                    shake_detector_.Arm(loop_ms);
                    shake_sampling_was_active = true;
                    last_shake_ui_update_ms = 0;
                    ESP_LOGI(TAG, "Shake Lab high-rate BMI270 sampling enabled interval=%dms",
                             static_cast<int>(kShakeLabSampleIntervalMs));
                }
                if (loop_ms < touch_active_until_ms_.load(std::memory_order_relaxed)) {
                    if (loop_ms - last_shake_touch_skip_log_ms >= kShakeLabTouchSkipLogIntervalMs) {
                        ESP_LOGI(TAG, "Shake Lab BMI270 read skipped: touch active");
                        last_shake_touch_skip_log_ms = loop_ms;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }
                int16_t x = 0;
                int16_t y = 0;
                int16_t z = 0;
                int16_t gx = 0;
                int16_t gy = 0;
                int16_t gz = 0;
                if (!Bmi270ReadSample(x, y, z, gx, gy, gz)) {
                    if (loop_ms - last_shake_i2c_error_log_ms >= kShakeLabI2cErrorLogIntervalMs) {
                        ESP_LOGW(TAG, "Shake Lab BMI270 I2C read failed");
                        last_shake_i2c_error_log_ms = loop_ms;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }
                const auto result = shake_detector_.Process({x, y, z, gx, gy, gz, loop_ms});
                if (result.transition == ShakeDetector::Transition::ARMED_TO_SHAKING) {
                    ESP_LOGI(TAG, "Shake Lab ARMED -> SHAKING peaks=%u reversals=%u accel=%u gyro=%u",
                             result.stats.peak_count, result.stats.direction_reversals,
                             result.stats.max_accel_deviation, result.stats.max_gyro_peak);
                } else if (result.transition == ShakeDetector::Transition::SHAKING_TO_SETTLING) {
                    ESP_LOGI(TAG, "Shake Lab SHAKING -> SETTLING duration=%lums peaks=%u reversals=%u accel=%u gyro=%u",
                             static_cast<unsigned long>(result.stats.shaking_duration_ms), result.stats.peak_count,
                             result.stats.direction_reversals, result.stats.max_accel_deviation,
                             result.stats.max_gyro_peak);
                } else if (result.transition == ShakeDetector::Transition::SETTLING_TO_REVEAL) {
                    ESP_LOGI(TAG, "Shake Lab SETTLING -> REVEAL duration=%lums peaks=%u reversals=%u",
                             static_cast<unsigned long>(result.stats.shaking_duration_ms), result.stats.peak_count,
                             result.stats.direction_reversals);
                } else if (result.transition == ShakeDetector::Transition::COOLDOWN_COMPLETE) {
                    ESP_LOGI(TAG, "Shake Lab cooldown complete");
                }
                if (result.transition != ShakeDetector::Transition::NONE ||
                    loop_ms - last_shake_ui_update_ms >= kShakeLabUiUpdateIntervalMs) {
                    if (PublishShakeLabUpdate(result)) {
                        last_shake_ui_update_ms = loop_ms;
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(kShakeLabSampleIntervalMs));
                continue;
            }
            if (shake_sampling_was_active) {
                shake_detector_.Reset();
                shake_sampling_was_active = false;
                ESP_LOGI(TAG, "Shake Lab high-rate BMI270 sampling disabled; restoring low-rate orientation polling");
            }
            if (loop_ms < touch_active_until_ms_.load(std::memory_order_relaxed)) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            int16_t x = 0;
            int16_t y = 0;
            int16_t z = 0;
            int16_t gx = 0;
            int16_t gy = 0;
            int16_t gz = 0;
            if (!Bmi270ReadSample(x, y, z, gx, gy, gz)) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            const int64_t now_ms = esp_timer_get_time() / 1000;
            const int64_t mag_sq = static_cast<int64_t>(x) * x + static_cast<int64_t>(y) * y + static_cast<int64_t>(z) * z;
            const int gyro_peak = std::max({std::abs(static_cast<int>(gx)), std::abs(static_cast<int>(gy)), std::abs(static_cast<int>(gz))});

            if (now_ms - last_log_ms > 3000) {
                ESP_LOGI(TAG, "BMI270 hourglass x=%d y=%d z=%d gyro=%d active=%d",
                         x, y, z, gyro_peak, hourglass_active);
                last_log_ms = now_ms;
            }

            if (bmi270_baseline_mag_sq_ == 0) {
                const bool landscape_candidate = mag_sq > 1000000 && gyro_peak < 45 &&
                    std::abs(static_cast<int>(x)) > 1500 && std::abs(static_cast<int>(y)) < 900;
                if (landscape_candidate) {
                    stable_baseline_count++;
                    if (stable_baseline_count >= 5) {
                        bmi270_baseline_x_ = x;
                        bmi270_baseline_y_ = y;
                        bmi270_baseline_z_ = z;
                        bmi270_baseline_mag_sq_ = mag_sq;
                        ESP_LOGI(TAG, "BMI270 landscape baseline x=%d y=%d z=%d mag=%d",
                                 x, y, z, static_cast<int>(mag_sq));
                    }
                } else {
                    stable_baseline_count = 0;
                }
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }

            const int64_t dot = static_cast<int64_t>(x) * bmi270_baseline_x_ +
                                static_cast<int64_t>(y) * bmi270_baseline_y_ +
                                static_cast<int64_t>(z) * bmi270_baseline_z_;
            const int64_t xy_dot = static_cast<int64_t>(x) * bmi270_baseline_x_ +
                                   static_cast<int64_t>(y) * bmi270_baseline_y_;
            const int64_t baseline_xy_mag_sq = static_cast<int64_t>(bmi270_baseline_x_) * bmi270_baseline_x_ +
                                               static_cast<int64_t>(bmi270_baseline_y_) * bmi270_baseline_y_;
            const int64_t current_xy_mag_sq = static_cast<int64_t>(x) * x + static_cast<int64_t>(y) * y;
            const bool good_gravity = mag_sq > 1000000;
            const bool portrait_90 = good_gravity && baseline_xy_mag_sq > 250000 &&
                current_xy_mag_sq > baseline_xy_mag_sq / 3 &&
                std::llabs(xy_dot) < baseline_xy_mag_sq * 35 / 100 &&
                std::llabs(dot) < bmi270_baseline_mag_sq_ * 65 / 100;
            const bool landscape_normal = good_gravity && dot > bmi270_baseline_mag_sq_ * 65 / 100;

            if (!hourglass_active && portrait_90 && gyro_peak < 150) {
                if (now_ms < kHourglassStartupHoldoffMs) {
                    stable_portrait_count = 0;
                    stable_landscape_count = 0;
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }
                stable_portrait_count++;
                stable_landscape_count = 0;
                if (stable_portrait_count >= 5) {
                    ESP_LOGI(TAG, "BMI270 90 portrait stable; enter hourglass");
                    if (EnterHourglassFromBmi270()) {
                        hourglass_active = true;
                        stable_portrait_count = 0;
                    }
                }
            } else if (!hourglass_active) {
                stable_portrait_count = 0;
            }

            if (hourglass_active && landscape_normal && gyro_peak < 100) {
                stable_landscape_count++;
                stable_portrait_count = 0;
                if (stable_landscape_count >= 5) {
                    ESP_LOGI(TAG, "BMI270 landscape stable; exit hourglass");
                    if (ExitHourglassFromBmi270()) {
                        hourglass_active = false;
                        stable_landscape_count = 0;
                    }
                }
            } else if (hourglass_active) {
                stable_landscape_count = 0;
            }

            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }

    void InitializeSdCard() {
        if (fc_emulator_service_.PrepareSdCard()) {
            ESP_LOGI(TAG, "SD card prepared early");
        } else {
            ESP_LOGW(TAG, "SD card early prepare failed; services will retry when opened");
        }
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

        // Create LVGL input device
        if (display_ && lvgl_port_lock(0)) {
            auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
            touch_indev_ = lv_indev_create();
            lv_indev_set_type(touch_indev_, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(touch_indev_, TouchReadCb);
            lv_indev_set_disp(touch_indev_, qd_display->GetLvDisplay());
            lv_indev_set_driver_data(touch_indev_, this);
            lvgl_port_unlock();
            ESP_LOGI(TAG, "LVGL touch input device created");
        }

        // Keep legacy touch polling for gesture detection
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

    DesktopUI* GetDesktopUI() {
        if (!display_) {
            return nullptr;
        }
        return static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
    }

    void ShowMusicLyric(const std::string& title, const std::string& artist, const std::string& line, bool ensure_page = false) {
        ESP_LOGI(TAG, "ShowMusicLyric request title=%s artist=%s line=%s",
                 title.c_str(), artist.c_str(), line.c_str());
        if (!display_) {
            ESP_LOGW(TAG, "ShowMusicLyric skipped: display null");
            return;
        }
        if (!IsCurrentLyricSong(title, artist)) {
            ESP_LOGI(TAG, "ShowMusicLyric skipped stale title=%s artist=%s",
                     title.c_str(), artist.c_str());
            return;
        }
        if (!lvgl_port_lock(250)) {
            ESP_LOGW(TAG, "ShowMusicLyric skipped: lvgl busy title=%s", title.c_str());
            return;
        }
        auto* desktop_ui = GetDesktopUI();
        if (!desktop_ui) {
            lvgl_port_unlock();
            ESP_LOGW(TAG, "ShowMusicLyric skipped: desktop_ui null");
            return;
        }
        desktop_ui->SetMusicLyric(title.c_str(), artist.c_str(), line.c_str());
        lvgl_port_unlock();
        ESP_LOGI(TAG, "ShowMusicLyric applied line=%s", line.c_str());
    }

    bool MusicToolStarted(const std::string& result) {
        return result.rfind("Music URL started", 0) == 0;
    }

    std::string MusicFailureLine(const std::string& result) {
        if (result.find("short preview") != std::string::npos) {
            return "Only a preview source was found. Try another song version.";
        }
        if (result.find("rejected") != std::string::npos ||
            result.find("unavailable") != std::string::npos) {
            return "The music source was rejected or unavailable.";
        }
        return "No playable source was found. Try another song name or version.";
    }
    void ShowMusicPlayFailure(const std::string& title, const std::string& artist, const std::string& result) {
        lyric_generation_.fetch_add(1);
        SetCurrentLyricSong(title, artist, 0);
        ShowMusicLyric(title.empty() ? "Music" : title, artist, MusicFailureLine(result), true);
    }

    std::vector<ScheduledLyricLine> ParseLyricsJson(const std::string& lyrics_json) {
        std::vector<ScheduledLyricLine> lines;
        if (lyrics_json.empty()) {
            return lines;
        }

        cJSON* root = cJSON_Parse(lyrics_json.c_str());
        if (!root) {
            AppendLooseLrcText(lyrics_json, lines);
            ESP_LOGI(TAG, "ParseLyricsJson raw_lrc bytes=%u lines=%u",
                     static_cast<unsigned>(lyrics_json.size()), static_cast<unsigned>(lines.size()));
            return lines;
        }

        cJSON* lyric_source = root;
        if (cJSON_IsString(root)) {
            std::string nested = root->valuestring ? root->valuestring : "";
            cJSON_Delete(root);
            cJSON* nested_root = cJSON_Parse(nested.c_str());
            if (nested_root) {
                root = nested_root;
                lyric_source = root;
            } else {
                AppendLooseLrcText(nested, lines);
                ESP_LOGI(TAG, "ParseLyricsJson nested_lrc bytes=%u lines=%u",
                         static_cast<unsigned>(lyrics_json.size()), static_cast<unsigned>(lines.size()));
                return lines;
            }
        }

        if (cJSON_IsObject(lyric_source)) {
            static const char* const array_keys[] = {"lyrics", "lines", "data", "items"};
            cJSON* wrapped_array = GetFirstObjectItem(lyric_source, array_keys, sizeof(array_keys) / sizeof(array_keys[0]));
            if (cJSON_IsArray(wrapped_array)) {
                lyric_source = wrapped_array;
            } else {
                static const char* const lrc_keys[] = {"lrc", "lyric", "lyrics", "content"};
                cJSON* wrapped_lrc = GetFirstObjectItem(lyric_source, lrc_keys, sizeof(lrc_keys) / sizeof(lrc_keys[0]));
                if (cJSON_IsObject(wrapped_lrc)) {
                    cJSON* nested_lrc = cJSON_GetObjectItem(wrapped_lrc, "lyric");
                    if (!nested_lrc) {
                        nested_lrc = cJSON_GetObjectItem(wrapped_lrc, "content");
                    }
                    wrapped_lrc = nested_lrc;
                }
                if (cJSON_IsString(wrapped_lrc)) {
                    AppendLrcText(wrapped_lrc->valuestring, lines);
                    if (!lines.empty()) {
                        SortAndDedupLyrics(lines);
                        cJSON_Delete(root);
                        ESP_LOGI(TAG, "ParseLyricsJson wrapped_lrc bytes=%u lines=%u",
                                 static_cast<unsigned>(lyrics_json.size()), static_cast<unsigned>(lines.size()));
                        return lines;
                    }
                }
            }
        }

        if (!cJSON_IsArray(lyric_source)) {
            AppendLyricsRecursive(lyric_source, lines, 0);
            if (!lines.empty()) {
                SortAndDedupLyrics(lines);
                cJSON_Delete(root);
                ESP_LOGI(TAG, "ParseLyricsJson recursive bytes=%u lines=%u",
                         static_cast<unsigned>(lyrics_json.size()), static_cast<unsigned>(lines.size()));
                return lines;
            }
            ESP_LOGW(TAG, "ParseLyricsJson unsupported root bytes=%u type=%d sample=%.96s",
                     static_cast<unsigned>(lyrics_json.size()), root ? root->type : -1, lyrics_json.c_str());
            cJSON_Delete(root);
            return lines;
        }

        const int count = cJSON_GetArraySize(lyric_source);
        lines.reserve(std::min(count, 90));
        for (int i = 0; i < count && lines.size() < 90; ++i) {
            AppendLyricLineFromJson(cJSON_GetArrayItem(lyric_source, i), lines);
        }
        if (lines.empty()) {
            AppendLyricsRecursive(lyric_source, lines, 0);
        }
        cJSON_Delete(root);
        SortAndDedupLyrics(lines);
        ESP_LOGI(TAG, "ParseLyricsJson bytes=%u lines=%u",
                 static_cast<unsigned>(lyrics_json.size()), static_cast<unsigned>(lines.size()));
        return lines;
    }

    void StartLyricsFromPlayUrl(const std::string& title, const std::string& artist, const std::string& lyrics_json) {
        const int generation = lyric_generation_.fetch_add(1) + 1;
        auto lines = ParseLyricsJson(lyrics_json);
        const std::string fallback_line = lines.empty() ? ("Playing: " + title) : lines.front().text;
        ESP_LOGI(TAG, "StartLyricsFromPlayUrl title=%s lyrics_json length=%u lines=%u",
                 title.c_str(), static_cast<unsigned>(lyrics_json.size()), static_cast<unsigned>(lines.size()));
        SetCurrentLyricSong(title, artist, lines.empty() ? 0 : generation);
        if (lines.empty()) {
            ESP_LOGI(TAG, "play_url lyrics skipped title=%s", title.c_str());
            ShowMusicLyric(title, artist, "Playing: " + title, true);
            return;
        }

        auto* args = new LyricTaskArgs{
            .board = this,
            .generation = generation,
            .title = title,
            .artist = artist,
            .lines = std::move(lines),
        };
        const int line_count = static_cast<int>(args->lines.size());
        const BaseType_t ret = xTaskCreateWithCaps([](void* arg) {
            auto* task_args = static_cast<LyricTaskArgs*>(arg);
            task_args->board->RunLyricSchedule(task_args);
            delete task_args;
            vTaskDeleteWithCaps(nullptr);
        }, "play_lyrics", 8192, args, 2, nullptr, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ret != pdPASS) {
            ESP_LOGW(TAG, "play_url lyric task create failed ret=%ld free_internal=%u largest_internal=%u",
                     static_cast<long>(ret),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
            SetCurrentLyricSong(title, artist, 0);
            const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (free_internal > 4096) {
                ShowMusicLyric(title, artist, fallback_line, true);
            }
            delete args;
            return;
        }
        ESP_LOGI(TAG, "play_url lyrics started title=%s lines=%d stack=psram", title.c_str(), line_count);
    }

    void RunLyricSchedule(const LyricTaskArgs* args) {
        ShowMusicLyric(args->title, args->artist, "Playing: " + args->title, true);
        const int64_t start_ms = esp_timer_get_time() / 1000;
        std::string last_text;
        for (const auto& line : args->lines) {
            if (args->generation != lyric_generation_.load()) {
                ESP_LOGI(TAG, "play_url lyric task exit generation=%d current=%d",
                         args->generation, lyric_generation_.load());
                return;
            }
            if (line.text.empty() || line.text == last_text) {
                continue;
            }
            last_text = line.text;
            const int64_t now_ms = esp_timer_get_time() / 1000;
            const int64_t target_ms = start_ms + 800 + line.time_ms;
            int64_t wait_ms = target_ms - now_ms;
            // Preserve lyric timing while allowing Stop/Next/new-song to
            // release the task and its strings promptly.
            while (wait_ms > 0) {
                if (args->generation != lyric_generation_.load()) {
                    ESP_LOGI(TAG, "play_url lyric task cancelled while waiting generation=%d current=%d",
                             args->generation, lyric_generation_.load());
                    return;
                }
                const int delay_ms = static_cast<int>(std::min<int64_t>(wait_ms, 200));
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
                wait_ms = target_ms - (esp_timer_get_time() / 1000);
            }
            if (args->generation != lyric_generation_.load()) {
                ESP_LOGI(TAG, "play_url lyric task exit after wait generation=%d current=%d",
                         args->generation, lyric_generation_.load());
                return;
            }
            ESP_LOGI(TAG, "play_url lyric line title=%s line=%s", args->title.c_str(), line.text.c_str());
            ShowMusicLyric(args->title, args->artist, line.text);
        }
        ClearScheduledLyricsIfCurrent(args->generation);
    }

    void StartMusicCommandServer() {
        if (music_command_server_) {
            return;
        }

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = kMusicCommandHttpPort;
        config.ctrl_port = kMusicCommandHttpPort + 1;
        config.max_uri_handlers = 2;
        config.stack_size = 4096;
        config.lru_purge_enable = true;

        esp_err_t err = httpd_start(&music_command_server_, &config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "music command http start failed port=%d err=%s",
                     kMusicCommandHttpPort, esp_err_to_name(err));
            music_command_server_ = nullptr;
            return;
        }

        httpd_uri_t play = {};
        play.uri = "/music/play_url";
        play.method = HTTP_POST;
        play.handler = MusicCommandPlayHandler;
        play.user_ctx = this;
        httpd_register_uri_handler(music_command_server_, &play);

        httpd_uri_t health = {};
        health.uri = "/music/health";
        health.method = HTTP_GET;
        health.handler = MusicCommandHealthHandler;
        health.user_ctx = this;
        httpd_register_uri_handler(music_command_server_, &health);

        ESP_LOGI(TAG, "music command http listening port=%d", kMusicCommandHttpPort);
    }

    static esp_err_t MusicCommandHealthHandler(httpd_req_t* req) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }

    static esp_err_t MusicCommandPlayHandler(httpd_req_t* req) {
        auto* board = static_cast<QdtechS3TouchLcd35Board*>(req->user_ctx);
        if (!board) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No board");
        }
        if (req->content_len == 0 || req->content_len > kMusicCommandBodyMax) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad size");
        }

        std::string body;
        body.resize(req->content_len);
        size_t received = 0;
        while (received < req->content_len) {
            const int ret = httpd_req_recv(req, body.data() + received, req->content_len - received);
            if (ret <= 0) {
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read failed");
            }
            received += ret;
        }

        cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
        if (!root) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        }
        cJSON* url = cJSON_GetObjectItem(root, "url");
        cJSON* title = cJSON_GetObjectItem(root, "title");
        cJSON* artist = cJSON_GetObjectItem(root, "artist");
        if (!cJSON_IsString(url) || strncmp(url->valuestring, "http", 4) != 0) {
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad url");
        }

        const std::string url_text = url->valuestring;
        const std::string title_text = cJSON_IsString(title) ? title->valuestring : "Music";
        const std::string artist_text = cJSON_IsString(artist) ? artist->valuestring : "";
        cJSON_Delete(root);

        ESP_LOGI(TAG, "music command play title=%s artist=%s url=%s",
                 title_text.c_str(), artist_text.c_str(), url_text.c_str());
        const auto result = board->radio_service_.PlayUrlFromTool(title_text, artist_text, url_text);
        if (result.rfind("Music URL started", 0) == 0) {
            board->lyric_generation_.fetch_add(1);
            board->SetCurrentLyricSong(title_text, artist_text, 0);
            board->ShowMusicLyric(title_text, artist_text, "Playing: " + title_text, true);
        }

        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }

    void StartLyricUdpServer() {
        if (lyric_udp_started_) {
            return;
        }
        lyric_udp_started_ = true;
        const BaseType_t ret = xTaskCreateWithCaps([](void* arg) {
            static_cast<QdtechS3TouchLcd35Board*>(arg)->LyricUdpTask();
        }, "lyric_udp", 8192, this, 2, nullptr, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ret != pdPASS) {
            lyric_udp_started_ = false;
            ESP_LOGW(TAG, "lyric udp task create failed");
        }
    }

    void LyricUdpTask() {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGW(TAG, "lyric udp socket failed");
            lyric_udp_started_ = false;
            vTaskDeleteWithCaps(nullptr);
            return;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(kLyricUdpPort);
        if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ESP_LOGW(TAG, "lyric udp bind failed port=%d", kLyricUdpPort);
            close(sock);
            lyric_udp_started_ = false;
            vTaskDeleteWithCaps(nullptr);
            return;
        }

        ESP_LOGI(TAG, "lyric udp listening port=%d", kLyricUdpPort);
        char buffer[kLyricUdpPacketMax + 1];
        while (true) {
            sockaddr_in source = {};
            socklen_t source_len = sizeof(source);
            const int len = recvfrom(sock, buffer, kLyricUdpPacketMax, 0,
                                     reinterpret_cast<sockaddr*>(&source), &source_len);
            if (len <= 0) {
                continue;
            }
            buffer[len] = '\0';
            cJSON* root = cJSON_Parse(buffer);
            if (!root) {
                continue;
            }
            cJSON* url = cJSON_GetObjectItem(root, "url");
            if (cJSON_IsString(url) && strncmp(url->valuestring, "http", 4) == 0) {
                cJSON* title = cJSON_GetObjectItem(root, "title");
                cJSON* artist = cJSON_GetObjectItem(root, "artist");
                const std::string title_text = cJSON_IsString(title) ? title->valuestring : "Music";
                const std::string artist_text = cJSON_IsString(artist) ? artist->valuestring : "";
                const std::string url_text = url->valuestring;
                cJSON_Delete(root);
                ESP_LOGI(TAG, "music udp play title=%s artist=%s url=%s",
                         title_text.c_str(), artist_text.c_str(), url_text.c_str());
                const auto result = radio_service_.PlayUrlFromTool(title_text, artist_text, url_text);
                if (result.rfind("Music URL started", 0) == 0) {
                    lyric_generation_.fetch_add(1);
                    SetCurrentLyricSong(title_text, artist_text, 0);
                    ShowMusicLyric(title_text, artist_text, "Playing: " + title_text, true);
                }
                continue;
            }
            cJSON* line = cJSON_GetObjectItem(root, "line");
            if (!cJSON_IsString(line) || line->valuestring[0] == '\0') {
                cJSON_Delete(root);
                continue;
            }
            cJSON* title = cJSON_GetObjectItem(root, "title");
            cJSON* artist = cJSON_GetObjectItem(root, "artist");
            const std::string title_text = cJSON_IsString(title) ? title->valuestring : "";
            const std::string artist_text = cJSON_IsString(artist) ? artist->valuestring : "";
            const std::string line_text = line->valuestring;
            cJSON_Delete(root);
            ESP_LOGI(TAG, "lyric udp line title=%s line=%s", title_text.c_str(), line_text.c_str());
            if (ShouldIgnoreExternalLyric(title_text, artist_text)) {
                ESP_LOGI(TAG, "lyric udp line ignored title=%s line=%s", title_text.c_str(), line_text.c_str());
                continue;
            }
            ShowMusicLyric(title_text, artist_text, line_text, true);
        }
    }

    static void TouchReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
        auto* board = static_cast<QdtechS3TouchLcd35Board*>(lv_indev_get_driver_data(indev));
        if (!board) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        auto* desktop_ui = board->GetDesktopUI();
        if (desktop_ui && desktop_ui->IsFcPlayingView()) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        if (board->touch_cached_down_.load(std::memory_order_acquire)) {
            data->point.x = board->touch_cached_x_.load(std::memory_order_relaxed);
            data->point.y = board->touch_cached_y_.load(std::memory_order_relaxed);
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }

    void PollTouch() {
        QdtechTouch::TouchPoint points[10] = {};
        // Touch and BMI270 share I2C0.  Serialize the complete touch
        // transaction; concurrent transfers previously left the controller in
        // repeated software timeouts after an otherwise normal tap.
        if (!LockSharedI2c(pdMS_TO_TICKS(5))) {
            return;
        }
        const size_t point_count = touch_.ReadPoints(points, 10);
        UnlockSharedI2c();
        const bool touched = point_count > 0;
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (touched) {
            touch_active_until_ms_.store(now_ms + 800, std::memory_order_relaxed);
        }
        auto* desktop_ui = GetDesktopUI();

        if (touched) {
            touch_cached_x_.store(points[0].x, std::memory_order_relaxed);
            touch_cached_y_.store(points[0].y, std::memory_order_relaxed);
        }
        touch_cached_down_.store(touched, std::memory_order_release);

        if (desktop_ui && desktop_ui->IsFcPlayingView()) {
            uint16_t xs[10] = {};
            uint16_t ys[10] = {};
            for (size_t i = 0; i < point_count; ++i) {
                xs[i] = points[i].x;
                ys[i] = points[i].y;
            }
            desktop_ui->HandleTouchPoints(xs, ys, point_count);
            touch_down_ = false;
            if (touched) {
                touch_last_x_ = points[0].x;
                touch_last_y_ = points[0].y;
            }
            return;
        }

        const uint16_t x = touched ? points[0].x : touch_last_x_;
        const uint16_t y = touched ? points[0].y : touch_last_y_;

        if (touched && !touch_down_) {
            touch_down_ = true;
            touch_start_ms_ = now_ms;
            touch_start_x_ = x;
            touch_start_y_ = y;
            touch_last_x_ = x;
            touch_last_y_ = y;
            ESP_LOGI(TAG, "touch down x=%u y=%u", x, y);
            HandleTouchState(x, y, true);
        } else if (touched && touch_down_) {
            touch_last_x_ = x;
            touch_last_y_ = y;
            HandleTouchState(x, y, true);
        } else if (!touched && touch_down_) {
            touch_down_ = false;
            int16_t dx = (int16_t)touch_last_x_ - (int16_t)touch_start_x_;
            int16_t dy = (int16_t)touch_last_y_ - (int16_t)touch_start_y_;
            int64_t duration = now_ms - touch_start_ms_;

            ESP_LOGI(TAG, "touch release x=%u y=%u dx=%d dy=%d duration=%dms",
                     touch_last_x_, touch_last_y_, dx, dy, static_cast<int>(duration));
            HandleTouchState(touch_last_x_, touch_last_y_, false);
            HandleTouchRelease(touch_start_x_, touch_start_y_, touch_last_x_, touch_last_y_, duration);
        }
    }

    void HandleTouchState(uint16_t x, uint16_t y, bool pressed) {
        if (display_) {
            static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI()->HandleTouchState(x, y, pressed);
        }
    }

    void HandleTouchRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y,
                            int64_t duration_ms) {
        if (display_ && lvgl_port_lock(0)) {
            static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI()->HandleTouchRelease(
                start_x, start_y, end_x, end_y, duration_ms);
            lvgl_port_unlock();
        }
    }

    void InitializeButtons() {
        boot_button_.OnPressDown([]() {
            ESP_LOGI(TAG, "BOOT press down");
        });
        boot_button_.OnPressUp([]() {
            ESP_LOGI(TAG, "BOOT press up");
        });
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                ESP_LOGI(TAG, "BOOT click ignored during startup/config to keep saved WiFi");
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnLongPress([this]() {
            EnterDeepSleep();
        });

        gpio_set_pull_mode(BOOT_BUTTON_GPIO, GPIO_PULLUP_ONLY);
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<QdtechS3TouchLcd35Board*>(arg)->PollBootPowerButton();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "boot_power",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &boot_power_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(boot_power_timer_, 50000));
    }

    void PollBootPowerButton() {
        if (entering_deep_sleep_) {
            return;
        }
        const bool pressed = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (pressed && !boot_power_pressed_) {
            boot_power_pressed_ = true;
            boot_power_long_handled_ = false;
            boot_power_press_start_ms_ = now_ms;
            ESP_LOGI(TAG, "BOOT gpio down");
        } else if (!pressed && boot_power_pressed_) {
            boot_power_pressed_ = false;
            ESP_LOGI(TAG, "BOOT gpio up duration=%dms", static_cast<int>(now_ms - boot_power_press_start_ms_));
        } else if (pressed && !boot_power_long_handled_ && now_ms - boot_power_press_start_ms_ >= 2500) {
            boot_power_long_handled_ = true;
            ESP_LOGI(TAG, "BOOT gpio long press duration=%dms", static_cast<int>(now_ms - boot_power_press_start_ms_));
            EnterDeepSleep();
        }
    }

    void EnterDeepSleep() {
        if (entering_deep_sleep_) {
            return;
        }
        entering_deep_sleep_ = true;
        ESP_LOGI(TAG, "BOOT long press: preparing deep sleep, release BOOT to power off");
        if (boot_power_timer_) {
            esp_timer_stop(boot_power_timer_);
        }
        GetBacklight()->SetBrightness(0, false);

        const int64_t wait_start_ms = esp_timer_get_time() / 1000;
        while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
            if ((esp_timer_get_time() / 1000) - wait_start_ms > 10000) {
                ESP_LOGW(TAG, "BOOT still held after 10s; entering deep sleep anyway may wake immediately");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        vTaskDelay(pdMS_TO_TICKS(250));

        ESP_LOGI(TAG, "BOOT released: entering deep sleep, wake on GPIO%d low", BOOT_BUTTON_GPIO);
        rtc_gpio_pullup_en(BOOT_BUTTON_GPIO);
        rtc_gpio_pulldown_dis(BOOT_BUTTON_GPIO);
        ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(BOOT_BUTTON_GPIO, 0));
        esp_deep_sleep_start();
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "Reboot into WiFi setup. Confirm first.",
            PropertyList(), [this](const PropertyList& properties) {
                ResetWifiConfiguration();
                return true;
            });
        mcp_server.AddTool("self.weather.set_location",
            "Set weather city by latitude and longitude.",
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
        mcp_server.AddTool("self.display.qrcode",
            "Show a QR code on the device screen. Use this for login URLs such as NetEase Music QR login. Pass the raw QR text or URL in content, not a base64 image.",
            PropertyList({
                Property("content", kPropertyTypeString),
                Property("title", kPropertyTypeString, std::string("Scan QR")),
                Property("hint", kPropertyTypeString, std::string("Tap screen to close")),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto content = properties["content"].value<std::string>();
                const auto title = properties["title"].value<std::string>();
                const auto hint = properties["hint"].value<std::string>();
                if (content.empty()) {
                    return std::string("QR code was NOT shown: content is empty.");
                }
                if (content.size() > 700) {
                    return std::string("QR code was NOT shown: content is too long.");
                }
                if (!display_ || !lvgl_port_lock(250)) {
                    return std::string("QR code was NOT shown: display is busy.");
                }
                bool shown = false;
                if (auto* desktop_ui = GetDesktopUI()) {
                    shown = desktop_ui->ShowQrCode(content.c_str(), title.c_str(), hint.c_str());
                }
                lvgl_port_unlock();
                if (!shown) {
                    return std::string("QR code was NOT shown: QR encoder rejected the content.");
                }
                ESP_LOGI(TAG, "self.display.qrcode title=%s content_len=%u",
                         title.c_str(), static_cast<unsigned>(content.size()));
                return std::string("QR code shown on the device screen.");
            });
        mcp_server.AddTool("self.radio.get_status",
            "Get radio status.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                return radio_service_.GetStatusJson();
            });
        mcp_server.AddTool("self.radio.play",
            "Play radio, optionally by station name.",
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
            "Stop radio.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Stop();
                return true;
            });
        mcp_server.AddTool("self.radio.next",
            "Next radio station.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Next();
                return true;
            });
        mcp_server.AddTool("self.radio.previous",
            "Previous radio station.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Prev();
                return true;
            });
        
        // WiFi管理工具
        mcp_server.AddTool("self.music.play_url",
            "Play a FULL direct HTTP MP3 song URL on this device speaker. Do not use preview, trial, ringtone, web page, playlist, or short clip URLs. Prefer URLs with Content-Length over 1 MB and enough duration for a complete song. Calling this again interrupts the current URL or radio stream. If this tool returns Music URL was NOT started, do not say it already found or played the song; either search for a different complete MP3 URL and call this tool again, or tell the user the song source is unavailable due to copyright or an invalid link. After this tool succeeds, do not speak any extra confirmation.",
            PropertyList({
                Property("title", kPropertyTypeString, std::string("Scan QR")),
                Property("artist", kPropertyTypeString, std::string("")),
                Property("url", kPropertyTypeString),
                Property("lyrics_json", kPropertyTypeString, std::string("")),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto title = properties["title"].value<std::string>();
                const auto artist = properties["artist"].value<std::string>();
                const auto url = properties["url"].value<std::string>();
                const auto lyrics_json = properties["lyrics_json"].value<std::string>();
                ESP_LOGI(TAG, "self.music.play_url title=%s artist=%s url_len=%u lyrics_json length=%u",
                         title.c_str(), artist.c_str(), static_cast<unsigned>(url.size()),
                         static_cast<unsigned>(lyrics_json.size()));
                const auto result = radio_service_.PlayUrlFromTool(title, artist, url);
                const bool started = MusicToolStarted(result);
                if (started && display_ && lvgl_port_lock(250)) {
                    if (auto* desktop_ui = GetDesktopUI()) {
                        desktop_ui->RememberMusicTrack(title.c_str(), artist.c_str(), url.c_str(), lyrics_json.c_str());
                    }
                    lvgl_port_unlock();
                }
                if (started) {
                    StartLyricsFromPlayUrl(title, artist, lyrics_json);
                } else {
                    ShowMusicPlayFailure(title, artist, result);
                }
                return result;
            });
        mcp_server.AddTool("self.music.play",
            "Alias for self.music.play_url. Play a FULL direct HTTP MP3 song URL, not a preview or short clip. If this tool returns Music URL was NOT started, retry with a different complete MP3 URL or tell the user the source is unavailable. After this tool succeeds, do not speak any extra confirmation.",
            PropertyList({
                Property("title", kPropertyTypeString, std::string("Scan QR")),
                Property("artist", kPropertyTypeString, std::string("")),
                Property("url", kPropertyTypeString, std::string("")),
                Property("lyrics_json", kPropertyTypeString, std::string("")),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto title = properties["title"].value<std::string>();
                const auto artist = properties["artist"].value<std::string>();
                const auto url = properties["url"].value<std::string>();
                const auto lyrics_json = properties["lyrics_json"].value<std::string>();
                ESP_LOGI(TAG, "self.music.play title=%s artist=%s url_len=%u lyrics_json length=%u",
                         title.c_str(), artist.c_str(), static_cast<unsigned>(url.size()),
                         static_cast<unsigned>(lyrics_json.size()));
                const auto result = radio_service_.PlayUrlFromTool(title, artist, url);
                const bool started = MusicToolStarted(result);
                if (started && display_ && lvgl_port_lock(250)) {
                    if (auto* desktop_ui = GetDesktopUI()) {
                        desktop_ui->RememberMusicTrack(title.c_str(), artist.c_str(), url.c_str(), lyrics_json.c_str());
                    }
                    lvgl_port_unlock();
                }
                if (started) {
                    StartLyricsFromPlayUrl(title, artist, lyrics_json);
                } else {
                    ShowMusicPlayFailure(title, artist, result);
                }
                return result;
            });
        mcp_server.AddTool("self.music.stop",
            "Stop the current music URL playback on this device speaker.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                radio_service_.Stop();
                lyric_generation_.fetch_add(1);
                SetCurrentLyricSong("", "", 0);
                if (display_ && lvgl_port_lock(250)) {
                    if (auto* desktop_ui = GetDesktopUI()) {
                        desktop_ui->ClearMusicLyric();
                    }
                    lvgl_port_unlock();
                }
                return true;
            });
        mcp_server.AddTool("self.music.show_lyric",
            "Show the current music lyric line on the XiaoZhi page without speaking it aloud.",
            PropertyList({
                Property("line", kPropertyTypeString),
                Property("title", kPropertyTypeString, std::string("Scan QR")),
                Property("artist", kPropertyTypeString, std::string("")),
            }), [this](const PropertyList& properties) -> ReturnValue {
                const auto line = properties["line"].value<std::string>();
                const auto title = properties["title"].value<std::string>();
                const auto artist = properties["artist"].value<std::string>();
                ESP_LOGI(TAG, "self.music.show_lyric title=%s artist=%s line=%s",
                         title.c_str(), artist.c_str(), line.c_str());
                if (ShouldIgnoreExternalLyric(title, artist)) {
                    ESP_LOGI(TAG, "self.music.show_lyric ignored title=%s artist=%s line=%s",
                             title.c_str(), artist.c_str(), line.c_str());
                    return true;
                }
                ShowMusicLyric(title, artist, line, true);
                return true;
            });

        mcp_server.AddTool("self.wifi.list_saved",
            "List saved WiFi networks.",
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
            "Switch WiFi network.",
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
            "Remove saved WiFi by index.",
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
            "Set default saved WiFi by index.",
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
        desktop_ui->SetMusicActions(
            [this]() { radio_service_.Play(); },
            [this]() { radio_service_.Pause(); },
            [this]() { radio_service_.Next(); });
        desktop_ui->SetMusicReplayCallback([this](const std::string& title,
                                                  const std::string& artist,
                                                  const std::string& url,
                                                  const std::string& lyrics_json) {
            const auto result = radio_service_.PlayUrlFromTool(title, artist, url);
            const bool started = MusicToolStarted(result);
            if (started) {
                StartLyricsFromPlayUrl(title, artist, lyrics_json);
            } else {
                ShowMusicPlayFailure(title, artist, result);
            }
        });
        radio_service_.SetPlaybackReleasedCallback([this]() {
            lyric_generation_.fetch_add(1);
            SetCurrentLyricSong("", "", 0);
            time_weather_service_.RequestRefresh(true);
            if (display_ && lvgl_port_lock(pdMS_TO_TICKS(100))) {
                if (auto* ui = GetDesktopUI()) {
                    ui->ClearMusicLyric();
                }
                lvgl_port_unlock();
            }
        });
        radio_service_.Start(desktop_ui);
    }

    void InitializePodcast() {
        if (!display_) {
            return;
        }
        auto* desktop_ui = static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
        podcast_service_.Start(desktop_ui);
        desktop_ui->podcast_stop_other_media_ = [this]() {
            radio_service_.Stop();
            fc_emulator_service_.Stop();
        };
    }

    void InitializePhotos() {
        if (!display_) {
            return;
        }
        auto* desktop_ui = static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
        photo_service_.Start(desktop_ui);
    }

    void InitializeFcEmulator() {
        if (!display_) {
            return;
        }
        auto* qd_display = static_cast<QdtechLandscapeDisplay*>(display_);
        auto* desktop_ui = qd_display->GetDesktopUI();
        fc_emulator_service_.SetDirectFrameCallback(
            [this, qd_display](const uint16_t* pixels, uint16_t width, uint16_t height) {
                return DrawFcFrame(qd_display, pixels, width, height);
            });
        fc_emulator_service_.Start(desktop_ui);
        desktop_ui->fc_stop_other_media_ = [this]() {
            radio_service_.Stop();
            podcast_service_.Stop();
        };
        desktop_ui->SetMainPageCallback([this]() {
            time_weather_service_.RequestRefresh(true);
        });
        desktop_ui->SetPhoneWebAction([this]() {
            StartPhoneWebFromSettings();
        });
        desktop_ui->SetFcExitCallback([this]() {
            time_weather_service_.RequestRefresh(true);
        });
    }

    void InitializeBattery() {
        if (!display_) {
            return;
        }
        auto* desktop_ui = static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
        battery_monitor_.Start(desktop_ui);
    }

    void InitializeFirmwareUpdate() {
        if (!display_) {
            return;
        }
        auto* desktop_ui = static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
        FirmwareUpdateService::GetInstance().Start(desktop_ui);
    }




    void InitializeWifiConfigServer() {
        if (!display_) {
            return;
        }
        auto* desktop_ui = static_cast<QdtechLandscapeDisplay*>(display_)->GetDesktopUI();
        wifi_config_server_.Start(desktop_ui, &time_weather_service_);
    }

    bool ConsumePhoneWebAfterSetupFlag() {
        Settings settings("wifi", true);
        if (settings.GetInt("qd_web_once", 0) != 1) {
            return false;
        }
        settings.SetInt("qd_web_once", 0);
        ESP_LOGI(TAG, "Phone web one-shot requested after WiFi setup");
        return true;
    }

    void StartPhoneWebFromSettings() {
        if (!display_) {
            return;
        }
        const int64_t now_us = esp_timer_get_time();
        if (wifi_config_server_.IsRunning()) {
            const std::string status = wifi_config_server_.Status();
            if (now_us - phone_web_last_notice_us_ > 5000000) {
                phone_web_last_notice_us_ = now_us;
                display_->ShowNotification("Phone Web: " + status, 30000);
            }
            ESP_LOGI(TAG, "Phone web already running: %s", status.c_str());
            return;
        }
        if (now_us - phone_web_last_start_us_ < 30000000) {
            if (now_us - phone_web_last_notice_us_ > 5000000) {
                phone_web_last_notice_us_ = now_us;
                display_->ShowNotification("Phone Web waiting: retry soon", 8000);
            }
            ESP_LOGW(TAG, "Phone web start ignored during cooldown");
            return;
        }
        phone_web_last_start_us_ = now_us;
        InitializeWifiConfigServer();
        const std::string status = wifi_config_server_.Status();
        if (wifi_config_server_.IsRunning()) {
            network_services_started_ = true;
            phone_web_last_notice_us_ = now_us;
            display_->ShowNotification("Phone Web: " + status, 30000);
            ESP_LOGI(TAG, "Phone web opened manually: %s", status.c_str());
        } else {
            phone_web_last_notice_us_ = now_us;
            display_->ShowNotification("Phone Web waiting: " + status, 30000);
            ScheduleDeferredNetworkServices(30000000);
            ESP_LOGW(TAG, "Phone web not ready, retry scheduled: %s", status.c_str());
        }
    }

    void EnsureStrongestBssidDefault() {
        nvs_handle_t nvs;
        esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "open wifi NVS failed for BSSID default: %s", esp_err_to_name(err));
            return;
        }

        uint8_t remember_bssid = 0;
        err = nvs_get_u8(nvs, "remember_bssid", &remember_bssid);
        if (err == ESP_ERR_NVS_NOT_FOUND || remember_bssid != 0) {
            err = nvs_set_u8(nvs, "remember_bssid", 0);
            if (err == ESP_OK) {
                err = nvs_commit(nvs);
            }
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Defaulted WiFi BSSID memory off for mesh/AP compatibility");
            } else {
                ESP_LOGW(TAG, "save WiFi BSSID default failed: %s", esp_err_to_name(err));
            }
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "read WiFi BSSID default failed: %s", esp_err_to_name(err));
        }
        nvs_close(nvs);
    }

    void ScheduleDeferredNetworkServices(int64_t delay_us = 180000000) {
        if (network_services_started_) {
            return;
        }
        if (!network_services_timer_) {
            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    static_cast<QdtechS3TouchLcd35Board*>(arg)->StartDeferredNetworkServices();
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "qd_net_services",
                .skip_unhandled_events = true,
            };
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &network_services_timer_));
        }
        if (esp_timer_is_active(network_services_timer_)) {
            ESP_ERROR_CHECK(esp_timer_stop(network_services_timer_));
        }
        ESP_ERROR_CHECK(esp_timer_start_once(network_services_timer_, delay_us));
        ESP_LOGI(TAG, "Deferred phone config services scheduled in %d sec",
                 static_cast<int>(delay_us / 1000000));
    }

    void StartDeferredNetworkServices() {
        if (network_services_started_) {
            return;
        }
        auto& app = Application::GetInstance();
        const DeviceState state = app.GetDeviceState();
        if (state == kDeviceStateConnecting || state == kDeviceStateListening ||
            state == kDeviceStateSpeaking || state == kDeviceStateAudioTesting) {
            ESP_ERROR_CHECK(esp_timer_start_once(network_services_timer_, 60000000));
            ESP_LOGW(TAG, "Deferred phone config services postponed, XiaoZhi state=%d", state);
            return;
        }
        InitializeWifiConfigServer();
        if (!wifi_config_server_.IsRunning()) {
            ESP_ERROR_CHECK(esp_timer_start_once(network_services_timer_, 60000000));
            ESP_LOGW(TAG, "Deferred phone web not ready, retry scheduled");
            return;
        }


        network_services_started_ = true;
        ESP_LOGI(TAG, "Deferred phone config services started");
    }

    bool DrawFcFrame(QdtechLandscapeDisplay* qd_display, const uint16_t* pixels,
                     uint16_t width, uint16_t height) {
        if (!qd_display || !pixels || width == 0 || height == 0 || height > 240) {
            return false;
        }

        if (width == 256 && height == 240) {
            constexpr uint16_t target_width = 256;
            constexpr uint16_t target_height = 240;
            constexpr size_t target_pixels = target_width * target_height;

            if (fc_scaled_pixels_ < target_pixels) {
                if (fc_scaled_frame_) {
                    heap_caps_free(fc_scaled_frame_);
                    fc_scaled_frame_ = nullptr;
                    fc_scaled_pixels_ = 0;
                }
                fc_scaled_frame_ = static_cast<uint16_t*>(heap_caps_malloc(
                    target_pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                if (!fc_scaled_frame_) {
                    ESP_LOGW(TAG, "FC scale buffer alloc failed, draw original frame");
                    const int fallback_x = (DISPLAY_WIDTH - width) / 2;
                    return qd_display->DrawLandscapeRgb565PreSwapped(fallback_x, 0, width, height, pixels);
                }
                fc_scaled_pixels_ = target_pixels;
            }

            memcpy(fc_scaled_frame_, pixels, target_pixels * sizeof(uint16_t));

            const int x = (DISPLAY_WIDTH - target_width) / 2;
            return qd_display->DrawLandscapeRgb565PreSwapped(x, 0, target_width, target_height, fc_scaled_frame_);
        }

        if (width > DISPLAY_WIDTH || height > 240) {
            return false;
        }
        const int x = (DISPLAY_WIDTH - width) / 2;
        const int y = (240 - height) / 2;
        return qd_display->DrawLandscapeRgb565PreSwapped(x, y, width, height, pixels);
    }

    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    SemaphoreHandle_t i2c_mutex_ = nullptr;
    i2c_master_dev_handle_t bmi270_dev_ = nullptr;
    TaskHandle_t bmi270_task_ = nullptr;
    bool bmi270_present_ = false;
    uint8_t bmi270_addr_ = 0;
    int16_t bmi270_baseline_x_ = 0;
    int16_t bmi270_baseline_y_ = 0;
    int16_t bmi270_baseline_z_ = 0;
    int64_t bmi270_baseline_mag_sq_ = 0;
    std::atomic<int64_t> touch_active_until_ms_{0};
    std::atomic<bool> shake_lab_sampling_active_{false};
    ShakeDetector shake_detector_;
    QdtechTouch touch_;
    esp_timer_handle_t touch_timer_ = nullptr;
    esp_timer_handle_t boot_power_timer_ = nullptr;
    bool boot_power_pressed_ = false;
    bool boot_power_long_handled_ = false;
    bool entering_deep_sleep_ = false;
    int64_t boot_power_press_start_ms_ = 0;
    lv_indev_t* touch_indev_ = nullptr;
    bool touch_down_ = false;
    std::atomic<bool> touch_cached_down_{false};
    std::atomic<uint16_t> touch_cached_x_{0};
    std::atomic<uint16_t> touch_cached_y_{0};
    int64_t touch_start_ms_ = 0;
    uint16_t touch_start_x_ = 0;
    uint16_t touch_start_y_ = 0;
    uint16_t touch_last_x_ = 0;
    uint16_t touch_last_y_ = 0;
    QdtechBatteryMonitor battery_monitor_;
    TimeWeatherService time_weather_service_;
    RadioService radio_service_;
    PodcastService podcast_service_;
    PhotoService photo_service_;
    FcEmulatorService fc_emulator_service_;
    QdWifiConfigServer wifi_config_server_;
    httpd_handle_t music_command_server_ = nullptr;
    uint16_t* fc_scaled_frame_ = nullptr;
    size_t fc_scaled_pixels_ = 0;
    bool time_weather_started_ = false;
    bool network_services_started_ = false;
    bool lyric_udp_started_ = false;
    int64_t phone_web_last_start_us_ = -30000000;
    int64_t phone_web_last_notice_us_ = -5000000;
    std::atomic<int> lyric_generation_{0};
    portMUX_TYPE lyric_lock_ = portMUX_INITIALIZER_UNLOCKED;
    char current_lyric_title_[96] = {};
    char current_lyric_artist_[96] = {};
    int scheduled_lyric_generation_ = 0;
    esp_timer_handle_t network_services_timer_ = nullptr;
};

DECLARE_BOARD(QdtechS3TouchLcd35Board);
