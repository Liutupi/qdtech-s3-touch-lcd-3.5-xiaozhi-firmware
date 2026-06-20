#include "desktop_ui.h"
#include "application.h"
#include "config.h"
#include "audio_codecs/audio_codec.h"
#include "boards/common/board.h"
#include "firmware_update_service.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <utility>

#include <esp_app_desc.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <ssid_manager.h>

#define TAG "DesktopUI"

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_48);
LV_FONT_DECLARE(qd_font_clock_72);
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(qd_font_lxgw_16);
LV_FONT_DECLARE(qd_font_lxgw_20);

// Color palette
static constexpr lv_color_t COLOR_BG = LV_COLOR_MAKE(0x00, 0x00, 0x00);
static constexpr lv_color_t COLOR_SURFACE = LV_COLOR_MAKE(0x0b, 0x0c, 0x0d);
static constexpr lv_color_t COLOR_SURFACE_2 = LV_COLOR_MAKE(0x12, 0x14, 0x13);
static constexpr lv_color_t COLOR_TEXT = LV_COLOR_MAKE(0xf6, 0xef, 0xdf);
static constexpr lv_color_t COLOR_CREAM = LV_COLOR_MAKE(0xff, 0xf4, 0xd8);
static constexpr lv_color_t COLOR_MUTED = LV_COLOR_MAKE(0x8a, 0x8a, 0x82);
static constexpr lv_color_t COLOR_LINE = LV_COLOR_MAKE(0x34, 0x35, 0x31);
static constexpr lv_color_t COLOR_GOLD = LV_COLOR_MAKE(0xff, 0xbd, 0x55);
static constexpr lv_color_t COLOR_GREEN = LV_COLOR_MAKE(0x82, 0xd7, 0x78);
static constexpr lv_color_t COLOR_PURPLE = LV_COLOR_MAKE(0xaa, 0x78, 0xff);
static constexpr lv_color_t COLOR_BLUE = LV_COLOR_MAKE(0x68, 0x9d, 0xff);
static constexpr lv_color_t COLOR_CLOCK_DOT = LV_COLOR_MAKE(0xd7, 0xde, 0xe3);
static constexpr lv_color_t RADIO_BAR_COLORS[16] = {
    LV_COLOR_MAKE(0xff, 0x6b, 0x6b), LV_COLOR_MAKE(0xff, 0x8e, 0x5a),
    LV_COLOR_MAKE(0xff, 0xb8, 0x4d), LV_COLOR_MAKE(0xf7, 0xd8, 0x5a),
    LV_COLOR_MAKE(0xc6, 0xe6, 0x6f), LV_COLOR_MAKE(0x8d, 0xdf, 0x84),
    LV_COLOR_MAKE(0x5f, 0xd6, 0xa4), LV_COLOR_MAKE(0x46, 0xcf, 0xc8),
    LV_COLOR_MAKE(0x55, 0xc7, 0xf3), LV_COLOR_MAKE(0x68, 0xb2, 0xff),
    LV_COLOR_MAKE(0x83, 0x9b, 0xff), LV_COLOR_MAKE(0x9d, 0x87, 0xf5),
    LV_COLOR_MAKE(0xb8, 0x7a, 0xe8), LV_COLOR_MAKE(0xd7, 0x77, 0xd9),
    LV_COLOR_MAKE(0xf0, 0x7c, 0xbe), LV_COLOR_MAKE(0xff, 0x88, 0x9a),
};

// Styles
static lv_style_t style_screen;
static lv_style_t style_en;
static lv_style_t style_muted;
static lv_style_t style_gold;
static lv_style_t style_green;
static lv_style_t style_panel;
static lv_style_t style_clock_card;

struct AppRow {
    const char* cn;
    const char* en;
    const char* status;
    lv_color_t color;
    lv_event_cb_t cb;
};

static void add_gesture_bubble(lv_obj_t* obj) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

static lv_obj_t* label_en(lv_obj_t* parent, const char* text, lv_style_t* style) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, style, 0);
    add_gesture_bubble(label);
    return label;
}

static void create_brand_mark(lv_obj_t* parent, int16_t x = 18, int16_t y = 4) {
    lv_obj_t* brand_a = label_en(parent, "Nothing", &style_en);
    lv_obj_set_style_text_font(brand_a, &lv_font_montserrat_20, 0);
    lv_obj_align(brand_a, LV_ALIGN_TOP_LEFT, x, y);

    lv_obj_t* brand_b = label_en(parent, "impossible", &style_gold);
    lv_obj_set_style_text_font(brand_b, &lv_font_montserrat_20, 0);
    lv_obj_align(brand_b, LV_ALIGN_TOP_LEFT, x, y + 20);
}

static lv_obj_t* circle(lv_obj_t* parent, int16_t size, lv_color_t color, lv_opa_t opa) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_radius(obj, size / 2, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    add_gesture_bubble(obj);
    return obj;
}

static lv_obj_t* bar(lv_obj_t* parent, int16_t w, int16_t h, lv_color_t color, lv_opa_t opa) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, LV_MIN(w, h) / 2, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    add_gesture_bubble(obj);
    return obj;
}

static void set_weather_part_visible(lv_obj_t* obj, bool visible) {
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int month) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    if (month < 1 || month > 12) {
        return 30;
    }
    return kDays[month - 1];
}

static const char* month_name(int month) {
    static constexpr const char* kMonthNames[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    if (month < 1 || month > 12) {
        return "Month";
    }
    return kMonthNames[month - 1];
}

static int first_weekday_monday_index(int year, int month);

static const char* chinese_weekday_for_date(int year, int month, int day) {
    static constexpr const char* kWeekdays[] = {
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE4\xB8\x80",
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE4\xBA\x8C",
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE4\xB8\x89",
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE5\x9B\x9B",
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE4\xBA\x94",
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE5\x85\xAD",
        "\xE6\x98\x9F""\xE6\x9C\x9F""\xE6\x97\xA5"
    };
    if (year <= 0 || month <= 0 || day <= 0) {
        return "--";
    }
    const int first = first_weekday_monday_index(year, month);
    return kWeekdays[(first + day - 1) % 7];
}

static int first_weekday_monday_index(int year, int month) {
    tm info = {};
    info.tm_year = year - 1900;
    info.tm_mon = month - 1;
    info.tm_mday = 1;
    info.tm_isdst = -1;
    if (mktime(&info) == (time_t)-1) {
        return 0;
    }
    return (info.tm_wday + 6) % 7;
}

// ===== Style initialization =====
static void init_styles() {
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, COLOR_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_pad_all(&style_screen, 0);

    lv_style_init(&style_en);
    lv_style_set_text_color(&style_en, COLOR_TEXT);
    lv_style_set_text_font(&style_en, &lv_font_montserrat_16);

    lv_style_init(&style_muted);
    lv_style_set_text_color(&style_muted, COLOR_MUTED);
    lv_style_set_text_font(&style_muted, &lv_font_montserrat_14);

    lv_style_init(&style_gold);
    lv_style_set_text_color(&style_gold, COLOR_GOLD);
    lv_style_set_text_font(&style_gold, &lv_font_montserrat_16);

    lv_style_init(&style_green);
    lv_style_set_text_color(&style_green, COLOR_GREEN);
    lv_style_set_text_font(&style_green, &lv_font_montserrat_16);

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, COLOR_SURFACE);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_border_color(&style_panel, COLOR_LINE);
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_radius(&style_panel, 6);
    lv_style_set_pad_all(&style_panel, 0);

    lv_style_init(&style_clock_card);
    lv_style_set_bg_color(&style_clock_card, LV_COLOR_MAKE(0x08, 0x08, 0x08));
    lv_style_set_bg_opa(&style_clock_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_clock_card, LV_COLOR_MAKE(0x2a, 0x28, 0x22));
    lv_style_set_border_width(&style_clock_card, 1);
    lv_style_set_radius(&style_clock_card, 5);
    lv_style_set_pad_all(&style_clock_card, 0);
}

// ===== Animation callbacks =====
void DesktopUI::ObjOpaCb(void* obj, int32_t value) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), value, 0);
}

void DesktopUI::ObjYCb(void* obj, int32_t value) {
    lv_obj_set_y(static_cast<lv_obj_t*>(obj), value);
}

void DesktopUI::ColonTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->clock_colon_dots_[0] || !self->clock_colon_dots_[1]) return;
    lv_opa_t opa = lv_obj_get_style_opa(self->clock_colon_dots_[0], 0);
    lv_opa_t next = opa < LV_OPA_50 ? LV_OPA_COVER : LV_OPA_40;
    lv_obj_set_style_opa(self->clock_colon_dots_[0], next, 0);
    lv_obj_set_style_opa(self->clock_colon_dots_[1], next, 0);
}

void DesktopUI::FaceTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (self) {
        self->anim_tick_++;
        self->UpdateFaceAnimation();
    }
}

static void SaveFocusCount(uint16_t count) {
    nvs_handle_t handle;
    if (nvs_open("focus", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u16(handle, "completed", count);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static uint16_t LoadFocusCount() {
    nvs_handle_t handle;
    uint16_t count = 0;
    if (nvs_open("focus", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u16(handle, "completed", &count);
        nvs_close(handle);
    }
    return count;
}

void DesktopUI::FocusTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->focus_running_) {
        return;
    }
    if (self->focus_remaining_sec_ > 0) {
        self->focus_remaining_sec_--;
    }
    if (self->focus_remaining_sec_ == 0) {
        self->focus_running_ = false;
        if (self->focus_is_work_) {
            self->focus_completed_count_++;
            SaveFocusCount(self->focus_completed_count_);
            ESP_LOGI(TAG, "Focus session completed count=%u", self->focus_completed_count_);
        } else {
            ESP_LOGI(TAG, "Focus break completed");
        }
    }
    self->UpdateFocusUI();
}

// ===== Page navigation =====
static DesktopUI* g_desktop_ui = nullptr;

static void navigate_back_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->NavigateBack();
    }
}

static void show_apps_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void open_xiaozhi_with_message(const char* state, const char* message, bool start_chat) {
    if (!g_desktop_ui) {
        return;
    }
    ESP_LOGI(TAG, "App card clicked: %s", state ? state : "XiaoZhi");
    g_desktop_ui->ShowPage(DesktopPage::XIAOZHI);
    g_desktop_ui->SetXiaozhiState(state, message, "thinking");
    if (start_chat) {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateIdle) {
            app.ToggleChatState();
        }
    }
}

static void open_app_card(uint8_t index) {
    switch (index) {
        case 0:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::RADIO);
            }
            break;
        case 1:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::PHOTO);
            }
            break;
        case 2:
            open_xiaozhi_with_message("XiaoZhi AI", "Starting conversation...", true);
            break;
        case 3:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::FC);
            }
            break;
        case 4:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::CALENDAR);
            }
            break;
        case 5:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::FOCUS);
            }
            break;
        case 6:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::NETWORK);
            }
            break;
        case 7:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::SETTINGS);
            }
            break;
        default:
            break;
    }
}

static void xiaozhi_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(2);
    }
}

static void radio_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(0);
    }
}

static void photo_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(1);
    }
}

static void fc_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(3);
    }
}

static void calendar_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(4);
    }
}

static void focus_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(5);
    }
}

static void network_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(6);
    }
}

static void settings_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(7);
    }
}

static void apps_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::MAIN);
    }
}

static void xiaozhi_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void radio_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void photo_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev || !g_desktop_ui) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_RIGHT || dir == LV_DIR_LEFT) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void fc_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui &&
        !g_desktop_ui->IsFcPlayingView()) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static uint8_t fc_controller_from_page_point(int16_t x, int16_t y) {
    if (y < 240) {
        return 0;
    }

    const int16_t rel_y = y - 240;
    uint8_t controller = 0;

    if (x < 178) {
        const int16_t dx = x - 84;
        const int16_t dy = rel_y - 40;
        const int16_t abs_dx = LV_ABS(dx);
        const int16_t abs_dy = LV_ABS(dy);

        if (abs_dx < 22 && abs_dy < 18) {
            return 0;
        }
        if (dy < -10 && abs_dy * 3 >= abs_dx * 2) {
            controller |= 0x10;
        }
        if (dy > 10 && abs_dy * 3 >= abs_dx * 2) {
            controller |= 0x20;
        }
        if (dx < -14 && abs_dx * 3 >= abs_dy * 2) {
            controller |= 0x40;
        }
        if (dx > 14 && abs_dx * 3 >= abs_dy * 2) {
            controller |= 0x80;
        }
    } else if (x >= 330 && x < 392 && rel_y >= 18) {
        controller |= 0x02;
    } else if (x >= 410 && rel_y >= 4) {
        controller |= 0x01;
    } else if (x >= 186 && x < 316 && rel_y >= 42) {
        controller |= (x < 244) ? 0x04 : 0x08;
    }

    return controller;
}

static bool fc_list_from_page_point(int16_t x, int16_t y) {
    return x >= 206 && x < 274 && y >= 244 && y < 274;
}

static void fc_page_touch_cb(lv_event_t* event) {
    if (!g_desktop_ui || !g_desktop_ui->fc_controller_cb_ || !g_desktop_ui->IsFcPlayingView()) return;

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_desktop_ui->fc_controller_cb_(0);
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;

    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    g_desktop_ui->fc_controller_cb_(fc_controller_from_page_point(point.x, point.y));
}

static void fc_key_cb(lv_event_t* event) {
    if (!g_desktop_ui || !g_desktop_ui->fc_controller_cb_ || !g_desktop_ui->IsFcPlayingView()) return;

    const uint8_t mask = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        g_desktop_ui->fc_controller_cb_(mask);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_desktop_ui->fc_controller_cb_(0);
    }
}

static void fc_prev_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->fc_prev_) {
        g_desktop_ui->fc_prev_();
    }
}

static void fc_next_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->fc_next_) {
        g_desktop_ui->fc_next_();
    }
}

static void fc_start_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->fc_play_pause_) {
        g_desktop_ui->fc_play_pause_();
    }
}

static void fc_back_list_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->fc_stop_) {
        g_desktop_ui->fc_stop_();
    }
}

static void calendar_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void calendar_prev_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->AdjustCalendarMonth(-1);
    }
}

static void calendar_today_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ShowTodayCalendar();
    }
}

static void calendar_next_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->AdjustCalendarMonth(1);
    }
}

static void radio_prev_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        if (g_desktop_ui->radio_prev_) {
            g_desktop_ui->radio_prev_();
        }
    }
}

static void radio_play_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        if (g_desktop_ui->radio_play_pause_) {
            g_desktop_ui->radio_play_pause_();
        }
    }
}

static void radio_stop_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        if (g_desktop_ui->radio_stop_) {
            g_desktop_ui->radio_stop_();
        }
    }
}

static void radio_next_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        if (g_desktop_ui->radio_next_) {
            g_desktop_ui->radio_next_();
        }
    }
}

static void focus_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void focus_start_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ToggleFocusTimer();
    }
}

static void focus_reset_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ResetFocusTimer();
    }
}

static void focus_work_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->SetFocusMode(true);
    }
}

static void focus_break_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->SetFocusMode(false);
    }
}

