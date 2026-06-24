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
#include "radio_service.h"
#include "time_weather_service.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
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
        if (ReadReg(kTouchInfo, &touch_info, 1) != ESP_OK || !(touch_info & 0x08)) {
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
        InitializeButtons();
        InitializeTools();
        InitializeRadio();
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

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        return battery_monitor_.GetLevel(level, charging, discharging);
    }

    void StartNetwork() override {
        WifiBoard::StartNetwork();
        
        // WiFi 连接后启动时间天气服�?
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

        if (board->touch_cached_down_) {
            data->point.x = board->touch_cached_x_;
            data->point.y = board->touch_cached_y_;
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }

    void PollTouch() {
        QdtechTouch::TouchPoint points[10] = {};
        const size_t point_count = touch_.ReadPoints(points, 10);
        const bool touched = point_count > 0;
        const int64_t now_ms = esp_timer_get_time() / 1000;
        auto* desktop_ui = GetDesktopUI();

        touch_cached_down_ = touched;
        if (touched) {
            touch_cached_x_ = points[0].x;
            touch_cached_y_ = points[0].y;
        }

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
        radio_service_.Start(desktop_ui);
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
        desktop_ui->SetMainPageCallback([this]() {
            time_weather_service_.RequestRefresh(false);
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
    QdtechTouch touch_;
    esp_timer_handle_t touch_timer_ = nullptr;
    esp_timer_handle_t boot_power_timer_ = nullptr;
    bool boot_power_pressed_ = false;
    bool boot_power_long_handled_ = false;
    bool entering_deep_sleep_ = false;
    int64_t boot_power_press_start_ms_ = 0;
    lv_indev_t* touch_indev_ = nullptr;
    bool touch_down_ = false;
    bool touch_cached_down_ = false;
    uint16_t touch_cached_x_ = 0;
    uint16_t touch_cached_y_ = 0;
    int64_t touch_start_ms_ = 0;
    uint16_t touch_start_x_ = 0;
    uint16_t touch_start_y_ = 0;
    uint16_t touch_last_x_ = 0;
    uint16_t touch_last_y_ = 0;
    QdtechBatteryMonitor battery_monitor_;
    TimeWeatherService time_weather_service_;
    RadioService radio_service_;
    PhotoService photo_service_;
    FcEmulatorService fc_emulator_service_;
    uint16_t* fc_scaled_frame_ = nullptr;
    size_t fc_scaled_pixels_ = 0;
    bool time_weather_started_ = false;
};

DECLARE_BOARD(QdtechS3TouchLcd35Board);