// 音量动态柱动画回调
static void RadioAnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self) return;

    for (int i = 0; i < 16; i++) {
        if (!self->radio_bars_[i]) continue;

        if (self->radio_playing_) {
            // 播放中：随机高度模拟频谱
            uint16_t height = 10 + (esp_random() % 50);
            lv_obj_set_style_bg_opa(self->radio_bars_[i], LV_OPA_COVER, 0);
            lv_obj_set_height(self->radio_bars_[i], height);
            lv_obj_align(self->radio_bars_[i], LV_ALIGN_BOTTOM_LEFT,
                        24 + i * 28, 0);
        } else {
            // 停止：显示静默状态
            lv_obj_set_style_bg_opa(self->radio_bars_[i], LV_OPA_50, 0);
            lv_obj_set_height(self->radio_bars_[i], 5);
            lv_obj_align(self->radio_bars_[i], LV_ALIGN_BOTTOM_LEFT,
                        24 + i * 28, 0);
        }
    }
}

static void settings_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void network_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void network_wifi_item_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !g_desktop_ui) return;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    uintptr_t index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));
    g_desktop_ui->SetDefaultNetwork(static_cast<size_t>(index));
}

static void settings_brightness_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED && g_desktop_ui) {
        auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
        g_desktop_ui->SetSystemBrightness(lv_slider_get_value(slider));
    }
}

static void settings_volume_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_RELEASED && g_desktop_ui) {
        auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
        g_desktop_ui->SetSystemVolume(lv_slider_get_value(slider));
    }
}

static void settings_firmware_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        FirmwareUpdateService::GetInstance().HandleButton();
    }
}

static void main_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_LEFT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void xiaozhi_toggle_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        Application::GetInstance().ToggleChatState();
    }
}

// ===== DesktopUI implementation =====
void DesktopUI::Create() {
    g_desktop_ui = this;
    init_styles();

    lv_obj_t* root = lv_scr_act();
    lv_obj_set_style_bg_color(root, COLOR_BG, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    CreateMainPage(root);
    CreateAppsPage(root);
    CreatePhotoPage(root);
    CreateFcPage(root);
    CreateCalendarPage(root);
    CreateRadioPage(root);
    CreateFocusPage(root);
    CreateXiaozhiPage(root);
    CreateNetworkPage(root);
    CreateSettingsPage(root);

    // Start with main page
    ShowPage(DesktopPage::MAIN);

    // Face animation timer
    lv_timer_create(FaceTimerCb, 100, this);

    ESP_LOGI(TAG, "Desktop UI created");
}

void DesktopUI::ShowPage(DesktopPage page) {
    const bool was_main = current_page_ == DesktopPage::MAIN;
    const bool was_photo = current_page_ == DesktopPage::PHOTO;
    const bool was_fc = current_page_ == DesktopPage::FC;
    current_page_ = page;
    lv_obj_add_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
    if (photo_page_) {
        lv_obj_add_flag(photo_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (fc_page_) {
        lv_obj_add_flag(fc_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (calendar_page_) {
        lv_obj_add_flag(calendar_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (focus_page_) {
        lv_obj_add_flag(focus_page_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(radio_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
    if (network_page_) {
        lv_obj_add_flag(network_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings_page_) {
        lv_obj_add_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
    }

    switch (page) {
        case DesktopPage::MAIN:
            lv_obj_clear_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show main page");
            break;
        case DesktopPage::APPS:
            lv_obj_clear_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show apps page");
            break;
        case DesktopPage::PHOTO:
            if (photo_page_) {
                lv_obj_clear_flag(photo_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Show photo page");
            break;
        case DesktopPage::FC:
            if (fc_page_) {
                lv_obj_clear_flag(fc_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Show FC page");
            break;
        case DesktopPage::CALENDAR:
            if (calendar_page_) {
                lv_obj_clear_flag(calendar_page_, LV_OBJ_FLAG_HIDDEN);
                RenderCalendar();
            }
            ESP_LOGI(TAG, "Show calendar page");
            break;
        case DesktopPage::RADIO:
            lv_obj_clear_flag(radio_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show radio page");
            break;
        case DesktopPage::FOCUS:
            if (focus_page_) {
                lv_obj_clear_flag(focus_page_, LV_OBJ_FLAG_HIDDEN);
                UpdateFocusUI();
            }
            ESP_LOGI(TAG, "Show focus page");
            break;
        case DesktopPage::XIAOZHI:
            lv_obj_clear_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show xiaozhi page");
            break;
        case DesktopPage::NETWORK:
            if (network_page_) {
                lv_obj_clear_flag(network_page_, LV_OBJ_FLAG_HIDDEN);
                UpdateWifiList();
            }
            ESP_LOGI(TAG, "Show network page");
            break;
        case DesktopPage::SETTINGS:
            if (settings_page_) {
                lv_obj_clear_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
                RefreshSettingsControls();
            }
            ESP_LOGI(TAG, "Show settings page");
            break;
    }

    const bool is_photo = page == DesktopPage::PHOTO;
    if (photo_active_callback_ && was_photo != is_photo) {
        photo_active_callback_(is_photo);
    }
    const bool is_fc = page == DesktopPage::FC;
    if (fc_active_callback_ && was_fc != is_fc) {
        fc_active_callback_(is_fc);
    }
    if (was_fc && !is_fc && fc_exit_callback_) {
        fc_exit_callback_();
    }
    const bool is_main = page == DesktopPage::MAIN;
    if (is_main && !was_main && main_page_callback_) {
        main_page_callback_();
    }
    if (lv_screen_active()) {
        lv_obj_invalidate(lv_screen_active());
    }
}

void DesktopUI::HandleSwipe(int16_t dx, int16_t dy) {
    const int16_t min_dx = 70;
    const int16_t max_dy = 90;

    if (LV_ABS(dy) >= max_dy) {
        return;
    }

    if (current_page_ == DesktopPage::PHOTO && LV_ABS(dx) > min_dx) {
        NavigateBack();
    } else if (current_page_ == DesktopPage::MAIN && dx < -min_dx) {
        ShowPage(DesktopPage::APPS);
    } else if (current_page_ != DesktopPage::MAIN && dx > min_dx) {
        NavigateBack();
    }
}

void DesktopUI::NavigateBack() {
    if (current_page_ == DesktopPage::MAIN) {
        return;
    }

    const DesktopPage target = current_page_ == DesktopPage::APPS
        ? DesktopPage::MAIN
        : DesktopPage::APPS;
    ESP_LOGI(TAG, "Navigate back page=%d target=%d",
             static_cast<int>(current_page_), static_cast<int>(target));
    ShowPage(target);
}

void DesktopUI::HandleTouchRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y,
                                   int64_t duration_ms) {
    const int16_t dx = static_cast<int16_t>(end_x) - static_cast<int16_t>(start_x);
    const int16_t dy = static_cast<int16_t>(end_y) - static_cast<int16_t>(start_y);

    if (current_page_ == DesktopPage::SETTINGS) {
        // Settings page scrolling and sliders are now handled by LVGL
        // Only keep gesture detection for swipe navigation
        if (LV_ABS(dy) > 30 && LV_ABS(dy) > LV_ABS(dx)) {
            if (settings_content_) {
                lv_obj_scroll_by_bounded(settings_content_, 0, dy, LV_ANIM_ON);
                ESP_LOGI(TAG, "Settings scroll dy=%d", dy);
            }
            return;
        }
        // Slider interactions are now handled by LVGL click events
        return;
    }

    if (duration_ms < 300 && LV_ABS(dx) < 30 && LV_ABS(dy) < 30) {
        HandleTap(end_x, end_y);
    } else if (duration_ms < 500) {
        HandleSwipe(dx, dy);
    } else {
        ESP_LOGI(TAG, "Touch release ignored dx=%d dy=%d duration=%dms",
                 dx, dy, static_cast<int>(duration_ms));
    }
}

void DesktopUI::HandleTouchState(uint16_t x, uint16_t y, bool pressed) {
    if (current_page_ != DesktopPage::FC || !fc_playing_view_ || !fc_controller_cb_) {
        return;
    }

    if (!pressed) {
        fc_controller_cb_(0);
        return;
    }

    HandleTouchPoints(&x, &y, 1);
}

void DesktopUI::HandleTouchPoints(const uint16_t* xs, const uint16_t* ys, size_t count) {
    if (current_page_ != DesktopPage::FC || !fc_playing_view_ || !fc_controller_cb_) {
        return;
    }

    uint8_t controller = 0;
    bool list_hit = false;
    for (size_t i = 0; xs && ys && i < count; ++i) {
        list_hit = list_hit || fc_list_from_page_point(xs[i], ys[i]);
        controller |= fc_controller_from_page_point(xs[i], ys[i]);
    }
    if (list_hit) {
        if (!fc_list_touch_latched_ && fc_stop_) {
            fc_list_touch_latched_ = true;
            fc_stop_();
        }
        return;
    }
    fc_list_touch_latched_ = false;
    fc_controller_cb_(controller);
}

void DesktopUI::HandleTap(uint16_t x, uint16_t y) {
    ESP_LOGI(TAG, "Tap x=%u y=%u page=%d", x, y, static_cast<int>(current_page_));

    if (current_page_ == DesktopPage::MAIN) {
        if (x >= 386 && x < 462 && y >= 10 && y < 42) {
            ShowPage(DesktopPage::APPS);
        }
        return;
    }

    if (current_page_ == DesktopPage::XIAOZHI) {
        if (x >= 360 && x < 470 && y >= 35 && y < 90) {
            NavigateBack();
            return;
        }
        if (x >= 177 && x < 303 && y >= 264 && y < 302) {
            Application::GetInstance().ToggleChatState();
            return;
        }
        return;
    }

    if (current_page_ == DesktopPage::RADIO) {
        // Radio page buttons are now handled by LVGL click events
        return;
    }

    if (current_page_ == DesktopPage::PHOTO) {
        return;
    }

    if (current_page_ == DesktopPage::FC) {
        return;
    }

    if (current_page_ == DesktopPage::CALENDAR) {
        // Calendar page buttons are now handled by LVGL click events
        return;
    }

    if (current_page_ == DesktopPage::FOCUS) {
        // Focus page buttons are now handled by LVGL click events
        return;
    }

    if (current_page_ == DesktopPage::SETTINGS) {
        if (x >= 360 && x < 470 && y >= 35 && y < 90) {
            NavigateBack();
        }
        return;
    }

    if (current_page_ != DesktopPage::APPS) {
        return;
    }

    // Apps page buttons are now handled by LVGL click events
    // Only keep gesture detection for swipe navigation
    return;
}

// ===== Main page =====
void DesktopUI::CreateMainPage(lv_obj_t* root) {
    main_page_ = lv_obj_create(root);
    lv_obj_add_style(main_page_, &style_screen, 0);
    lv_obj_set_size(main_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(main_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(main_page_, main_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(main_page_);

    // Brand
    lv_obj_t* brand_a = label_en(main_page_, "Nothing", &style_en);
    lv_obj_set_style_text_font(brand_a, &lv_font_montserrat_20, 0);
    lv_obj_align(brand_a, LV_ALIGN_TOP_LEFT, 20, 10);

    lv_obj_t* brand_b = label_en(main_page_, "impossible", &style_gold);
    lv_obj_set_style_text_font(brand_b, &lv_font_montserrat_20, 0);
    lv_obj_align(brand_b, LV_ALIGN_TOP_LEFT, 20, 32);
    
    lv_obj_t* brand_c = label_en(main_page_, "Tupi", &style_muted);
    lv_obj_set_style_text_font(brand_c, &lv_font_montserrat_14, 0);
    lv_obj_align(brand_c, LV_ALIGN_TOP_LEFT, 20, 54);

    CreateBigTime(main_page_);

    // Date and weekday
    date_label_ = label_en(main_page_, "01 / 01     |", &style_en);
    lv_obj_set_style_text_color(date_label_, COLOR_CREAM, 0);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(date_label_, LV_ALIGN_TOP_LEFT, 52, 174);

    week_label_ = label_en(main_page_, "MON", &style_green);
    lv_obj_set_style_text_font(week_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(week_label_, LV_ALIGN_TOP_LEFT, 178, 174);

    CreateWeatherPanel(main_page_);
    CreateQuotePanel(main_page_);

    // Menu button
    lv_obj_t* menu = CreateButton(main_page_, "Menu", show_apps_cb);
    lv_obj_align(menu, LV_ALIGN_TOP_RIGHT, -18, 10);
}

void DesktopUI::CreateBigTime(lv_obj_t* parent) {
    lv_obj_t* time_group = lv_obj_create(parent);
    lv_obj_remove_style_all(time_group);
    lv_obj_set_size(time_group, 254, 154);
    lv_obj_align(time_group, LV_ALIGN_TOP_LEFT, 20, 18);
    lv_obj_clear_flag(time_group, LV_OBJ_FLAG_SCROLLABLE);
    add_gesture_bubble(time_group);

    clock_hour_label_ = lv_label_create(time_group);
    lv_label_set_text(clock_hour_label_, "00");
    lv_obj_set_size(clock_hour_label_, 108, 60);
    lv_obj_set_style_text_font(clock_hour_label_, &qd_font_clock_72, 0);
    lv_obj_set_style_text_color(clock_hour_label_, COLOR_CREAM, 0);
    lv_obj_set_style_text_align(clock_hour_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(clock_hour_label_, LV_ALIGN_TOP_LEFT, 0, 77);
    add_gesture_bubble(clock_hour_label_);

    clock_minute_label_ = lv_label_create(time_group);
    lv_label_set_text(clock_minute_label_, "00");
    lv_obj_set_size(clock_minute_label_, 110, 60);
    lv_obj_set_style_text_font(clock_minute_label_, &qd_font_clock_72, 0);
    lv_obj_set_style_text_color(clock_minute_label_, COLOR_GOLD, 0);
    lv_obj_set_style_text_align(clock_minute_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(clock_minute_label_, LV_ALIGN_TOP_LEFT, 142, 77);
    add_gesture_bubble(clock_minute_label_);

    clock_colon_dots_[0] = circle(time_group, 18, COLOR_CLOCK_DOT, LV_OPA_COVER);
    lv_obj_align(clock_colon_dots_[0], LV_ALIGN_TOP_LEFT, 118, 73);
    clock_colon_dots_[1] = circle(time_group, 18, COLOR_CLOCK_DOT, LV_OPA_COVER);
    lv_obj_align(clock_colon_dots_[1], LV_ALIGN_TOP_LEFT, 118, 116);
    lv_timer_create(ColonTimerCb, 500, this);

    RenderBigTime(0, 0, false);
}

void DesktopUI::CreateWeatherPanel(lv_obj_t* parent) {
    lv_obj_t* panel = CreatePanel(parent, 166, 154, 294, 50);
    lv_obj_set_style_bg_color(panel, COLOR_SURFACE_2, 0);

    lv_obj_t* title = label_en(panel, "Weather", &style_muted);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    // Weather icon pieces are reused for all weather codes to keep the main page light.
    weather_sun_ = circle(panel, 54, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align(weather_sun_, LV_ALIGN_TOP_LEFT, 56, 28);

    // Clouds
    weather_cloud_[0] = circle(panel, 38, COLOR_CREAM, LV_OPA_70);
    lv_obj_align(weather_cloud_[0], LV_ALIGN_TOP_LEFT, 42, 60);
    weather_cloud_[1] = circle(panel, 46, COLOR_CREAM, LV_OPA_80);
    lv_obj_align(weather_cloud_[1], LV_ALIGN_TOP_LEFT, 72, 52);
    weather_cloud_[2] = bar(panel, 88, 26, COLOR_CREAM, LV_OPA_80);
    lv_obj_align(weather_cloud_[2], LV_ALIGN_TOP_LEFT, 40, 78);

    for (int i = 0; i < 3; ++i) {
        weather_rain_[i] = bar(panel, 4, 20, COLOR_BLUE, LV_OPA_80);
        lv_obj_align(weather_rain_[i], LV_ALIGN_TOP_LEFT, 55 + i * 22, 100);
        lv_obj_set_style_transform_rotation(weather_rain_[i], 180, 0);

        weather_snow_[i] = circle(panel, 8, COLOR_CREAM, LV_OPA_COVER);
        lv_obj_align(weather_snow_[i], LV_ALIGN_TOP_LEFT, 54 + i * 24, 104);
    }

    weather_storm_[0] = bar(panel, 10, 32, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align(weather_storm_[0], LV_ALIGN_TOP_LEFT, 84, 92);
    lv_obj_set_style_transform_rotation(weather_storm_[0], 250, 0);
    weather_storm_[1] = bar(panel, 10, 26, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align(weather_storm_[1], LV_ALIGN_TOP_LEFT, 72, 112);
    lv_obj_set_style_transform_rotation(weather_storm_[1], 250, 0);

    // Shared weather pulse animation
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, weather_sun_);
    lv_anim_set_values(&anim, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_time(&anim, 1600);
    lv_anim_set_playback_time(&anim, 1600);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, ObjOpaCb);
    lv_anim_start(&anim);
    
    // Weather particle animation timer
    weather_particle_timer_ = lv_timer_create(WeatherParticleCb, 200, this);
    lv_timer_pause(weather_particle_timer_);

    weather_temp_label_ = label_en(panel, "-- C", &style_en);
    lv_obj_set_style_text_font(weather_temp_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(weather_temp_label_, 142);
    lv_label_set_long_mode(weather_temp_label_, LV_LABEL_LONG_CLIP);
    lv_obj_align(weather_temp_label_, LV_ALIGN_TOP_LEFT, 12, 108);

    weather_meta_label_ = label_en(panel, "Weather pending", &style_green);
    lv_obj_set_width(weather_meta_label_, 142);
    lv_label_set_long_mode(weather_meta_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(weather_meta_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(weather_meta_label_, LV_ALIGN_TOP_LEFT, 12, 132);

    ApplyWeatherVisual(-1);
}

void DesktopUI::CreateQuotePanel(lv_obj_t* parent) {
    daily_card_panel_ = CreatePanel(parent, 438, 94, 20, 214);

    daily_card_date_label_ = label_en(daily_card_panel_, "--/--", &style_gold);
    lv_obj_set_width(daily_card_date_label_, 92);
    lv_obj_set_style_text_align(daily_card_date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(daily_card_date_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(daily_card_date_label_, LV_ALIGN_TOP_LEFT, 16, 15);

    daily_card_title_label_ = label_en(daily_card_panel_, "今日", &style_muted);
    lv_obj_set_width(daily_card_title_label_, 108);
    lv_label_set_long_mode(daily_card_title_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(daily_card_title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(daily_card_title_label_, &font_puhui_16_4, 0);
    lv_obj_align(daily_card_title_label_, LV_ALIGN_TOP_LEFT, 16, 47);

    lv_obj_t* divider = bar(daily_card_panel_, 2, 62, COLOR_LINE, LV_OPA_COVER);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 132, 16);

    quote_label_ = label_en(daily_card_panel_, "正在同步今日卡片", &style_en);
    lv_obj_set_width(quote_label_, 266);
    lv_label_set_long_mode(quote_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(quote_label_, &font_puhui_16_4, 0);
    lv_obj_align(quote_label_, LV_ALIGN_TOP_LEFT, 152, 14);

    network_status_label_ = label_en(daily_card_panel_, "XiaoZhi AI", &style_muted);
    lv_obj_set_width(network_status_label_, 266);
    lv_label_set_long_mode(network_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(network_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(network_status_label_, LV_ALIGN_BOTTOM_LEFT, 152, -7);
    
    // Daily card breathing animation
    lv_timer_create(DailyCardBreathCb, 50, this);
}

// ===== Apps page =====
void DesktopUI::CreateAppsPage(lv_obj_t* root) {
    apps_page_ = lv_obj_create(root);
    lv_obj_add_style(apps_page_, &style_screen, 0);
    lv_obj_set_size(apps_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(apps_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(apps_page_, apps_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_set_style_bg_color(apps_page_, LV_COLOR_MAKE(0x0e, 0x08, 0x05), 0);

    create_brand_mark(apps_page_);

    CreateStatusBar(apps_page_);

    lv_obj_t* title = label_en(apps_page_, "Apps", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(apps_page_, "App Center", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 86, 53);

    lv_obj_t* back = CreateButton(apps_page_, "Back", navigate_back_cb);
    lv_obj_set_style_bg_color(back, LV_COLOR_MAKE(0x24, 0x16, 0x0f), 0);
    lv_obj_set_style_border_color(back, LV_COLOR_MAKE(0x78, 0x48, 0x26), 0);
    lv_obj_set_style_radius(back, 16, 0);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    // App tiles
    struct AppInfo {
        const char* cn;
        const char* en;
        const char* status;
        lv_color_t color;
        lv_event_cb_t cb;
    };

    AppInfo apps[] = {
        {"RAD", "Radio", "Music FM", COLOR_GOLD, radio_card_cb},
        {"PIC", "Photos", "SD Slideshow", COLOR_GREEN, photo_card_cb},
        {"AI", "XiaoZhi", "Online", COLOR_PURPLE, xiaozhi_card_cb},
        {"FC", "NES", "SD ROMs", COLOR_GREEN, fc_card_cb},
        {"CAL", "Calendar", "Today", COLOR_PURPLE, calendar_card_cb},
        {"FOC", "Focus", "25 min", COLOR_GOLD, focus_card_cb},
        {"NET", "Network", "WiFi Hub", COLOR_BLUE, network_card_cb},
        {"SET", "Settings", "System", COLOR_GOLD, settings_card_cb},
    };

    for (uint8_t i = 0; i < 8; ++i) {
        lv_obj_t* tile = CreateAppTile(apps_page_, i, apps[i].cn, apps[i].en, apps[i].status, apps[i].color);
        if (apps[i].cb) {
            lv_obj_add_event_cb(tile, apps[i].cb, LV_EVENT_CLICKED, NULL);
        }
    }

    lv_obj_t* hint = label_en(apps_page_, "Swipe right: Home", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

lv_obj_t* DesktopUI::CreateAppTile(lv_obj_t* parent, uint8_t index, const char* cn, const char* en, const char* status, lv_color_t color) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_add_style(box, &style_panel, 0);
    lv_obj_set_size(box, 204, 48);
    const int16_t x = 24 + (index % 2) * 218;
    const int16_t y = 82 + (index / 2) * 54;
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(box, apps_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(box, LV_COLOR_MAKE(0x18, 0x0f, 0x0a), 0);
    lv_obj_set_style_border_color(box, LV_COLOR_MAKE(0x68, 0x3d, 0x22), 0);
    lv_obj_set_style_radius(box, 6, 0);
    add_gesture_bubble(box);

    lv_obj_t* icon_box = lv_obj_create(box);
    lv_obj_remove_style_all(icon_box);
    lv_obj_set_size(icon_box, 36, 34);
    lv_obj_set_style_radius(icon_box, 6, 0);
    lv_obj_set_style_bg_color(icon_box, LV_COLOR_MAKE(0x1b, 0x11, 0x0b), 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(icon_box, COLOR_GOLD, 0);
    lv_obj_set_style_border_width(icon_box, 1, 0);
    lv_obj_align(icon_box, LV_ALIGN_TOP_LEFT, 10, 7);
    add_gesture_bubble(icon_box);

    lv_obj_t* cn_label = label_en(icon_box, cn, &style_gold);
    lv_obj_set_style_text_font(cn_label, &lv_font_montserrat_12, 0);
    lv_obj_center(cn_label);

    lv_obj_t* en_label = label_en(box, en, &style_gold);
    lv_obj_set_style_text_color(en_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(en_label, &lv_font_montserrat_16, 0);
    lv_obj_align(en_label, LV_ALIGN_TOP_LEFT, 58, 8);

    lv_obj_t* dot = circle(box, 5, color, LV_OPA_COVER);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 58, 31);
    lv_obj_t* status_label = label_en(box, status, &style_muted);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 67, 27);
    if (index == 4) {
        calendar_app_status_label_ = status_label;
    }

    switch (index) {
        case 0: {
            for (int i = 0; i < 7; ++i) {
                const int h = 8 + ((i % 3) * 5);
                lv_obj_t* wave = bar(box, 3, h, COLOR_GOLD, LV_OPA_COVER);
                lv_obj_align(wave, LV_ALIGN_TOP_RIGHT, -74 + i * 8, 20 - h / 2);
            }
            break;
        }
        case 1: {
            lv_obj_t* frame = lv_obj_create(box);
            lv_obj_remove_style_all(frame);
            lv_obj_set_size(frame, 58, 26);
            lv_obj_set_style_radius(frame, 3, 0);
            lv_obj_set_style_bg_color(frame, LV_COLOR_MAKE(0x8b, 0x6c, 0x45), 0);
            lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(frame, LV_COLOR_MAKE(0xe8, 0xc9, 0x8e), 0);
            lv_obj_set_style_border_width(frame, 1, 0);
            lv_obj_align(frame, LV_ALIGN_TOP_RIGHT, -14, 11);
            add_gesture_bubble(frame);
            lv_obj_t* mountain_a = bar(frame, 20, 3, COLOR_CREAM, LV_OPA_COVER);
            lv_obj_set_style_transform_rotation(mountain_a, 420, 0);
            lv_obj_align(mountain_a, LV_ALIGN_TOP_LEFT, 11, 15);
            lv_obj_t* mountain_b = bar(frame, 24, 3, COLOR_CREAM, LV_OPA_COVER);
            lv_obj_set_style_transform_rotation(mountain_b, -450, 0);
            lv_obj_align(mountain_b, LV_ALIGN_TOP_LEFT, 24, 14);
            break;
        }
        case 2: {
            lv_obj_t* face = circle(box, 28, LV_COLOR_MAKE(0x20, 0x14, 0x0d), LV_OPA_COVER);
            lv_obj_set_style_border_color(face, COLOR_GOLD, 0);
            lv_obj_set_style_border_width(face, 2, 0);
            lv_obj_align(face, LV_ALIGN_TOP_RIGHT, -32, 10);
            lv_obj_t* eye_l = circle(face, 3, COLOR_GOLD, LV_OPA_COVER);
            lv_obj_align(eye_l, LV_ALIGN_TOP_LEFT, 8, 9);
            lv_obj_t* eye_r = circle(face, 3, COLOR_GOLD, LV_OPA_COVER);
            lv_obj_align(eye_r, LV_ALIGN_TOP_RIGHT, -8, 9);
            lv_obj_t* smile = label_en(face, ")", &style_gold);
            lv_obj_set_style_text_font(smile, &lv_font_montserrat_16, 0);
            lv_obj_set_style_transform_rotation(smile, 900, 0);
            lv_obj_align(smile, LV_ALIGN_BOTTOM_MID, 0, -3);
            break;
        }
        case 3: {
            lv_obj_t* cart = bar(box, 42, 18, LV_COLOR_MAKE(0x8d, 0xa7, 0xb4), LV_OPA_COVER);
            lv_obj_set_style_radius(cart, 2, 0);
            lv_obj_set_style_border_color(cart, LV_COLOR_MAKE(0x5c, 0x3a, 0x24), 0);
            lv_obj_set_style_border_width(cart, 2, 0);
            lv_obj_align(cart, LV_ALIGN_TOP_RIGHT, -28, 15);
            lv_obj_t* led = circle(box, 6, LV_COLOR_MAKE(0xc5, 0x6e, 0x4c), LV_OPA_COVER);
            lv_obj_align(led, LV_ALIGN_TOP_RIGHT, -15, 21);
            break;
        }
        case 4: {
            lv_obj_t* day = label_en(box, "18", &style_en);
            lv_obj_set_style_text_font(day, &lv_font_montserrat_20, 0);
            lv_obj_align(day, LV_ALIGN_TOP_RIGHT, -36, 13);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    lv_obj_t* mark = circle(box, 3, COLOR_GOLD, LV_OPA_60);
                    lv_obj_align(mark, LV_ALIGN_TOP_RIGHT, -16 + c * 6, 13 + r * 6);
                }
            }
            break;
        }
        case 5: {
            lv_obj_t* ring = circle(box, 36, LV_COLOR_MAKE(0x1c, 0x11, 0x0b), LV_OPA_COVER);
            lv_obj_set_style_border_color(ring, LV_COLOR_MAKE(0xe0, 0x8d, 0x4d), 0);
            lv_obj_set_style_border_width(ring, 4, 0);
            lv_obj_align(ring, LV_ALIGN_TOP_RIGHT, -25, 6);
            lv_obj_t* number = label_en(ring, "25", &style_en);
            lv_obj_set_style_text_font(number, &lv_font_montserrat_12, 0);
            lv_obj_center(number);
            break;
        }
        case 6: {
            lv_obj_t* wifi_icon = lv_obj_create(box);
            lv_obj_remove_style_all(wifi_icon);
            lv_obj_set_size(wifi_icon, 54, 36);
            lv_obj_align(wifi_icon, LV_ALIGN_TOP_RIGHT, -18, 7);
            add_gesture_bubble(wifi_icon);

            auto make_wifi_arc = [&](int16_t size, int16_t x, int16_t y) {
                lv_obj_t* arc = lv_arc_create(wifi_icon);
                lv_obj_remove_style_all(arc);
                lv_obj_set_size(arc, size, size);
                lv_arc_set_angles(arc, 220, 320);
                lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
                lv_obj_set_style_arc_color(arc, COLOR_GREEN, LV_PART_INDICATOR);
                lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
                lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
                lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
                lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_pos(arc, x, y);
                add_gesture_bubble(arc);
            };
            make_wifi_arc(42, 6, 0);
            make_wifi_arc(30, 12, 7);
            make_wifi_arc(18, 18, 14);
            lv_obj_t* base = circle(wifi_icon, 6, COLOR_GREEN, LV_OPA_COVER);
            lv_obj_set_pos(base, 24, 29);
            break;
        }
        case 7: {
            for (int i = 0; i < 3; ++i) {
                lv_obj_t* line = bar(box, 54, 2, COLOR_CREAM, LV_OPA_COVER);
                lv_obj_align(line, LV_ALIGN_TOP_RIGHT, -16, 13 + i * 11);
                lv_obj_t* knob = circle(box, 6, COLOR_GOLD, LV_OPA_COVER);
                lv_obj_align(knob, LV_ALIGN_TOP_RIGHT, -36 + (i % 2) * 18, 11 + i * 11);
            }
            break;
        }
    }
    return box;
}

// ===== Photo page =====
void DesktopUI::CreatePhotoPage(lv_obj_t* root) {
    photo_page_ = lv_obj_create(root);
    lv_obj_add_style(photo_page_, &style_screen, 0);
    lv_obj_set_size(photo_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(photo_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(photo_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(photo_page_, photo_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(photo_page_);

    lv_obj_t* frame = CreatePanel(photo_page_, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0);
    lv_obj_set_style_bg_color(frame, LV_COLOR_MAKE(0x06, 0x06, 0x06), 0);
    lv_obj_set_style_border_width(frame, 0, 0);
    lv_obj_set_style_radius(frame, 0, 0);
    lv_obj_move_background(frame);

    photo_image_a_ = lv_image_create(frame);
    photo_image_b_ = lv_image_create(frame);
    lv_obj_set_style_opa(photo_image_a_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(photo_image_b_, LV_OPA_TRANSP, 0);
    lv_obj_center(photo_image_a_);
    lv_obj_center(photo_image_b_);
    add_gesture_bubble(photo_image_a_);
    add_gesture_bubble(photo_image_b_);
}

// ===== FC emulator page =====
void DesktopUI::CreateFcPage(lv_obj_t* root) {
    fc_page_ = lv_obj_create(root);
    lv_obj_add_style(fc_page_, &style_screen, 0);
    lv_obj_set_size(fc_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(fc_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(fc_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(fc_page_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fc_page_, fc_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(fc_page_, fc_page_touch_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(fc_page_, fc_page_touch_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(fc_page_, fc_page_touch_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(fc_page_, fc_page_touch_cb, LV_EVENT_PRESS_LOST, NULL);
    add_gesture_bubble(fc_page_);

    fc_list_group_ = lv_obj_create(fc_page_);
    lv_obj_remove_style_all(fc_list_group_);
    lv_obj_set_size(fc_list_group_, 480, 320);
    lv_obj_align(fc_list_group_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(fc_list_group_, LV_OBJ_FLAG_SCROLLABLE);
    add_gesture_bubble(fc_list_group_);

    fc_game_group_ = lv_obj_create(fc_page_);
    lv_obj_remove_style_all(fc_game_group_);
    lv_obj_set_size(fc_game_group_, 480, 320);
    lv_obj_align(fc_game_group_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(fc_game_group_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(fc_game_group_, LV_OBJ_FLAG_HIDDEN);
    add_gesture_bubble(fc_game_group_);

    fc_title_label_ = label_en(fc_list_group_, "FC / NES", &style_gold);
    lv_obj_set_style_text_font(fc_title_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(fc_title_label_, LV_ALIGN_TOP_LEFT, 24, 14);

    fc_detail_label_ = label_en(fc_list_group_, "Scanning SD card", &style_muted);
    lv_obj_set_style_text_font(fc_detail_label_, &font_puhui_16_4, 0);
    lv_obj_set_width(fc_detail_label_, 300);
    lv_label_set_long_mode(fc_detail_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(fc_detail_label_, LV_ALIGN_TOP_LEFT, 132, 19);

    lv_obj_t* list_panel = CreatePanel(fc_list_group_, 432, 204, 24, 52);
    lv_obj_set_style_bg_color(list_panel, LV_COLOR_MAKE(0x05, 0x07, 0x09), 0);
    lv_obj_set_style_border_color(list_panel, LV_COLOR_MAKE(0x26, 0x31, 0x3c), 0);
    lv_obj_set_style_border_width(list_panel, 1, 0);
    lv_obj_set_style_radius(list_panel, 6, 0);

    fc_list_label_ = label_en(list_panel, "No .nes\n/sdcard/nes", &style_en);
    lv_obj_set_style_text_font(fc_list_label_, &font_puhui_16_4, 0);
    lv_obj_set_width(fc_list_label_, 400);
    lv_label_set_long_mode(fc_list_label_, LV_LABEL_LONG_CLIP);
    lv_obj_align(fc_list_label_, LV_ALIGN_TOP_LEFT, 16, 14);

    lv_obj_t* back = CreateButton(fc_list_group_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 24, 272);

    lv_obj_t* prev = CreateButton(fc_list_group_, "Prev", fc_prev_cb);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 126, 272);

    lv_obj_t* start = CreateButton(fc_list_group_, "Start", fc_start_cb);
    lv_obj_set_size(start, 104, 32);
    lv_obj_align(start, LV_ALIGN_TOP_LEFT, 224, 272);

    lv_obj_t* next = CreateButton(fc_list_group_, "Next", fc_next_cb);
    lv_obj_align(next, LV_ALIGN_TOP_LEFT, 356, 272);

    lv_obj_t* screen = CreatePanel(fc_game_group_, 480, 240, 0, 0);
    lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0x02, 0x03, 0x05), 0);
    lv_obj_set_style_border_color(screen, LV_COLOR_MAKE(0x26, 0x31, 0x3c), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_radius(screen, 0, 0);

    fc_screen_image_ = lv_image_create(screen);
    lv_obj_center(fc_screen_image_);
    add_gesture_bubble(fc_screen_image_);

    lv_obj_t* controls = CreatePanel(fc_game_group_, 480, 80, 0, 240);
    lv_obj_set_style_bg_color(controls, LV_COLOR_MAKE(0x05, 0x07, 0x09), 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_radius(controls, 0, 0);

    auto make_key = [&](const char* text, int x, int y, uint8_t mask, int w = 64, int h = 44) {
        lv_obj_t* key = lv_obj_create(fc_game_group_);
        lv_obj_add_style(key, &style_panel, 0);
        lv_obj_set_size(key, w, h);
        lv_obj_align(key, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_clear_flag(key, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(key, LV_COLOR_MAKE(0x12, 0x1a, 0x20), 0);
        lv_obj_set_style_bg_opa(key, LV_OPA_80, 0);
        lv_obj_set_style_border_color(key, LV_COLOR_MAKE(0x47, 0xb3, 0xff), 0);
        lv_obj_set_style_border_width(key, 1, 0);
        lv_obj_set_style_radius(key, 6, 0);
        lv_obj_add_event_cb(key, fc_key_cb, LV_EVENT_PRESSED, reinterpret_cast<void*>(static_cast<uintptr_t>(mask)));
        lv_obj_add_event_cb(key, fc_key_cb, LV_EVENT_PRESSING, reinterpret_cast<void*>(static_cast<uintptr_t>(mask)));
        lv_obj_add_event_cb(key, fc_key_cb, LV_EVENT_RELEASED, reinterpret_cast<void*>(static_cast<uintptr_t>(mask)));
        lv_obj_add_event_cb(key, fc_key_cb, LV_EVENT_PRESS_LOST, reinterpret_cast<void*>(static_cast<uintptr_t>(mask)));

        lv_obj_t* label = label_en(key, text, &style_en);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_center(label);
        return key;
    };

    auto make_action = [&](const char* text, int x, int y, lv_event_cb_t cb, int w = 52, int h = 34) {
        lv_obj_t* key = lv_obj_create(fc_game_group_);
        lv_obj_add_style(key, &style_panel, 0);
        lv_obj_set_size(key, w, h);
        lv_obj_align(key, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_clear_flag(key, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(key, LV_COLOR_MAKE(0x1c, 0x18, 0x14), 0);
        lv_obj_set_style_bg_opa(key, LV_OPA_80, 0);
        lv_obj_set_style_border_color(key, COLOR_GOLD, 0);
        lv_obj_set_style_border_width(key, 1, 0);
        lv_obj_set_style_radius(key, 6, 0);
        lv_obj_add_event_cb(key, cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* label = label_en(key, text, &style_gold);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_center(label);
        return key;
    };

    make_key("U", 62, 242, 0x10, 44, 24);
    make_key("L", 8, 262, 0x40, 56, 50);
    make_key("D", 62, 294, 0x20, 44, 24);
    make_key("R", 108, 262, 0x80, 56, 50);

    make_action("LIST", 206, 244, fc_back_list_cb, 68, 30);
    make_key("Sel", 184, 284, 0x04, 58, 32);
    make_key("Start", 252, 284, 0x08, 72, 32);

    make_key("B", 330, 258, 0x02, 62, 54);
    make_key("A", 410, 244, 0x01, 64, 68);

    SetFcMode(false);
}

// ===== Calendar page =====
void DesktopUI::CreateCalendarPage(lv_obj_t* root) {
    calendar_page_ = lv_obj_create(root);
    lv_obj_add_style(calendar_page_, &style_screen, 0);
    lv_obj_set_size(calendar_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(calendar_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(calendar_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(calendar_page_, calendar_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(calendar_page_);

    lv_obj_set_style_bg_color(calendar_page_, LV_COLOR_MAKE(0x2a, 0x16, 0x0c), 0);

    lv_obj_t* glow = circle(calendar_page_, 146, LV_COLOR_MAKE(0xb0, 0x6c, 0x36), LV_OPA_30);
    lv_obj_align(glow, LV_ALIGN_BOTTOM_LEFT, -48, 34);

    lv_obj_t* today_card = CreatePanel(calendar_page_, 146, 252, 10, 18);
    lv_obj_set_style_bg_color(today_card, LV_COLOR_MAKE(0xff, 0xf3, 0xdb), 0);
    lv_obj_set_style_border_width(today_card, 0, 0);
    lv_obj_set_style_radius(today_card, 8, 0);

    lv_obj_t* card_sun = circle(today_card, 112, LV_COLOR_MAKE(0xff, 0xc8, 0x78), LV_OPA_30);
    lv_obj_align(card_sun, LV_ALIGN_TOP_RIGHT, 38, -32);

    lv_obj_t* card_shadow = circle(today_card, 96, LV_COLOR_MAKE(0xed, 0xa0, 0x54), LV_OPA_20);
    lv_obj_align(card_shadow, LV_ALIGN_BOTTOM_LEFT, -58, 26);

    lv_obj_t* card_today_cn = label_en(today_card, "\xE4\xBB\x8A""\xE6\x97\xA5", &style_gold);
    lv_obj_set_style_text_font(card_today_cn, &font_puhui_16_4, 0);
    lv_obj_align(card_today_cn, LV_ALIGN_TOP_LEFT, 20, 22);

    lv_obj_t* card_today_en = label_en(today_card, "TODAY", &style_muted);
    lv_obj_set_style_text_color(card_today_en, LV_COLOR_MAKE(0x55, 0x4c, 0x42), 0);
    lv_obj_align(card_today_en, LV_ALIGN_TOP_LEFT, 20, 54);

    calendar_card_day_label_ = label_en(today_card, "--", &style_en);
    lv_obj_set_style_text_color(calendar_card_day_label_, LV_COLOR_MAKE(0x20, 0x16, 0x10), 0);
    lv_obj_set_style_text_font(calendar_card_day_label_, &lv_font_montserrat_48, 0);
    lv_obj_align(calendar_card_day_label_, LV_ALIGN_TOP_MID, 0, 92);

    calendar_card_weekday_label_ = label_en(today_card, "--", &style_en);
    lv_obj_set_style_text_color(calendar_card_weekday_label_, LV_COLOR_MAKE(0x2e, 0x21, 0x18), 0);
    lv_obj_set_style_text_font(calendar_card_weekday_label_, &font_puhui_16_4, 0);
    lv_obj_align(calendar_card_weekday_label_, LV_ALIGN_TOP_LEFT, 22, 164);

    calendar_card_date_label_ = label_en(today_card, "---- / --", &style_gold);
    lv_obj_set_style_text_font(calendar_card_date_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(calendar_card_date_label_, LV_ALIGN_TOP_LEFT, 22, 198);

    lv_obj_t* panel = CreatePanel(calendar_page_, 304, 252, 166, 18);
    lv_obj_set_style_bg_color(panel, LV_COLOR_MAKE(0x18, 0x0f, 0x0a), 0);
    lv_obj_set_style_border_color(panel, LV_COLOR_MAKE(0x63, 0x43, 0x2b), 0);
    lv_obj_set_style_radius(panel, 8, 0);

    calendar_title_label_ = label_en(panel, "Month ----", &style_en);
    lv_obj_set_style_text_color(calendar_title_label_, LV_COLOR_MAKE(0xff, 0xf5, 0xe4), 0);
    lv_obj_set_style_text_font(calendar_title_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(calendar_title_label_, LV_ALIGN_TOP_LEFT, 18, 18);

    lv_obj_t* top_today = CreateButton(panel, "Today", calendar_today_cb);
    lv_obj_set_size(top_today, 76, 28);
    lv_obj_set_style_bg_color(top_today, LV_COLOR_MAKE(0xff, 0xc1, 0x70), 0);
    lv_obj_set_style_border_width(top_today, 0, 0);
    lv_obj_align(top_today, LV_ALIGN_TOP_RIGHT, -18, 18);

    calendar_today_label_ = label_en(panel, "Minimal monthly view", &style_muted);
    lv_obj_set_width(calendar_today_label_, 180);
    lv_label_set_long_mode(calendar_today_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(calendar_today_label_, LV_COLOR_MAKE(0x9a, 0x76, 0x5e), 0);
    lv_obj_set_style_text_font(calendar_today_label_, &lv_font_montserrat_14, 0);
    lv_obj_align(calendar_today_label_, LV_ALIGN_TOP_LEFT, 18, 50);

    lv_obj_t* divider = bar(panel, 268, 1, LV_COLOR_MAKE(0x6b, 0x48, 0x2e), LV_OPA_COVER);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 18, 76);

    static constexpr const char* kWeekdays[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
    for (int col = 0; col < 7; ++col) {
        lv_obj_t* label = label_en(panel, kWeekdays[col], &style_muted);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        const lv_color_t weekday_color = col >= 5 ? COLOR_GOLD : lv_color_make(0xa8, 0x86, 0x6e);
        lv_obj_set_style_text_color(label, weekday_color, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_set_width(label, 36);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14 + col * 40, 90);
    }

    for (int i = 0; i < 42; ++i) {
        const int col = i % 7;
        const int row = i / 7;
        lv_obj_t* cell = lv_obj_create(panel);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, 32, 20);
        lv_obj_set_style_radius(cell, 8, 0);
        lv_obj_set_style_bg_color(cell, LV_COLOR_MAKE(0x24, 0x18, 0x10), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_50, 0);
        lv_obj_set_style_border_color(cell, LV_COLOR_MAKE(0x5d, 0x40, 0x2b), 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_align(cell, LV_ALIGN_TOP_LEFT, 16 + col * 40, 116 + row * 22);
        add_gesture_bubble(cell);
        calendar_day_cells_[i] = cell;

        lv_obj_t* day = label_en(cell, "", &style_en);
        lv_obj_set_width(day, 32);
        lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(day, &lv_font_montserrat_14, 0);
        lv_obj_center(day);
        calendar_day_labels_[i] = day;
    }

    lv_obj_t* prev = CreateButton(calendar_page_, "Prev", calendar_prev_cb);
    lv_obj_set_size(prev, 96, 34);
    lv_obj_set_ext_click_area(prev, 12);
    lv_obj_set_style_bg_color(prev, LV_COLOR_MAKE(0x20, 0x14, 0x0d), 0);
    lv_obj_set_style_border_color(prev, LV_COLOR_MAKE(0x5b, 0x3c, 0x27), 0);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 18, 278);

    lv_obj_t* today = CreateButton(calendar_page_, "Today", calendar_today_cb);
    lv_obj_set_size(today, 96, 34);
    lv_obj_set_ext_click_area(today, 12);
    lv_obj_set_style_bg_color(today, LV_COLOR_MAKE(0xff, 0xc1, 0x70), 0);
    lv_obj_set_style_border_width(today, 0, 0);
    lv_obj_align(today, LV_ALIGN_TOP_MID, 0, 278);

    lv_obj_t* next = CreateButton(calendar_page_, "Next", calendar_next_cb);
    lv_obj_set_size(next, 96, 34);
    lv_obj_set_ext_click_area(next, 12);
    lv_obj_set_style_bg_color(next, LV_COLOR_MAKE(0x20, 0x14, 0x0d), 0);
    lv_obj_set_style_border_color(next, LV_COLOR_MAKE(0x5b, 0x3c, 0x27), 0);
    lv_obj_align(next, LV_ALIGN_TOP_RIGHT, -18, 278);

    RenderCalendar();
}

// ===== Radio page =====
void DesktopUI::CreateRadioPage(lv_obj_t* root) {
    radio_page_ = lv_obj_create(root);
    lv_obj_add_style(radio_page_, &style_screen, 0);
    lv_obj_set_size(radio_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(radio_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(radio_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(radio_page_, radio_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(radio_page_);

    // 顶部状态栏
    create_brand_mark(radio_page_);

    CreateStatusBar(radio_page_);

    // 标题
    lv_obj_t* title = label_en(radio_page_, "Radio", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    // 返回按钮
    lv_obj_t* back = CreateButton(radio_page_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    // 当前电台信息
    radio_station_label_ = label_en(radio_page_, "CNR China Voice", &style_gold);
    lv_obj_set_style_text_font(radio_station_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(radio_station_label_, LV_ALIGN_TOP_LEFT, 24, 88);

    radio_state_label_ = label_en(radio_page_, "Ready", &style_green);
    lv_obj_set_style_text_font(radio_state_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(radio_state_label_, LV_ALIGN_TOP_LEFT, 24, 118);

    radio_meta_label_ = label_en(radio_page_, "MP3 64 kbps", &style_muted);
    lv_obj_set_style_text_font(radio_meta_label_, &lv_font_montserrat_14, 0);
    lv_obj_align(radio_meta_label_, LV_ALIGN_TOP_LEFT, 24, 144);

    // 播放控制按钮
    lv_obj_t* prev = CreateButton(radio_page_, "Prev", radio_prev_cb);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 24, 180);

    lv_obj_t* play = CreateButton(radio_page_, "Play", radio_play_cb);
    lv_obj_align(play, LV_ALIGN_TOP_LEFT, 124, 180);

    lv_obj_t* stop = CreateButton(radio_page_, "Stop", radio_stop_cb);
    lv_obj_align(stop, LV_ALIGN_TOP_LEFT, 224, 180);

    lv_obj_t* next = CreateButton(radio_page_, "Next", radio_next_cb);
    lv_obj_align(next, LV_ALIGN_TOP_LEFT, 324, 180);

    // 电台数量信息
    lv_obj_t* info = label_en(radio_page_, "37 stations available", &style_muted);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 24, 230);

    // 音量动态柱
    for (int i = 0; i < 16; i++) {
        radio_bars_[i] = lv_obj_create(radio_page_);
        lv_obj_remove_style_all(radio_bars_[i]);
        lv_obj_set_width(radio_bars_[i], 20);
        lv_obj_set_height(radio_bars_[i], 5);
        lv_obj_set_style_bg_color(radio_bars_[i], RADIO_BAR_COLORS[i], 0);
        lv_obj_set_style_bg_opa(radio_bars_[i], LV_OPA_50, 0);
        lv_obj_set_style_radius(radio_bars_[i], 3, 0);
        lv_obj_align(radio_bars_[i], LV_ALIGN_BOTTOM_LEFT, 24 + i * 28, 0);
    }

    // 启动动画定时器
    radio_anim_timer_ = lv_timer_create(RadioAnimTimerCb, 100, this);

    // 提示文字
    lv_obj_t* hint = label_en(radio_page_, "Swipe right: Apps", &style_muted);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void DesktopUI::CreateFocusPage(lv_obj_t* root) {
    focus_page_ = lv_obj_create(root);
    lv_obj_add_style(focus_page_, &style_screen, 0);
    lv_obj_set_size(focus_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(focus_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(focus_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(focus_page_, focus_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(focus_page_);

    CreateStatusBar(focus_page_);

    lv_obj_t* title = label_en(focus_page_, "专注时钟", &style_en);
    lv_obj_set_style_text_font(title, &qd_font_lxgw_20, 0);
    lv_obj_set_style_text_color(title, COLOR_CREAM, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 42);

    lv_obj_t* sub = label_en(focus_page_, "Focus Timer", &style_gold);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_16, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 118, 46);
    focus_mode_label_ = sub;

    lv_obj_t* left_panel = CreatePanel(focus_page_, 104, 116, 22, 86);
    lv_obj_set_style_bg_color(left_panel, LV_COLOR_MAKE(0x11, 0x0f, 0x0c), 0);
    lv_obj_set_style_border_color(left_panel, LV_COLOR_MAKE(0x31, 0x25, 0x1b), 0);
    lv_obj_t* left_kicker = label_en(left_panel, "状态", &style_gold);
    lv_obj_set_style_text_font(left_kicker, &qd_font_lxgw_16, 0);
    lv_obj_align(left_kicker, LV_ALIGN_TOP_LEFT, 14, 14);
    lv_obj_t* left_quote = label_en(left_panel, "专注当下\n收获未来", &style_muted);
    lv_obj_set_width(left_quote, 76);
    lv_label_set_long_mode(left_quote, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(left_quote, &qd_font_lxgw_16, 0);
    lv_obj_set_style_text_line_space(left_quote, 8, 0);
    lv_obj_align(left_quote, LV_ALIGN_TOP_LEFT, 14, 48);

    focus_arc_ = lv_arc_create(focus_page_);
    lv_obj_remove_style_all(focus_arc_);
    lv_obj_set_size(focus_arc_, 172, 172);
    lv_obj_align(focus_arc_, LV_ALIGN_TOP_LEFT, 142, 70);
    lv_arc_set_rotation(focus_arc_, 270);
    lv_arc_set_bg_angles(focus_arc_, 0, 360);
    lv_arc_set_range(focus_arc_, 0, 1000);
    lv_obj_set_style_arc_width(focus_arc_, 11, LV_PART_MAIN);
    lv_obj_set_style_arc_color(focus_arc_, LV_COLOR_MAKE(0x33, 0x20, 0x15), LV_PART_MAIN);
    lv_obj_set_style_arc_width(focus_arc_, 11, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(focus_arc_, LV_COLOR_MAKE(0xff, 0x7a, 0x2d), LV_PART_INDICATOR);
    lv_obj_set_style_opa(focus_arc_, LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_t* inner = circle(focus_page_, 126, LV_COLOR_MAKE(0x0e, 0x0d, 0x0b), LV_OPA_COVER);
    lv_obj_align(inner, LV_ALIGN_TOP_LEFT, 165, 93);

    focus_time_label_ = label_en(focus_page_, "25:00", &style_en);
    lv_obj_set_width(focus_time_label_, 148);
    lv_obj_set_style_text_align(focus_time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(focus_time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(focus_time_label_, COLOR_CREAM, 0);
    lv_obj_align(focus_time_label_, LV_ALIGN_TOP_LEFT, 154, 123);

    focus_state_label_ = label_en(focus_page_, "准备开始", &style_gold);
    lv_obj_set_width(focus_state_label_, 148);
    lv_obj_set_style_text_align(focus_state_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(focus_state_label_, &qd_font_lxgw_16, 0);
    lv_obj_align(focus_state_label_, LV_ALIGN_TOP_LEFT, 154, 178);

    lv_obj_t* work_btn = lv_obj_create(focus_page_);
    lv_obj_add_style(work_btn, &style_panel, 0);
    lv_obj_set_size(work_btn, 108, 42);
    lv_obj_align(work_btn, LV_ALIGN_TOP_LEFT, 342, 82);
    lv_obj_set_style_border_color(work_btn, LV_COLOR_MAKE(0xff, 0x62, 0x2e), 0);
    lv_obj_clear_flag(work_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(work_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(work_btn, focus_work_cb, LV_EVENT_CLICKED, NULL);
    add_gesture_bubble(work_btn);
    lv_obj_t* work_label = label_en(work_btn, "专注 25 分钟", &style_en);
    lv_obj_set_style_text_font(work_label, &qd_font_lxgw_16, 0);
    lv_obj_align(work_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* break_btn = lv_obj_create(focus_page_);
    lv_obj_add_style(break_btn, &style_panel, 0);
    lv_obj_set_size(break_btn, 108, 42);
    lv_obj_align(break_btn, LV_ALIGN_TOP_LEFT, 342, 134);
    lv_obj_clear_flag(break_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(break_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(break_btn, focus_break_cb, LV_EVENT_CLICKED, NULL);
    add_gesture_bubble(break_btn);
    lv_obj_t* break_label = label_en(break_btn, "休息 5 分钟", &style_muted);
    lv_obj_set_style_text_font(break_label, &qd_font_lxgw_16, 0);
    lv_obj_align(break_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* done_panel = lv_obj_create(focus_page_);
    lv_obj_add_style(done_panel, &style_panel, 0);
    lv_obj_set_size(done_panel, 108, 54);
    lv_obj_align(done_panel, LV_ALIGN_TOP_LEFT, 342, 198);
    lv_obj_clear_flag(done_panel, LV_OBJ_FLAG_SCROLLABLE);
    add_gesture_bubble(done_panel);
    lv_obj_t* done_title = label_en(done_panel, "今日完成", &style_muted);
    lv_obj_set_style_text_font(done_title, &qd_font_lxgw_16, 0);
    lv_obj_align(done_title, LV_ALIGN_TOP_LEFT, 14, 8);
    focus_completed_label_ = label_en(done_panel, "0 个番茄", &style_gold);
    lv_obj_set_style_text_font(focus_completed_label_, &qd_font_lxgw_16, 0);
    lv_obj_align(focus_completed_label_, LV_ALIGN_TOP_LEFT, 14, 30);

    lv_obj_t* start_btn = lv_obj_create(focus_page_);
    lv_obj_add_style(start_btn, &style_panel, 0);
    lv_obj_set_size(start_btn, 132, 44);
    lv_obj_align(start_btn, LV_ALIGN_TOP_LEFT, 128, 262);
    lv_obj_set_style_bg_color(start_btn, LV_COLOR_MAKE(0xe9, 0x45, 0x19), 0);
    lv_obj_set_style_border_color(start_btn, LV_COLOR_MAKE(0xff, 0x8a, 0x32), 0);
    lv_obj_clear_flag(start_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(start_btn, focus_start_cb, LV_EVENT_CLICKED, NULL);
    add_gesture_bubble(start_btn);
    focus_start_label_ = label_en(start_btn, "开始", &style_en);
    lv_obj_set_style_text_font(focus_start_label_, &qd_font_lxgw_20, 0);
    lv_obj_align(focus_start_label_, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* reset_btn = lv_obj_create(focus_page_);
    lv_obj_add_style(reset_btn, &style_panel, 0);
    lv_obj_set_size(reset_btn, 112, 44);
    lv_obj_align(reset_btn, LV_ALIGN_TOP_LEFT, 276, 262);
    lv_obj_clear_flag(reset_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(reset_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(reset_btn, focus_reset_cb, LV_EVENT_CLICKED, NULL);
    add_gesture_bubble(reset_btn);
    lv_obj_t* reset_label = label_en(reset_btn, "重置", &style_en);
    lv_obj_set_style_text_font(reset_label, &qd_font_lxgw_20, 0);
    lv_obj_align(reset_label, LV_ALIGN_CENTER, 0, 0);

    focus_timer_ = lv_timer_create(FocusTimerCb, 1000, this);
    lv_timer_pause(focus_timer_);
    focus_completed_count_ = LoadFocusCount();
    UpdateFocusUI();
}

// ===== XiaoZhi page =====
void DesktopUI::CreateXiaozhiPage(lv_obj_t* root) {
    xiaozhi_page_ = lv_obj_create(root);
    lv_obj_add_style(xiaozhi_page_, &style_screen, 0);
    lv_obj_set_size(xiaozhi_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(xiaozhi_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(xiaozhi_page_, xiaozhi_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(xiaozhi_page_, xiaozhi_toggle_cb, LV_EVENT_CLICKED, NULL);

    // 全屏黑色背景，只有面部
    CreateFaceUI(xiaozhi_page_);
}

// ===== Network page =====
void DesktopUI::CreateNetworkPage(lv_obj_t* root) {
    network_page_ = lv_obj_create(root);
    lv_obj_add_style(network_page_, &style_screen, 0);
    lv_obj_set_size(network_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(network_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(network_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(network_page_, network_gesture_cb, LV_EVENT_GESTURE, NULL);

    create_brand_mark(network_page_);

    CreateStatusBar(network_page_);

    lv_obj_t* title = label_en(network_page_, "Network", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(network_page_, "WiFi Center", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 110, 53);

    lv_obj_t* back = CreateButton(network_page_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    lv_obj_t* status_panel = CreatePanel(network_page_, 432, 64, 24, 88);
    lv_obj_t* status_title = label_en(status_panel, "Connection", &style_gold);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_16, 0);
    lv_obj_align(status_title, LV_ALIGN_TOP_LEFT, 14, 9);

    network_detail_label_ = label_en(status_panel, "Waiting for WiFi", &style_green);
    lv_obj_set_width(network_detail_label_, 250);
    lv_label_set_long_mode(network_detail_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(network_detail_label_, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    network_saved_count_label_ = label_en(status_panel, "Saved: --", &style_muted);
    lv_obj_set_style_text_font(network_saved_count_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(network_saved_count_label_, LV_ALIGN_RIGHT_MID, -14, 0);

    lv_obj_t* list_title = label_en(network_page_, "Saved WiFi", &style_gold);
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_16, 0);
    lv_obj_align(list_title, LV_ALIGN_TOP_LEFT, 28, 164);

    lv_obj_t* list_panel = CreatePanel(network_page_, 432, 112, 24, 190);
    lv_obj_add_flag(list_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(list_panel, 6, 0);
    add_gesture_bubble(list_panel);

    network_list_container_ = lv_obj_create(list_panel);
    lv_obj_remove_style_all(network_list_container_);
    lv_obj_set_size(network_list_container_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(network_list_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(network_list_container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(network_list_container_, 5, 0);
    add_gesture_bubble(network_list_container_);
}

// ===== Settings page =====
void DesktopUI::CreateSettingsPage(lv_obj_t* root) {
    settings_page_ = lv_obj_create(root);
    lv_obj_add_style(settings_page_, &style_screen, 0);
    lv_obj_set_size(settings_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(settings_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(settings_page_, settings_gesture_cb, LV_EVENT_GESTURE, NULL);

    create_brand_mark(settings_page_);

    CreateStatusBar(settings_page_);

    lv_obj_t* title = label_en(settings_page_, "Settings", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(settings_page_, "System Configuration", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 100, 53);

    lv_obj_t* back = CreateButton(settings_page_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    settings_content_ = lv_obj_create(settings_page_);
    lv_obj_add_style(settings_content_, &style_panel, 0);
    lv_obj_set_size(settings_content_, 452, 220);
    lv_obj_align(settings_content_, LV_ALIGN_TOP_LEFT, 14, 88);
    lv_obj_set_scroll_dir(settings_content_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settings_content_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_all(settings_content_, 10, 0);
    lv_obj_set_style_pad_bottom(settings_content_, 14, 0);
    add_gesture_bubble(settings_content_);

    lv_obj_t* system_title = label_en(settings_content_, "Display & Sound", &style_gold);
    lv_obj_set_style_text_font(system_title, &lv_font_montserrat_16, 0);
    lv_obj_align(system_title, LV_ALIGN_TOP_LEFT, 4, 2);

    lv_obj_t* brightness_row = lv_obj_create(settings_content_);
    lv_obj_add_style(brightness_row, &style_panel, 0);
    lv_obj_set_size(brightness_row, 414, 58);
    lv_obj_align(brightness_row, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_clear_flag(brightness_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* brightness_label = label_en(brightness_row, "Brightness", &style_en);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 14, 8);
    settings_brightness_value_ = label_en(brightness_row, "--%", &style_gold);
    lv_obj_set_width(settings_brightness_value_, 48);
    lv_obj_set_style_text_align(settings_brightness_value_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(settings_brightness_value_, LV_ALIGN_TOP_RIGHT, -14, 8);
    settings_brightness_slider_ = lv_slider_create(brightness_row);
    lv_slider_set_range(settings_brightness_slider_, 5, 100);
    lv_obj_set_size(settings_brightness_slider_, 382, 12);
    lv_obj_align(settings_brightness_slider_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(settings_brightness_slider_, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider_, COLOR_GOLD, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_brightness_slider_, COLOR_CREAM, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_brightness_slider_, settings_brightness_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t* volume_row = lv_obj_create(settings_content_);
    lv_obj_add_style(volume_row, &style_panel, 0);
    lv_obj_set_size(volume_row, 414, 58);
    lv_obj_align(volume_row, LV_ALIGN_TOP_LEFT, 0, 94);
    lv_obj_clear_flag(volume_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* volume_label = label_en(volume_row, "Volume", &style_en);
    lv_obj_align(volume_label, LV_ALIGN_TOP_LEFT, 14, 8);
    settings_volume_value_ = label_en(volume_row, "--%", &style_green);
    lv_obj_set_width(settings_volume_value_, 48);
    lv_obj_set_style_text_align(settings_volume_value_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(settings_volume_value_, LV_ALIGN_TOP_RIGHT, -14, 8);
    settings_volume_slider_ = lv_slider_create(volume_row);
    lv_slider_set_range(settings_volume_slider_, 0, 100);
    lv_obj_set_size(settings_volume_slider_, 382, 12);
    lv_obj_align(settings_volume_slider_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(settings_volume_slider_, COLOR_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_volume_slider_, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_volume_slider_, COLOR_CREAM, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_volume_slider_, settings_volume_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t* weather_title = label_en(settings_content_, "Weather", &style_gold);
    lv_obj_set_style_text_font(weather_title, &lv_font_montserrat_16, 0);
    lv_obj_align(weather_title, LV_ALIGN_TOP_LEFT, 4, 166);

    lv_obj_t* weather_row = lv_obj_create(settings_content_);
    lv_obj_add_style(weather_row, &style_panel, 0);
    lv_obj_set_size(weather_row, 414, 58);
    lv_obj_align(weather_row, LV_ALIGN_TOP_LEFT, 0, 192);
    lv_obj_clear_flag(weather_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* weather_label = label_en(weather_row, "Location", &style_en);
    lv_obj_align(weather_label, LV_ALIGN_TOP_LEFT, 14, 8);
    lv_obj_t* weather_value = label_en(weather_row, "Configured by XiaoZhi / MCP", &style_muted);
    lv_obj_set_style_text_font(weather_value, &lv_font_montserrat_12, 0);
    lv_obj_align(weather_value, LV_ALIGN_BOTTOM_LEFT, 14, -9);

    lv_obj_t* firmware_title = label_en(settings_content_, "Firmware", &style_gold);
    lv_obj_set_style_text_font(firmware_title, &lv_font_montserrat_16, 0);
    lv_obj_align(firmware_title, LV_ALIGN_TOP_LEFT, 4, 258);

    lv_obj_t* firmware_row = lv_obj_create(settings_content_);
    lv_obj_add_style(firmware_row, &style_panel, 0);
    lv_obj_set_size(firmware_row, 414, 74);
    lv_obj_align(firmware_row, LV_ALIGN_TOP_LEFT, 0, 284);
    lv_obj_clear_flag(firmware_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* version_label = label_en(firmware_row, "Version", &style_en);
    lv_obj_align(version_label, LV_ALIGN_TOP_LEFT, 14, 9);

    const esp_app_desc_t* app_desc = esp_app_get_description();
    settings_firmware_version_label_ = label_en(firmware_row, app_desc ? app_desc->version : "--", &style_gold);
    lv_obj_set_width(settings_firmware_version_label_, 150);
    lv_label_set_long_mode(settings_firmware_version_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(settings_firmware_version_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(settings_firmware_version_label_, LV_ALIGN_TOP_RIGHT, -14, 9);

    lv_obj_t* ota_label = label_en(firmware_row, "OTA", &style_en);
    lv_obj_align(ota_label, LV_ALIGN_BOTTOM_LEFT, 14, -11);
    settings_firmware_status_label_ = label_en(firmware_row, "Tap Check", &style_muted);
    lv_obj_set_width(settings_firmware_status_label_, 176);
    lv_label_set_long_mode(settings_firmware_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(settings_firmware_status_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(settings_firmware_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(settings_firmware_status_label_, LV_ALIGN_BOTTOM_LEFT, 54, -12);

    settings_firmware_button_ = lv_btn_create(firmware_row);
    lv_obj_set_size(settings_firmware_button_, 84, 30);
    lv_obj_set_style_radius(settings_firmware_button_, 15, 0);
    lv_obj_set_style_bg_color(settings_firmware_button_, COLOR_SURFACE_2, 0);
    lv_obj_set_style_border_color(settings_firmware_button_, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(settings_firmware_button_, 1, 0);
    lv_obj_add_event_cb(settings_firmware_button_, settings_firmware_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(settings_firmware_button_, LV_ALIGN_BOTTOM_RIGHT, -14, -8);
    settings_firmware_button_label_ = label_en(settings_firmware_button_, "Check", &style_en);
    lv_obj_set_style_text_font(settings_firmware_button_label_, &lv_font_montserrat_12, 0);
    lv_obj_center(settings_firmware_button_label_);
}

void DesktopUI::UpdateWifiList() {
    if (!network_list_container_) return;

    lv_obj_clean(network_list_container_);

    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();

    if (network_saved_count_label_) {
        char count_text[24];
        snprintf(count_text, sizeof(count_text), "Saved: %u", static_cast<unsigned>(ssid_list.size()));
        lv_label_set_text(network_saved_count_label_, count_text);
    }

    if (ssid_list.empty()) {
        lv_obj_t* empty_label = label_en(network_list_container_, "No saved WiFi networks", &style_muted);
        lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty_label, LV_PCT(100));
        return;
    }

    for (size_t i = 0; i < ssid_list.size(); i++) {
        lv_obj_t* item = lv_obj_create(network_list_container_);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(item, COLOR_SURFACE_2, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_pad_hor(item, 16, 0);
        lv_obj_set_style_pad_ver(item, 8, 0);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(item, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(item, network_wifi_item_cb, LV_EVENT_CLICKED, NULL);
        add_gesture_bubble(item);

        lv_obj_t* ssid_label = label_en(item, ssid_list[i].ssid.c_str(), &style_en);
        lv_obj_set_width(ssid_label, 250);
        lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(ssid_label, 1);

        char index_str[16];
        snprintf(index_str, sizeof(index_str), "#%d", (int)i + 1);
        label_en(item, index_str, &style_muted);

        if (i == 0) {
            lv_obj_t* default_label = label_en(item, "Default", &style_green);
            lv_obj_set_style_text_font(default_label, &lv_font_montserrat_12, 0);
        } else {
            lv_obj_t* set_label = label_en(item, "Set", &style_muted);
            lv_obj_set_style_text_font(set_label, &lv_font_montserrat_12, 0);
        }
    }
}

void DesktopUI::CreateFaceUI(lv_obj_t* parent) {
    // 全屏黑色背景，直接在parent上创建元素

    // 左眼白
    eye_left_ = lv_obj_create(parent);
    lv_obj_set_size(eye_left_, 60, 70);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -80, -40);
    lv_obj_set_style_radius(eye_left_, 30, 0);
    lv_obj_set_style_bg_color(eye_left_, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(eye_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_left_, 0, 0);
    add_gesture_bubble(eye_left_);

    // 左瞳孔
    pupil_left_ = lv_obj_create(eye_left_);
    lv_obj_set_size(pupil_left_, 28, 35);
    lv_obj_align(pupil_left_, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_radius(pupil_left_, 14, 0);
    lv_obj_set_style_bg_color(pupil_left_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pupil_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_left_, 0, 0);

    // 左眼高光
    highlight_left_ = lv_obj_create(eye_left_);
    lv_obj_set_size(highlight_left_, 10, 10);
    lv_obj_align(highlight_left_, LV_ALIGN_CENTER, -8, -10);
    lv_obj_set_style_radius(highlight_left_, 5, 0);
    lv_obj_set_style_bg_color(highlight_left_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(highlight_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(highlight_left_, 0, 0);

    // 右眼白
    eye_right_ = lv_obj_create(parent);
    lv_obj_set_size(eye_right_, 60, 70);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, 80, -40);
    lv_obj_set_style_radius(eye_right_, 30, 0);
    lv_obj_set_style_bg_color(eye_right_, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(eye_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_right_, 0, 0);
    add_gesture_bubble(eye_right_);

    // 右瞳孔
    pupil_right_ = lv_obj_create(eye_right_);
    lv_obj_set_size(pupil_right_, 28, 35);
    lv_obj_align(pupil_right_, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_radius(pupil_right_, 14, 0);
    lv_obj_set_style_bg_color(pupil_right_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pupil_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_right_, 0, 0);

    // 右眼高光
    highlight_right_ = lv_obj_create(eye_right_);
    lv_obj_set_size(highlight_right_, 10, 10);
    lv_obj_align(highlight_right_, LV_ALIGN_CENTER, -8, -10);
    lv_obj_set_style_radius(highlight_right_, 5, 0);
    lv_obj_set_style_bg_color(highlight_right_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(highlight_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(highlight_right_, 0, 0);

    // 左眉毛
    eyebrow_left_ = lv_obj_create(parent);
    lv_obj_set_size(eyebrow_left_, 70, 10);
    lv_obj_align(eyebrow_left_, LV_ALIGN_CENTER, -80, -90);
    lv_obj_set_style_radius(eyebrow_left_, 5, 0);
    lv_obj_set_style_bg_color(eyebrow_left_, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(eyebrow_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eyebrow_left_, 0, 0);
    add_gesture_bubble(eyebrow_left_);

    // 右眉毛
    eyebrow_right_ = lv_obj_create(parent);
    lv_obj_set_size(eyebrow_right_, 70, 10);
    lv_obj_align(eyebrow_right_, LV_ALIGN_CENTER, 80, -90);
    lv_obj_set_style_radius(eyebrow_right_, 5, 0);
    lv_obj_set_style_bg_color(eyebrow_right_, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(eyebrow_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eyebrow_right_, 0, 0);
    add_gesture_bubble(eyebrow_right_);

    // 嘴巴
    mouth_ = lv_obj_create(parent);
    lv_obj_set_size(mouth_, 80, 20);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_radius(mouth_, 10, 0);
    lv_obj_set_style_bg_color(mouth_, COLOR_GOLD, 0);
    lv_obj_set_style_bg_opa(mouth_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);
    add_gesture_bubble(mouth_);
}

// ===== Helper functions =====
lv_obj_t* DesktopUI::CreateButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 76, 32);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_bg_color(btn, COLOR_SURFACE_2, 0);
    lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }
    // 不为按钮添加手势冒泡，确保点击事件正常工作

    lv_obj_t* txt = label_en(btn, text, &style_en);
    lv_obj_center(txt);
    return btn;
}

lv_obj_t* DesktopUI::CreatePanel(lv_obj_t* parent, int16_t w, int16_t h, int16_t x, int16_t y) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, w, h);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    add_gesture_bubble(panel);
    return panel;
}

void DesktopUI::CreateStatusBar(lv_obj_t* parent) {
    lv_obj_t* wifi = label_en(parent, "WiFi", &style_green);
    lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -168, 12);

    lv_obj_t* time = label_en(parent, "--:--", &style_en);
    lv_obj_set_style_text_font(time, &lv_font_montserrat_20, 0);
    lv_obj_align(time, LV_ALIGN_TOP_RIGHT, -86, 9);
    for (size_t i = 0; i < sizeof(status_bar_time_labels_) / sizeof(status_bar_time_labels_[0]); ++i) {
        if (!status_bar_time_labels_[i]) {
            status_bar_time_labels_[i] = time;
            break;
        }
    }

    lv_obj_t* battery = label_en(parent, "--%", &style_green);
    lv_obj_align(battery, LV_ALIGN_TOP_RIGHT, -20, 12);
    for (size_t i = 0; i < sizeof(status_bar_battery_labels_) / sizeof(status_bar_battery_labels_[0]); ++i) {
        if (!status_bar_battery_labels_[i]) {
            status_bar_battery_labels_[i] = battery;
            break;
        }
    }
    SetBatteryStatus(battery_level_, battery_charging_, battery_level_ >= 0);
}

void DesktopUI::AdjustCalendarMonth(int delta) {
    if (calendar_year_ <= 0 || calendar_month_ <= 0) {
        ShowTodayCalendar();
        return;
    }

    calendar_month_ += delta;
    while (calendar_month_ < 1) {
        calendar_month_ += 12;
        calendar_year_--;
    }
    while (calendar_month_ > 12) {
        calendar_month_ -= 12;
        calendar_year_++;
    }
    calendar_follow_today_ = false;
    RenderCalendar();
    ESP_LOGI(TAG, "Calendar month changed year=%d month=%d", calendar_year_, calendar_month_);
}

void DesktopUI::ShowTodayCalendar() {
    if (current_year_ <= 0 || current_month_ <= 0) {
        ESP_LOGW(TAG, "Calendar today requested before time sync");
        return;
    }
    calendar_year_ = current_year_;
    calendar_month_ = current_month_;
    calendar_follow_today_ = true;
    RenderCalendar();
    ESP_LOGI(TAG, "Calendar returned to today year=%d month=%d day=%d",
             current_year_, current_month_, current_day_);
}

void DesktopUI::RenderCalendar() {
    if (!calendar_title_label_ || !calendar_today_label_) {
        return;
    }

    if (calendar_year_ <= 0 || calendar_month_ <= 0) {
        lv_label_set_text(calendar_title_label_, "Month ----");
        lv_label_set_text(calendar_today_label_, "Waiting for time sync");
        if (calendar_card_day_label_) {
            lv_label_set_text(calendar_card_day_label_, "--");
        }
        if (calendar_card_weekday_label_) {
            lv_label_set_text(calendar_card_weekday_label_, "--");
        }
        if (calendar_card_date_label_) {
            lv_label_set_text(calendar_card_date_label_, "---- / --");
        }
        for (int i = 0; i < 42; ++i) {
            if (calendar_day_labels_[i]) {
                lv_label_set_text(calendar_day_labels_[i], "");
            }
            if (calendar_day_cells_[i]) {
                lv_obj_set_style_bg_opa(calendar_day_cells_[i], LV_OPA_20, 0);
                lv_obj_set_style_border_color(calendar_day_cells_[i], LV_COLOR_MAKE(0x5d, 0x40, 0x2b), 0);
            }
        }
        return;
    }

    char title[32];
    snprintf(title, sizeof(title), "%s %04d", month_name(calendar_month_), calendar_year_);
    lv_label_set_text(calendar_title_label_, title);

    lv_label_set_text(calendar_today_label_, "Minimal monthly view");

    char today_day[8];
    char today_date[24];
    if (current_year_ > 0) {
        snprintf(today_day, sizeof(today_day), "%d", current_day_);
        snprintf(today_date, sizeof(today_date), "%04d / %02d", current_year_, current_month_);
        if (calendar_card_day_label_) {
            lv_label_set_text(calendar_card_day_label_, today_day);
        }
        if (calendar_card_weekday_label_) {
            lv_label_set_text(calendar_card_weekday_label_, chinese_weekday_for_date(current_year_, current_month_, current_day_));
        }
        if (calendar_card_date_label_) {
            lv_label_set_text(calendar_card_date_label_, today_date);
        }
    } else {
        if (calendar_card_day_label_) {
            lv_label_set_text(calendar_card_day_label_, "--");
        }
        if (calendar_card_weekday_label_) {
            lv_label_set_text(calendar_card_weekday_label_, "--");
        }
        if (calendar_card_date_label_) {
            lv_label_set_text(calendar_card_date_label_, "---- / --");
        }
    }

    const int first = first_weekday_monday_index(calendar_year_, calendar_month_);
    const int days = days_in_month(calendar_year_, calendar_month_);
    const int prev_month = calendar_month_ == 1 ? 12 : calendar_month_ - 1;
    const int prev_year = calendar_month_ == 1 ? calendar_year_ - 1 : calendar_year_;
    const int prev_days = days_in_month(prev_year, prev_month);

    for (int i = 0; i < 42; ++i) {
        if (!calendar_day_labels_[i] || !calendar_day_cells_[i]) {
            continue;
        }

        int day = 0;
        bool in_current_month = false;
        if (i < first) {
            day = prev_days - first + i + 1;
        } else if (i < first + days) {
            day = i - first + 1;
            in_current_month = true;
        } else {
            day = i - first - days + 1;
        }

        char text[16];
        snprintf(text, sizeof(text), "%d", day);
        lv_label_set_text(calendar_day_labels_[i], text);

        const bool is_today = in_current_month &&
                              calendar_year_ == current_year_ &&
                              calendar_month_ == current_month_ &&
                              day == current_day_;
        const int col = i % 7;
        const bool weekend = col >= 5;

        lv_color_t bg_color = lv_color_make(0x17, 0x0f, 0x0a);
        lv_color_t border_color = lv_color_make(0x35, 0x26, 0x1c);
        lv_color_t text_color = lv_color_make(0x72, 0x58, 0x44);
        lv_opa_t bg_opa = LV_OPA_30;
        if (is_today) {
            bg_color = lv_color_make(0xff, 0xc1, 0x70);
            border_color = bg_color;
            text_color = lv_color_make(0x20, 0x16, 0x10);
            bg_opa = LV_OPA_COVER;
        } else if (weekend && in_current_month) {
            bg_color = lv_color_make(0xe7, 0x91, 0x42);
            border_color = bg_color;
            text_color = lv_color_make(0xff, 0xd0, 0x94);
            bg_opa = LV_OPA_COVER;
        } else if (in_current_month) {
            bg_color = lv_color_make(0x24, 0x18, 0x10);
            border_color = lv_color_make(0x5d, 0x40, 0x2b);
            text_color = lv_color_make(0xff, 0xf5, 0xe4);
            bg_opa = LV_OPA_70;
        }

        lv_obj_set_style_bg_color(calendar_day_cells_[i], bg_color, 0);
        lv_obj_set_style_bg_opa(calendar_day_cells_[i], bg_opa, 0);
        lv_obj_set_style_border_color(calendar_day_cells_[i], border_color, 0);
        lv_obj_set_style_text_color(calendar_day_labels_[i], text_color, 0);
    }
}

// ===== Time rendering =====
void DesktopUI::FlipDigit(uint8_t index, uint8_t digit, bool animate) {
    (void)index;
    (void)digit;
    (void)animate;
}

void DesktopUI::RenderBigTime(int hour, int minute, bool animate) {
    (void)animate;
    if (!clock_hour_label_ || !clock_minute_label_) {
        return;
    }
    char hour_text[3];
    char minute_text[3];
    snprintf(hour_text, sizeof(hour_text), "%02d", hour);
    snprintf(minute_text, sizeof(minute_text), "%02d", minute);
    lv_label_set_text(clock_hour_label_, hour_text);
    lv_label_set_text(clock_minute_label_, minute_text);
}

// ===== Face animation =====
void DesktopUI::SetFacePart(lv_obj_t* obj, int x, int y, int w, int h, int radius) {
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_pos(obj, x, y);
}

void DesktopUI::UpdateFaceAnimation() {
    if (!eye_left_ || !eye_right_ || !mouth_) return;

    const DeviceState state = Application::GetInstance().GetDeviceState();
    const bool speaking = state == kDeviceStateSpeaking;
    const bool listening = state == kDeviceStateListening || state == kDeviceStateConnecting;

    // ===== 眨眼动画 =====
    int eye_h = 70;
    if (speaking) {
        eye_h = 65 + (int)(5 * sin(anim_tick_ * 0.2));
    } else if (listening) {
        eye_h = 75;
    } else {
        const int blink_phase = anim_tick_ % 60;
        if (blink_phase >= 55) {
            eye_h = 10;
        }
    }

    lv_obj_set_height(eye_left_, eye_h);
    lv_obj_set_height(eye_right_, eye_h);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -80, -40);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, 80, -40);

    // ===== 瞳孔随机移动 =====
    if (anim_tick_ % 40 == 0) {
        pupil_target_x_ = (float)((rand() % 12) - 6);
        pupil_target_y_ = (float)((rand() % 8) - 4);
    }
    pupil_offset_x_ += (pupil_target_x_ - pupil_offset_x_) * 0.08f;
    pupil_offset_y_ += (pupil_target_y_ - pupil_offset_y_) * 0.08f;

    if (pupil_left_) {
        lv_obj_align(pupil_left_, LV_ALIGN_CENTER, (int)pupil_offset_x_, 4 + (int)pupil_offset_y_);
    }
    if (pupil_right_) {
        lv_obj_align(pupil_right_, LV_ALIGN_CENTER, (int)pupil_offset_x_, 4 + (int)pupil_offset_y_);
    }
    if (highlight_left_) {
        lv_obj_align(highlight_left_, LV_ALIGN_CENTER, -8 + (int)pupil_offset_x_, -10 + (int)pupil_offset_y_);
    }
    if (highlight_right_) {
        lv_obj_align(highlight_right_, LV_ALIGN_CENTER, -8 + (int)pupil_offset_x_, -10 + (int)pupil_offset_y_);
    }

    // ===== 眉毛动画 =====
    int eyebrow_y = -90;
    if (speaking) {
        eyebrow_y = -92 + (int)(3 * sin(anim_tick_ * 0.15));
    } else if (listening) {
        eyebrow_y = -95;
    } else if (strcmp(emotion_, "sad") == 0) {
        eyebrow_y = -82;
    }

    if (eyebrow_left_) {
        lv_obj_align(eyebrow_left_, LV_ALIGN_CENTER, -80, eyebrow_y);
    }
    if (eyebrow_right_) {
        lv_obj_align(eyebrow_right_, LV_ALIGN_CENTER, 80, eyebrow_y);
    }

    // ===== 嘴巴动画 =====
    if (speaking) {
        static constexpr int mouth_heights[] = {20, 35, 28, 40, 24, 32};
        const int phase = anim_tick_ % 6;
        lv_obj_set_height(mouth_, mouth_heights[phase]);
        lv_obj_set_width(mouth_, 80 + (int)(15 * sin(anim_tick_ * 0.3)));
        lv_obj_set_style_bg_color(mouth_, COLOR_GREEN, 0);
        lv_obj_set_style_radius(mouth_, mouth_heights[phase] / 3, 0);
    } else if (listening) {
        lv_obj_set_height(mouth_, 25);
        lv_obj_set_width(mouth_, 70);
        lv_obj_set_style_bg_color(mouth_, COLOR_GOLD, 0);
        lv_obj_set_style_radius(mouth_, 12, 0);
    } else {
        lv_obj_set_height(mouth_, 16);
        lv_obj_set_width(mouth_, 60);
        lv_obj_set_style_bg_color(mouth_, COLOR_GOLD, 0);
        lv_obj_set_style_radius(mouth_, 8, 0);
    }
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 60);
}

// ===== Public API =====
void DesktopUI::SetTime(int hour, int minute, int year, int month, int day, const char* weekday) {
    if (!date_label_ || !week_label_) return;

    char date_text[24];
    char time_text[8];
    snprintf(date_text, sizeof(date_text), "%02d / %02d     |", month, day);
    snprintf(time_text, sizeof(time_text), "%02d:%02d", hour, minute);

    const bool minute_changed = hour != current_hour_ || minute != current_minute_;
    const bool date_changed = year != current_year_ || month != current_month_ || day != current_day_;
    const bool force_main_clock = current_page_ == DesktopPage::MAIN;
    if (!minute_changed && !date_changed && !force_main_clock) {
        return;
    }
    if (minute_changed || date_changed) {
        ESP_LOGI(TAG, "UI time set %04d-%02d-%02d %02d:%02d %s",
                 year, month, day, hour, minute, weekday ? weekday : "---");
    }
    current_hour_ = hour;
    current_minute_ = minute;
    current_year_ = year;
    current_month_ = month;
    current_day_ = day;
    if (calendar_year_ <= 0 || calendar_month_ <= 0 || calendar_follow_today_) {
        calendar_year_ = year;
        calendar_month_ = month;
        calendar_follow_today_ = true;
    }

    if (minute_changed || force_main_clock) {
        RenderBigTime(hour, minute, false);
        for (size_t i = 0; i < sizeof(status_bar_time_labels_) / sizeof(status_bar_time_labels_[0]); ++i) {
            if (status_bar_time_labels_[i]) {
                lv_label_set_text(status_bar_time_labels_[i], time_text);
            }
        }
    }

    if (date_changed) {
        lv_label_set_text(date_label_, date_text);
        lv_label_set_text(week_label_, weekday ? weekday : "---");
    }
    if (date_changed && calendar_app_status_label_) {
        char app_status[24];
        snprintf(app_status, sizeof(app_status), "%04d/%02d/%02d", year, month, day);
        lv_label_set_text(calendar_app_status_label_, app_status);
    }

    if (date_changed) {
        RenderCalendar();
    }
    if (current_page_ == DesktopPage::MAIN && main_page_) {
        lv_obj_invalidate(main_page_);
    }
}

void DesktopUI::SetMainPageCallback(std::function<void()> callback) {
    main_page_callback_ = std::move(callback);
}

void DesktopUI::SetPhotoActiveCallback(std::function<void(bool)> callback) {
    photo_active_callback_ = std::move(callback);
}

void DesktopUI::SetPhotoRefreshCallback(std::function<void()> callback) {
    photo_refresh_callback_ = std::move(callback);
}

void DesktopUI::RequestPhotoRefresh() {
    SetPhotoState("Refreshing", "Scanning SD card");
    if (photo_refresh_callback_) {
        photo_refresh_callback_();
    }
}

void DesktopUI::SetPhotoState(const char* title, const char* detail) {
    (void)title;
    (void)detail;
    if (photo_title_label_) {
        lv_obj_add_flag(photo_title_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (photo_detail_label_) {
        lv_obj_add_flag(photo_detail_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

void DesktopUI::SetPhotoFrame(const lv_img_dsc_t* image, const char* title, const char* detail) {
    SetPhotoState(title, detail);
    if (!photo_image_a_ || !photo_image_b_ || !image) {
        if (photo_image_a_) {
            lv_obj_set_style_opa(photo_image_a_, LV_OPA_TRANSP, 0);
        }
        if (photo_image_b_) {
            lv_obj_set_style_opa(photo_image_b_, LV_OPA_TRANSP, 0);
        }
        return;
    }

    lv_obj_t* next = photo_show_a_ ? photo_image_b_ : photo_image_a_;
    lv_obj_t* prev = photo_show_a_ ? photo_image_a_ : photo_image_b_;
    photo_show_a_ = !photo_show_a_;

    if (photo_title_label_) {
        lv_obj_add_flag(photo_title_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (photo_detail_label_) {
        lv_obj_add_flag(photo_detail_label_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_image_set_src(next, image);
    int32_t scale_x = image->header.w > 0 ? (DISPLAY_WIDTH * 256) / image->header.w : 256;
    int32_t scale_y = image->header.h > 0 ? (DISPLAY_HEIGHT * 256) / image->header.h : 256;
    int32_t scale = LV_MAX(scale_x, scale_y);
    if (scale <= 0) {
        scale = 256;
    }
    lv_image_set_scale(next, scale);
    lv_obj_center(next);
    lv_obj_set_style_opa(next, LV_OPA_TRANSP, 0);

    lv_anim_t fade_in;
    lv_anim_init(&fade_in);
    lv_anim_set_var(&fade_in, next);
    lv_anim_set_values(&fade_in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&fade_in, 650);
    lv_anim_set_exec_cb(&fade_in, ObjOpaCb);
    lv_anim_start(&fade_in);

    lv_anim_t fade_out;
    lv_anim_init(&fade_out);
    lv_anim_set_var(&fade_out, prev);
    lv_anim_set_values(&fade_out, lv_obj_get_style_opa(prev, 0), LV_OPA_TRANSP);
    lv_anim_set_time(&fade_out, 650);
    lv_anim_set_exec_cb(&fade_out, ObjOpaCb);
    lv_anim_start(&fade_out);
}

void DesktopUI::SetFcActiveCallback(std::function<void(bool)> callback) {
    fc_active_callback_ = std::move(callback);
}

void DesktopUI::SetFcExitCallback(std::function<void()> callback) {
    fc_exit_callback_ = std::move(callback);
}

void DesktopUI::SetFcActions(std::function<void()> play_pause, std::function<void()> stop,
                             std::function<void()> next, std::function<void()> prev) {
    fc_play_pause_ = std::move(play_pause);
    fc_stop_ = std::move(stop);
    fc_next_ = std::move(next);
    fc_prev_ = std::move(prev);
}

void DesktopUI::SetFcState(const char* title, const char* detail, const char* rom_list) {
    if (fc_title_label_ && title) {
        lv_label_set_text(fc_title_label_, title);
    }
    if (fc_detail_label_ && detail) {
        lv_label_set_text(fc_detail_label_, detail);
    }
    if (fc_list_label_ && rom_list) {
        lv_label_set_text(fc_list_label_, rom_list);
    }
}

void DesktopUI::SetFcMode(bool playing) {
    fc_playing_view_ = playing;
    fc_list_touch_latched_ = false;
    if (fc_list_group_) {
        if (playing) {
            lv_obj_add_flag(fc_list_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(fc_list_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (fc_game_group_) {
        if (playing) {
            lv_obj_clear_flag(fc_game_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(fc_game_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void DesktopUI::SetFcFrame(const lv_img_dsc_t* image) {
    if (!fc_playing_view_ || !fc_screen_image_ || !image) {
        return;
    }
    lv_image_set_src(fc_screen_image_, image);
    int32_t scale = 256;
    if (!(image->header.w == 256 && image->header.h == 240)) {
        int32_t scale_x = image->header.w > 0 ? (480 * 256) / image->header.w : 256;
        int32_t scale_y = image->header.h > 0 ? (240 * 256) / image->header.h : 256;
        scale = LV_MIN(scale_x, scale_y);
    }
    if (scale <= 0) {
        scale = 256;
    }
    lv_image_set_scale(fc_screen_image_, scale);
    lv_obj_center(fc_screen_image_);
}

void DesktopUI::SetFcControllerCallback(std::function<void(uint8_t)> callback) {
    fc_controller_cb_ = std::move(callback);
}

void DesktopUI::ApplyWeatherVisual(int weather_code) {
    const bool is_clear = weather_code == 0;
    const bool is_cloud = weather_code == 1 || weather_code == 2 || weather_code == 3 ||
                          weather_code == 45 || weather_code == 48 || weather_code < 0;
    const bool is_rain = (weather_code >= 51 && weather_code <= 67) ||
                         (weather_code >= 80 && weather_code <= 82);
    const bool is_snow = weather_code >= 71 && weather_code <= 77;
    const bool is_storm = weather_code >= 95;

    set_weather_part_visible(weather_sun_, is_clear || weather_code == 1 || weather_code == 2);
    if (weather_sun_) {
        lv_obj_set_style_opa(weather_sun_, is_clear ? LV_OPA_COVER : LV_OPA_50, 0);
    }

    const bool show_cloud = is_cloud || is_rain || is_snow || is_storm;
    for (auto* cloud : weather_cloud_) {
        set_weather_part_visible(cloud, show_cloud);
    }

    for (auto* rain : weather_rain_) {
        set_weather_part_visible(rain, is_rain || is_storm);
    }
    for (auto* snow : weather_snow_) {
        set_weather_part_visible(snow, is_snow);
    }
    for (auto* storm : weather_storm_) {
        set_weather_part_visible(storm, is_storm);
    }
    
    // Start/stop particle animation based on weather
    if (weather_particle_timer_) {
        if (is_clear || is_cloud) {
            lv_timer_resume(weather_particle_timer_);
        } else {
            lv_timer_pause(weather_particle_timer_);
        }
    }

    ESP_LOGI(TAG, "Weather visual code=%d clear=%d cloud=%d rain=%d snow=%d storm=%d",
             weather_code, is_clear, show_cloud, is_rain, is_snow, is_storm);
}

void DesktopUI::SetWeather(const char* temperature, const char* summary, int weather_code) {
    if (!weather_temp_label_ || !weather_meta_label_) return;
    lv_label_set_text(weather_temp_label_, temperature ? temperature : "-- C");
    lv_label_set_text(weather_meta_label_, summary ? summary : "Weather pending");
    ApplyWeatherVisual(weather_code);
    if (current_page_ == DesktopPage::MAIN && main_page_) {
        lv_obj_invalidate(main_page_);
    }
}

void DesktopUI::SetDailyQuote(const char* quote) {
    if (!quote_label_ || !quote) return;
    lv_label_set_text(quote_label_, quote);
}

void DesktopUI::SetDailyCard(const char* date, const char* title, const char* body) {
    if (daily_card_date_label_ && date) {
        lv_label_set_text(daily_card_date_label_, date);
    }
    if (daily_card_title_label_ && title) {
        lv_label_set_text(daily_card_title_label_, title);
    }
    if (quote_label_ && body) {
        lv_label_set_text(quote_label_, body);
    }
}

void DesktopUI::SetDefaultNetwork(size_t index) {
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    if (index >= ssid_list.size()) {
        ESP_LOGW(TAG, "Default WiFi index out of range: %u", static_cast<unsigned>(index));
        return;
    }

    ssid_manager.SetDefaultSsid(static_cast<int>(index));
    ESP_LOGI(TAG, "Default WiFi updated: %s", ssid_list[index].ssid.c_str());
    if (network_detail_label_) {
        lv_label_set_text(network_detail_label_, "Default WiFi updated");
    }
    UpdateWifiList();
}

void DesktopUI::SetSystemBrightness(int value) {
    value = LV_CLAMP(5, value, 100);
    auto* backlight = Board::GetInstance().GetBacklight();
    if (!backlight) {
        ESP_LOGW(TAG, "Brightness change ignored: no backlight");
        return;
    }
    backlight->SetBrightness(static_cast<uint8_t>(value), true);
    ESP_LOGI(TAG, "Settings brightness=%d", value);
    RefreshSettingsControls();
}

void DesktopUI::SetSystemVolume(int value) {
    value = LV_CLAMP(0, value, 100);
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        ESP_LOGW(TAG, "Volume change ignored: no audio codec");
        return;
    }
    codec->SetOutputVolume(value);
    ESP_LOGI(TAG, "Settings volume=%d", value);
    RefreshSettingsControls();
}

void DesktopUI::RefreshSettingsControls() {
    char value_text[16];

    if (settings_brightness_slider_ && settings_brightness_value_) {
        auto* backlight = Board::GetInstance().GetBacklight();
        const int brightness = backlight ? backlight->brightness() : 100;
        lv_slider_set_value(settings_brightness_slider_, brightness, LV_ANIM_OFF);
        snprintf(value_text, sizeof(value_text), "%d%%", brightness);
        lv_label_set_text(settings_brightness_value_, value_text);
    }

    if (settings_volume_slider_ && settings_volume_value_) {
        auto* codec = Board::GetInstance().GetAudioCodec();
        const int volume = codec ? codec->output_volume() : 70;
        lv_slider_set_value(settings_volume_slider_, volume, LV_ANIM_OFF);
        snprintf(value_text, sizeof(value_text), "%d%%", volume);
        lv_label_set_text(settings_volume_value_, value_text);
    }

    if (settings_firmware_version_label_) {
        const esp_app_desc_t* app_desc = esp_app_get_description();
        lv_label_set_text(settings_firmware_version_label_, app_desc ? app_desc->version : "--");
    }
}

bool DesktopUI::HandleSettingsSliderRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x) {
    auto apply_slider = [this, start_x, start_y, end_x](lv_obj_t* slider, int min_value, int max_value,
                                                        const std::function<void(int)>& apply) {
        if (!slider) {
            return false;
        }

        lv_area_t area;
        lv_obj_get_coords(slider, &area);
        lv_area_t content_area;
        lv_obj_get_coords(settings_content_, &content_area);
        constexpr int16_t touch_padding = 16;
        if (start_x < content_area.x1 || start_x > content_area.x2 ||
            start_y < content_area.y1 || start_y > content_area.y2) {
            return false;
        }
        if (start_x < area.x1 - touch_padding || start_x > area.x2 + touch_padding ||
            start_y < area.y1 - touch_padding || start_y > area.y2 + touch_padding) {
            return false;
        }

        const int32_t width = area.x2 > area.x1 ? area.x2 - area.x1 : 1;
        const int32_t clamped_x = LV_CLAMP(area.x1, static_cast<int32_t>(end_x), area.x2);
        const int value = min_value + (clamped_x - area.x1) * (max_value - min_value) / width;
        apply(value);
        return true;
    };

    if (apply_slider(settings_brightness_slider_, 5, 100,
                     [this](int value) { SetSystemBrightness(value); })) {
        return true;
    }
    return apply_slider(settings_volume_slider_, 0, 100,
                        [this](int value) { SetSystemVolume(value); });
}

void DesktopUI::ToggleFocusTimer() {
    if (focus_remaining_sec_ == 0) {
        ResetFocusTimer();
    }
    focus_running_ = !focus_running_;
    if (focus_timer_) {
        if (focus_running_) {
            lv_timer_resume(focus_timer_);
        } else {
            lv_timer_pause(focus_timer_);
        }
    }
    ESP_LOGI(TAG, "Focus timer %s mode=%s remaining=%lu",
             focus_running_ ? "start" : "pause", focus_is_work_ ? "work" : "break",
             static_cast<unsigned long>(focus_remaining_sec_));
    UpdateFocusUI();
}

void DesktopUI::ResetFocusTimer() {
    focus_running_ = false;
    focus_total_sec_ = focus_is_work_ ? 25 * 60 : 5 * 60;
    focus_remaining_sec_ = focus_total_sec_;
    if (focus_timer_) {
        lv_timer_pause(focus_timer_);
    }
    ESP_LOGI(TAG, "Focus timer reset mode=%s", focus_is_work_ ? "work" : "break");
    UpdateFocusUI();
}

void DesktopUI::SetFocusMode(bool work_mode) {
    if (focus_is_work_ == work_mode && focus_remaining_sec_ == focus_total_sec_) {
        UpdateFocusUI();
        return;
    }
    focus_is_work_ = work_mode;
    ResetFocusTimer();
    ESP_LOGI(TAG, "Focus mode changed to %s", focus_is_work_ ? "work" : "break");
}

void DesktopUI::UpdateFocusUI() {
    if (!focus_time_label_) {
        return;
    }

    char time_text[16];
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu",
             static_cast<unsigned long>(focus_remaining_sec_ / 60),
             static_cast<unsigned long>(focus_remaining_sec_ % 60));
    lv_label_set_text(focus_time_label_, time_text);

    const int32_t progress = focus_total_sec_ == 0
        ? 0
        : static_cast<int32_t>((focus_remaining_sec_ * 1000) / focus_total_sec_);
    if (focus_arc_) {
        lv_arc_set_value(focus_arc_, progress);
        const lv_color_t arc_color = focus_is_work_
            ? lv_color_make(0xff, 0x7a, 0x2d)
            : lv_color_make(0x92, 0xd7, 0xff);
        lv_obj_set_style_arc_color(focus_arc_, arc_color, LV_PART_INDICATOR);
    }

    if (focus_state_label_) {
        const char* state = focus_remaining_sec_ == 0
            ? (focus_is_work_ ? "专注完成" : "休息完成")
            : (focus_running_ ? (focus_is_work_ ? "专注中" : "休息中") : "准备开始");
        lv_label_set_text(focus_state_label_, state);
    }
    if (focus_mode_label_) {
        lv_label_set_text(focus_mode_label_, focus_is_work_ ? "Focus Timer" : "Break Timer");
    }
    if (focus_start_label_) {
        lv_label_set_text(focus_start_label_, focus_running_ ? "暂停" : "开始");
    }
    if (focus_completed_label_) {
        char done_text[32];
        snprintf(done_text, sizeof(done_text), "%u 个番茄", focus_completed_count_);
        lv_label_set_text(focus_completed_label_, done_text);
    }
}

void DesktopUI::SetNetworkStatus(const char* status) {
    if (!status) return;
    if (network_status_label_) {
        lv_label_set_text(network_status_label_, status);
    }
    if (network_detail_label_) {
        lv_label_set_text(network_detail_label_, status);
    }
}

void DesktopUI::SetBatteryStatus(int level, bool charging, bool valid) {
    battery_level_ = valid ? std::max(0, std::min(100, level)) : -1;
    battery_charging_ = charging;

    char text[12];
    if (battery_level_ < 0) {
        snprintf(text, sizeof(text), "--%%");
    } else if (charging) {
        snprintf(text, sizeof(text), "%d%%+", battery_level_);
    } else {
        snprintf(text, sizeof(text), "%d%%", battery_level_);
    }

    lv_color_t color = COLOR_GREEN;
    if (battery_level_ < 0) {
        color = COLOR_MUTED;
    } else if (battery_level_ <= 20) {
        color = lv_color_make(0xff, 0x88, 0x68);
    } else if (battery_level_ <= 40) {
        color = COLOR_GOLD;
    }

    for (size_t i = 0; i < sizeof(status_bar_battery_labels_) / sizeof(status_bar_battery_labels_[0]); ++i) {
        if (status_bar_battery_labels_[i]) {
            lv_label_set_text(status_bar_battery_labels_[i], text);
            lv_obj_set_style_text_color(status_bar_battery_labels_[i], color, 0);
        }
    }
}
void DesktopUI::SetFirmwareUpdateStatus(const char* status, bool update_available, bool busy, int progress) {
    (void)progress;
    if (settings_firmware_status_label_ && status) {
        lv_label_set_text(settings_firmware_status_label_, status);
    }

    if (settings_firmware_button_) {
        if (busy) {
            lv_obj_add_state(settings_firmware_button_, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(settings_firmware_button_, LV_STATE_DISABLED);
        }
        lv_obj_set_style_border_color(settings_firmware_button_,
                                      busy ? COLOR_MUTED : (update_available ? COLOR_GOLD : COLOR_GREEN),
                                      0);
    }

    if (settings_firmware_button_label_) {
        const char* text = busy ? "Wait" : (update_available ? "Update" : "Check");
        lv_label_set_text(settings_firmware_button_label_, text);
        lv_obj_set_style_text_color(settings_firmware_button_label_,
                                    update_available && !busy ? COLOR_GOLD : COLOR_TEXT, 0);
        lv_obj_center(settings_firmware_button_label_);
    }
}

void DesktopUI::SetRadioActions(std::function<void()> play_pause, std::function<void()> stop,
                                std::function<void()> next, std::function<void()> prev) {
    radio_play_pause_ = std::move(play_pause);
    radio_stop_ = std::move(stop);
    radio_next_ = std::move(next);
    radio_prev_ = std::move(prev);
}

void DesktopUI::SetRadioState(const char* station, const char* state, const char* meta) {
    if (radio_station_label_ && station) {
        lv_label_set_text(radio_station_label_, station);
    }
    if (radio_state_label_ && state) {
        lv_label_set_text(radio_state_label_, state);
        // 更新播放状态
        radio_playing_ = (state && (strcmp(state, "Playing") == 0 || strcmp(state, "Buffering") == 0));
    }
    if (radio_meta_label_ && meta) {
        lv_label_set_text(radio_meta_label_, meta);
    }
}

void DesktopUI::SetXiaozhiState(const char* state, const char* message, const char* emotion) {
    if (xiaozhi_state_label_) {
        lv_label_set_text(xiaozhi_state_label_, state ? state : "Standby");
    }
    if (xiaozhi_message_label_) {
        lv_label_set_text(xiaozhi_message_label_, message ? message : "");
    }
    if (xiaozhi_hint_label_) {
        lv_label_set_text(xiaozhi_hint_label_, state ? state : "");
    }
    if (emotion) {
        emotion_ = emotion;
    }
}

void DesktopUI::SetXiaozhiEmotion(const char* emotion) {
    emotion_ = emotion ? emotion : "neutral";
}

void DesktopUI::DailyCardBreathCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->daily_card_panel_) return;
    
    // Breathing animation: opacity 235-255 (0.92-1.0)
    static uint8_t breath_dir = 0; // 0=up, 1=down
    static lv_opa_t breath_opa = 235;
    
    if (breath_dir == 0) {
        breath_opa += 1;
        if (breath_opa >= 255) breath_dir = 1;
    } else {
        breath_opa -= 1;
        if (breath_opa <= 235) breath_dir = 0;
    }
    
    lv_obj_set_style_opa(self->daily_card_panel_, breath_opa, 0);
}

void DesktopUI::ClockShadowCb(lv_timer_t* timer) {
    (void)timer;
}

void DesktopUI::WeatherParticleCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->weather_sun_) return;
    
    // Simple particle effect: create small circles that float up
    lv_obj_t* parent = lv_obj_get_parent(self->weather_sun_);
    if (!parent) return;
    
    // Create a small particle
    lv_obj_t* particle = lv_obj_create(parent);
    lv_obj_remove_style_all(particle);
    lv_obj_set_size(particle, 4, 4);
    lv_obj_set_style_radius(particle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(particle, COLOR_GOLD, 0);
    lv_obj_set_style_bg_opa(particle, LV_OPA_60, 0);
    
    // Random position near sun
    int16_t x = 56 + (esp_random() % 54);
    int16_t y = 28 + (esp_random() % 30);
    lv_obj_align(particle, LV_ALIGN_TOP_LEFT, x, y);
    
    // Float up animation
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, particle);
    lv_anim_set_values(&anim, y, y - 40);
    lv_anim_set_time(&anim, 1500);
    lv_anim_set_exec_cb(&anim, ObjYCb);
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
        lv_obj_del(static_cast<lv_obj_t*>(a->var));
    });
    lv_anim_start(&anim);
    
    // Fade out
    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, particle);
    lv_anim_set_values(&fade, LV_OPA_60, LV_OPA_TRANSP);
    lv_anim_set_time(&fade, 1500);
    lv_anim_set_exec_cb(&fade, ObjOpaCb);
    lv_anim_start(&fade);
}
