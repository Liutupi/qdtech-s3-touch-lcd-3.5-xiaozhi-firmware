#include "desktop_ui.h"
#include "application.h"
#include "config.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <utility>

#include <esp_log.h>
#include <ssid_manager.h>

#define TAG "DesktopUI"

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_48);
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
    if (!self || !self->colon_label_) return;
    lv_opa_t opa = lv_obj_get_style_opa(self->colon_label_, 0);
    lv_obj_set_style_opa(self->colon_label_, opa < LV_OPA_50 ? LV_OPA_COVER : LV_OPA_20, 0);
}

void DesktopUI::FaceTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (self) {
        self->anim_tick_++;
        self->UpdateFaceAnimation();
    }
}

// ===== Page navigation =====
static DesktopUI* g_desktop_ui = nullptr;

static void show_main_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::MAIN);
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
                g_desktop_ui->ShowPage(DesktopPage::CALENDAR);
            }
            break;
        case 4:
            open_xiaozhi_with_message("Quote", "Ask XiaoZhi for a daily quote or encouragement.", true);
            break;
        case 5:
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

static void calendar_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(3);
    }
}

static void quote_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(4);
    }
}

static void settings_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(5);
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

static void settings_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
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
    CreateCalendarPage(root);
    CreateRadioPage(root);
    CreateXiaozhiPage(root);
    CreateSettingsPage(root);

    // Start with main page
    ShowPage(DesktopPage::MAIN);

    // Face animation timer
    lv_timer_create(FaceTimerCb, 100, this);

    ESP_LOGI(TAG, "Desktop UI created");
}

void DesktopUI::ShowPage(DesktopPage page) {
    const bool was_photo = current_page_ == DesktopPage::PHOTO;
    current_page_ = page;
    lv_obj_add_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
    if (photo_page_) {
        lv_obj_add_flag(photo_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (calendar_page_) {
        lv_obj_add_flag(calendar_page_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(radio_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
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
        case DesktopPage::XIAOZHI:
            lv_obj_clear_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show xiaozhi page");
            break;
        case DesktopPage::SETTINGS:
            if (settings_page_) {
                lv_obj_clear_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
                UpdateWifiList();
            }
            ESP_LOGI(TAG, "Show settings page");
            break;
    }

    const bool is_photo = page == DesktopPage::PHOTO;
    if (photo_active_callback_ && was_photo != is_photo) {
        photo_active_callback_(is_photo);
    }
}

void DesktopUI::HandleSwipe(int16_t dx, int16_t dy) {
    const int16_t min_dx = 70;
    const int16_t max_dy = 90;

    if (dx > min_dx && LV_ABS(dy) < max_dy) {
        ShowPage(DesktopPage::MAIN);
    } else if (dx < -min_dx && LV_ABS(dy) < max_dy) {
        ShowPage(DesktopPage::APPS);
    }
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
            ShowPage(DesktopPage::APPS);
            return;
        }
        if (x >= 177 && x < 303 && y >= 264 && y < 302) {
            Application::GetInstance().ToggleChatState();
            return;
        }
        return;
    }

    if (current_page_ == DesktopPage::RADIO) {
        if (x >= 360 && x < 470 && y >= 35 && y < 90) {
            if (radio_stop_) {
                radio_stop_();
            }
            ShowPage(DesktopPage::APPS);
            return;
        }
        if (x >= 40 && x < 126 && y >= 230 && y < 280) {
            if (radio_prev_) {
                radio_prev_();
            }
            return;
        }
        if (x >= 140 && x < 226 && y >= 230 && y < 280) {
            if (radio_play_pause_) {
                radio_play_pause_();
            }
            return;
        }
        if (x >= 240 && x < 326 && y >= 230 && y < 280) {
            if (radio_stop_) {
                radio_stop_();
            }
            return;
        }
        if (x >= 340 && x < 426 && y >= 230 && y < 280) {
            if (radio_next_) {
                radio_next_();
            }
            return;
        }
        return;
    }

    if (current_page_ == DesktopPage::PHOTO) {
        if (x >= 360 && x < 470 && y >= 35 && y < 90) {
            ShowPage(DesktopPage::APPS);
            return;
        }
        if (x >= 202 && x < 278 && y >= 268 && y < 308) {
            if (photo_refresh_callback_) {
                SetPhotoState("Refreshing", "Scanning SD card");
                photo_refresh_callback_();
            }
            return;
        }
        return;
    }

    if (current_page_ == DesktopPage::CALENDAR) {
        if (x >= 360 && x < 470 && y >= 35 && y < 90) {
            ShowPage(DesktopPage::APPS);
            return;
        }
        if (x >= 38 && x < 114 && y >= 264 && y < 304) {
            AdjustCalendarMonth(-1);
            return;
        }
        if (x >= 202 && x < 278 && y >= 264 && y < 304) {
            ShowTodayCalendar();
            return;
        }
        if (x >= 366 && x < 442 && y >= 264 && y < 304) {
            AdjustCalendarMonth(1);
            return;
        }
        return;
    }

    if (current_page_ != DesktopPage::APPS) {
        return;
    }

    if (x >= 360 && x < 470 && y >= 35 && y < 90) {
        ShowPage(DesktopPage::MAIN);
        return;
    }

    constexpr int16_t tile_w = 204;
    constexpr int16_t tile_h = 62;
    constexpr int16_t tile_x0 = 24;
    constexpr int16_t tile_y0 = 86;
    constexpr int16_t tile_x_gap = 218;
    constexpr int16_t tile_y_gap = 72;

    for (uint8_t i = 0; i < 6; ++i) {
        const int16_t tile_x = tile_x0 + (i % 2) * tile_x_gap;
        const int16_t tile_y = tile_y0 + (i / 2) * tile_y_gap;
        if (x >= tile_x && x < tile_x + tile_w && y >= tile_y && y < tile_y + tile_h) {
            open_app_card(i);
            return;
        }
    }
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

    const int16_t x[4] = {0, 54, 132, 186};
    for (uint8_t i = 0; i < 4; ++i) {
        lv_obj_t* card = lv_obj_create(time_group);
        lv_obj_add_style(card, &style_clock_card, 0);
        lv_obj_set_size(card, 48, 92);
        lv_obj_align(card, LV_ALIGN_TOP_LEFT, x[i], 52);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        add_gesture_bubble(card);

        // Divider line
        lv_obj_t* top = lv_obj_create(card);
        lv_obj_remove_style_all(top);
        lv_obj_set_size(top, 44, 1);
        lv_obj_set_style_bg_color(top, COLOR_LINE, 0);
        lv_obj_set_style_bg_opa(top, LV_OPA_70, 0);
        lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 45);

        // Shade
        lv_obj_t* shade = lv_obj_create(card);
        lv_obj_remove_style_all(shade);
        lv_obj_set_size(shade, 46, 43);
        lv_obj_set_style_bg_color(shade, LV_COLOR_MAKE(0x16, 0x13, 0x0d), 0);
        lv_obj_set_style_bg_opa(shade, LV_OPA_40, 0);
        lv_obj_align(shade, LV_ALIGN_TOP_MID, 0, 1);

        // Digit label
        lv_obj_t* digit = label_en(card, "0", &style_en);
        lv_obj_set_style_text_font(digit, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(digit, i < 2 ? COLOR_CREAM : COLOR_GOLD, 0);
        lv_obj_align(digit, LV_ALIGN_CENTER, 0, -2);
        clock_labels_[i] = digit;
    }

    colon_label_ = label_en(time_group, ":", &style_en);
    lv_obj_set_style_text_font(colon_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(colon_label_, COLOR_CREAM, 0);
    lv_obj_align(colon_label_, LV_ALIGN_TOP_LEFT, 111, 54);
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
    lv_obj_t* panel = CreatePanel(parent, 438, 94, 20, 214);

    daily_card_date_label_ = label_en(panel, "--/--", &style_gold);
    lv_obj_set_width(daily_card_date_label_, 82);
    lv_obj_set_style_text_align(daily_card_date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(daily_card_date_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(daily_card_date_label_, LV_ALIGN_TOP_LEFT, 16, 15);

    daily_card_title_label_ = label_en(panel, "今日", &style_muted);
    lv_obj_set_width(daily_card_title_label_, 82);
    lv_label_set_long_mode(daily_card_title_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(daily_card_title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(daily_card_title_label_, &qd_font_lxgw_16, 0);
    lv_obj_align(daily_card_title_label_, LV_ALIGN_TOP_LEFT, 16, 47);

    lv_obj_t* divider = bar(panel, 2, 62, COLOR_LINE, LV_OPA_COVER);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 112, 16);

    quote_label_ = label_en(panel, "正在同步今日卡片", &style_en);
    lv_obj_set_width(quote_label_, 286);
    lv_label_set_long_mode(quote_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(quote_label_, &qd_font_lxgw_20, 0);
    lv_obj_align(quote_label_, LV_ALIGN_TOP_LEFT, 132, 14);

    network_status_label_ = label_en(panel, "XiaoZhi AI", &style_muted);
    lv_obj_set_width(network_status_label_, 286);
    lv_label_set_long_mode(network_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(network_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(network_status_label_, LV_ALIGN_BOTTOM_LEFT, 132, -7);
}

// ===== Apps page =====
void DesktopUI::CreateAppsPage(lv_obj_t* root) {
    apps_page_ = lv_obj_create(root);
    lv_obj_add_style(apps_page_, &style_screen, 0);
    lv_obj_set_size(apps_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(apps_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(apps_page_, apps_gesture_cb, LV_EVENT_GESTURE, NULL);

    // Brand
    lv_obj_t* brand = label_en(apps_page_, "XiaoZhi AI", &style_en);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 18, 10);

    // Status bar
    CreateStatusBar(apps_page_);

    lv_obj_t* title = label_en(apps_page_, "Apps", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(apps_page_, "App Center", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 86, 53);

    lv_obj_t* back = CreateButton(apps_page_, "Back", show_main_cb);
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
        {"CAL", "Calendar", "Today", COLOR_PURPLE, calendar_card_cb},
        {"QTE", "Quote", "Daily", COLOR_GOLD, quote_card_cb},
        {"SET", "Settings", "System", COLOR_BLUE, settings_card_cb},
    };

    for (uint8_t i = 0; i < 6; ++i) {
        lv_obj_t* tile = CreateAppTile(apps_page_, i, apps[i].cn, apps[i].en, apps[i].status, apps[i].color);
        if (apps[i].cb) {
            lv_obj_add_event_cb(tile, apps[i].cb, LV_EVENT_CLICKED, NULL);
        }
    }

    lv_obj_t* hint = label_en(apps_page_, "Swipe left: Apps   Swipe right: Home", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

lv_obj_t* DesktopUI::CreateAppTile(lv_obj_t* parent, uint8_t index, const char* cn, const char* en, const char* status, lv_color_t color) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_add_style(box, &style_panel, 0);
    lv_obj_set_size(box, 204, 62);
    const int16_t x = 24 + (index % 2) * 218;
    const int16_t y = 86 + (index / 2) * 72;
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(box, apps_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    add_gesture_bubble(box);

    lv_obj_t* cn_label = label_en(box, cn, &style_en);
    lv_obj_set_style_text_color(cn_label, color, 0);
    lv_obj_set_style_text_font(cn_label, &lv_font_montserrat_20, 0);
    lv_obj_align(cn_label, LV_ALIGN_TOP_LEFT, 14, 8);

    lv_obj_t* en_label = label_en(box, en, &style_gold);
    lv_obj_set_style_text_color(en_label, COLOR_TEXT, 0);
    lv_obj_align(en_label, LV_ALIGN_TOP_LEFT, 70, 10);

    lv_obj_t* status_label = label_en(box, status, &style_muted);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 70, 34);
    if (index == 3) {
        calendar_app_status_label_ = status_label;
    }

    lv_obj_t* arrow = label_en(box, ">", &style_muted);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -14, 0);
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

// ===== Calendar page =====
void DesktopUI::CreateCalendarPage(lv_obj_t* root) {
    calendar_page_ = lv_obj_create(root);
    lv_obj_add_style(calendar_page_, &style_screen, 0);
    lv_obj_set_size(calendar_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(calendar_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(calendar_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(calendar_page_, calendar_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(calendar_page_);

    lv_obj_t* brand = label_en(calendar_page_, "XiaoZhi AI", &style_en);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 18, 10);

    CreateStatusBar(calendar_page_);

    lv_obj_t* title = label_en(calendar_page_, "Calendar", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(calendar_page_, "Month View", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 122, 53);

    lv_obj_t* back = CreateButton(calendar_page_, "Back", show_apps_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    lv_obj_t* panel = CreatePanel(calendar_page_, 432, 168, 24, 86);

    calendar_title_label_ = label_en(panel, "---- / --", &style_gold);
    lv_obj_set_style_text_font(calendar_title_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(calendar_title_label_, LV_ALIGN_TOP_LEFT, 16, 10);

    calendar_today_label_ = label_en(panel, "Waiting for time sync", &style_muted);
    lv_obj_set_width(calendar_today_label_, 230);
    lv_label_set_long_mode(calendar_today_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(calendar_today_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(calendar_today_label_, LV_ALIGN_TOP_RIGHT, -16, 15);

    static constexpr const char* kWeekdays[] = {"M", "T", "W", "T", "F", "S", "S"};
    for (int col = 0; col < 7; ++col) {
        lv_obj_t* label = label_en(panel, kWeekdays[col], &style_muted);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, 52);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14 + col * 60, 48);
    }

    for (int i = 0; i < 42; ++i) {
        const int col = i % 7;
        const int row = i / 7;
        lv_obj_t* cell = lv_obj_create(panel);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, 52, 18);
        lv_obj_set_style_radius(cell, 5, 0);
        lv_obj_set_style_bg_color(cell, COLOR_SURFACE_2, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
        lv_obj_align(cell, LV_ALIGN_TOP_LEFT, 14 + col * 60, 70 + row * 16);
        add_gesture_bubble(cell);
        calendar_day_cells_[i] = cell;

        lv_obj_t* day = label_en(cell, "", &style_en);
        lv_obj_set_width(day, 52);
        lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(day, &lv_font_montserrat_14, 0);
        lv_obj_center(day);
        calendar_day_labels_[i] = day;
    }

    lv_obj_t* prev = CreateButton(calendar_page_, "Prev", calendar_prev_cb);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 38, 268);

    lv_obj_t* today = CreateButton(calendar_page_, "Today", calendar_today_cb);
    lv_obj_align(today, LV_ALIGN_TOP_LEFT, 202, 268);

    lv_obj_t* next = CreateButton(calendar_page_, "Next", calendar_next_cb);
    lv_obj_align(next, LV_ALIGN_TOP_LEFT, 366, 268);

    lv_obj_t* hint = label_en(calendar_page_, "Swipe right: Apps", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);

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

    lv_obj_t* brand = label_en(radio_page_, "XiaoZhi AI", &style_en);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 18, 10);

    CreateStatusBar(radio_page_);

    lv_obj_t* title = label_en(radio_page_, "Radio", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(radio_page_, "Network FM", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 88, 53);

    lv_obj_t* back = CreateButton(radio_page_, "Back", show_apps_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    lv_obj_t* panel = CreatePanel(radio_page_, 432, 134, 24, 88);
    radio_station_label_ = label_en(panel, "CNR China Voice", &style_gold);
    lv_obj_set_style_text_font(radio_station_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(radio_station_label_, LV_ALIGN_TOP_LEFT, 16, 14);

    radio_state_label_ = label_en(panel, "Ready", &style_green);
    lv_obj_set_style_text_font(radio_state_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(radio_state_label_, LV_ALIGN_TOP_LEFT, 16, 52);

    radio_meta_label_ = label_en(panel, "Tap Play to start MP3 stream", &style_muted);
    lv_obj_set_width(radio_meta_label_, 390);
    lv_label_set_long_mode(radio_meta_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(radio_meta_label_, &lv_font_montserrat_14, 0);
    lv_obj_align(radio_meta_label_, LV_ALIGN_BOTTOM_LEFT, 16, -14);

    lv_obj_t* prev = CreateButton(radio_page_, "Prev", nullptr);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 52, 238);

    lv_obj_t* play = CreateButton(radio_page_, "Play", nullptr);
    lv_obj_align(play, LV_ALIGN_TOP_LEFT, 152, 238);

    lv_obj_t* stop = CreateButton(radio_page_, "Stop", nullptr);
    lv_obj_align(stop, LV_ALIGN_TOP_LEFT, 252, 238);

    lv_obj_t* next = CreateButton(radio_page_, "Next", nullptr);
    lv_obj_align(next, LV_ALIGN_TOP_LEFT, 352, 238);

    lv_obj_t* hint = label_en(radio_page_, "Swipe right: Apps", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
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

// ===== Settings page =====
void DesktopUI::CreateSettingsPage(lv_obj_t* root) {
    settings_page_ = lv_obj_create(root);
    lv_obj_add_style(settings_page_, &style_screen, 0);
    lv_obj_set_size(settings_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(settings_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(settings_page_, settings_gesture_cb, LV_EVENT_GESTURE, NULL);
    
    // Brand
    lv_obj_t* brand = label_en(settings_page_, "XiaoZhi AI", &style_en);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 18, 10);

    // Status bar
    CreateStatusBar(settings_page_);

    lv_obj_t* title = label_en(settings_page_, "Settings", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(settings_page_, "System Configuration", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 100, 53);

    lv_obj_t* back = CreateButton(settings_page_, "Back", show_apps_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    // WiFi Section Title
    lv_obj_t* wifi_title = label_en(settings_page_, "WiFi Networks", &style_gold);
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_16, 0);
    lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 24, 82);

    // WiFi list panel
    lv_obj_t* list_panel = lv_obj_create(settings_page_);
    lv_obj_add_style(list_panel, &style_panel, 0);
    lv_obj_set_size(list_panel, 438, 160);
    lv_obj_align(list_panel, LV_ALIGN_TOP_LEFT, 14, 106);
    lv_obj_set_scroll_dir(list_panel, LV_DIR_VER);
    add_gesture_bubble(list_panel);
    
    // 创建WiFi列表容器
    lv_obj_t* list_container = lv_obj_create(list_panel);
    lv_obj_remove_style_all(list_container);
    lv_obj_set_size(list_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    add_gesture_bubble(list_container);
    
    // 添加滚动条样式
    lv_obj_set_scrollbar_mode(list_panel, LV_SCROLLBAR_MODE_AUTO);
    
    // Help text for WiFi management
    lv_obj_t* wifi_help = label_en(settings_page_, "Say: 'Switch to WiFi name' or 'Remove WiFi index'", &style_muted);
    lv_obj_set_style_text_font(wifi_help, &lv_font_montserrat_12, 0);
    lv_obj_align(wifi_help, LV_ALIGN_TOP_LEFT, 24, 272);
    
    // Other Settings Section
    lv_obj_t* other_title = label_en(settings_page_, "Other Settings", &style_gold);
    lv_obj_set_style_text_font(other_title, &lv_font_montserrat_16, 0);
    lv_obj_align(other_title, LV_ALIGN_TOP_LEFT, 24, 296);
    
    // Settings options
    lv_obj_t* opt1 = CreatePanel(settings_page_, 438, 36, 14, 320);
    lv_obj_t* opt1_label = label_en(opt1, "Brightness", &style_en);
    lv_obj_align(opt1_label, LV_ALIGN_LEFT_MID, 16, 0);
    
    lv_obj_t* opt2 = CreatePanel(settings_page_, 438, 36, 14, 362);
    lv_obj_t* opt2_label = label_en(opt2, "Volume", &style_en);
    lv_obj_align(opt2_label, LV_ALIGN_LEFT_MID, 16, 0);
    
    lv_obj_t* opt3 = CreatePanel(settings_page_, 438, 36, 14, 404);
    lv_obj_t* opt3_label = label_en(opt3, "Weather Location", &style_en);
    lv_obj_align(opt3_label, LV_ALIGN_LEFT_MID, 16, 0);
    
    // Hint text
    lv_obj_t* hint = label_en(settings_page_, "Swipe right: Apps", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
    
    // 保存列表容器的引用
    lv_obj_set_user_data(settings_page_, list_container);
}

void DesktopUI::UpdateWifiList() {
    if (!settings_page_) return;
    
    lv_obj_t* list_container = static_cast<lv_obj_t*>(lv_obj_get_user_data(settings_page_));
    if (!list_container) return;
    
    // 清空列表
    lv_obj_clean(list_container);
    
    // 获取已保存的WiFi列表
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    
    if (ssid_list.empty()) {
        // 显示空列表提示
        lv_obj_t* empty_label = label_en(list_container, "No saved WiFi networks", &style_muted);
        lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty_label, LV_PCT(100));
        return;
    }
    
    // 添加WiFi条目
    for (size_t i = 0; i < ssid_list.size(); i++) {
        lv_obj_t* item = lv_obj_create(list_container);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(item, COLOR_SURFACE_2, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_pad_hor(item, 16, 0);
        lv_obj_set_style_pad_ver(item, 8, 0);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // WiFi名称
        lv_obj_t* ssid_label = label_en(item, ssid_list[i].ssid.c_str(), &style_en);
        lv_obj_set_flex_grow(ssid_label, 1);
        
        // 序号标签
        char index_str[16];
        snprintf(index_str, sizeof(index_str), "#%d", (int)i + 1);
        label_en(item, index_str, &style_muted);
        
        // 如果是第一个，显示"Default"标签
        if (i == 0) {
            lv_obj_t* default_label = label_en(item, "Default", &style_green);
            lv_obj_set_style_text_font(default_label, &lv_font_montserrat_12, 0);
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

    lv_obj_t* battery = label_en(parent, "80%", &style_green);
    lv_obj_align(battery, LV_ALIGN_TOP_RIGHT, -20, 12);
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
        lv_label_set_text(calendar_title_label_, "---- / --");
        lv_label_set_text(calendar_today_label_, "Waiting for time sync");
        for (int i = 0; i < 42; ++i) {
            if (calendar_day_labels_[i]) {
                lv_label_set_text(calendar_day_labels_[i], "");
            }
            if (calendar_day_cells_[i]) {
                lv_obj_set_style_bg_opa(calendar_day_cells_[i], LV_OPA_TRANSP, 0);
            }
        }
        return;
    }

    char title[32];
    snprintf(title, sizeof(title), "%04d / %02d", calendar_year_, calendar_month_);
    lv_label_set_text(calendar_title_label_, title);

    char today[48];
    if (current_year_ > 0) {
        snprintf(today, sizeof(today), "Today %04d/%02d/%02d", current_year_, current_month_, current_day_);
    } else {
        snprintf(today, sizeof(today), "Time not synced");
    }
    lv_label_set_text(calendar_today_label_, today);

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

        lv_obj_set_style_bg_color(calendar_day_cells_[i], is_today ? COLOR_GREEN : COLOR_SURFACE_2, 0);
        lv_obj_set_style_bg_opa(calendar_day_cells_[i], is_today ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(calendar_day_labels_[i],
                                    is_today ? COLOR_BG :
                                    !in_current_month ? COLOR_MUTED :
                                    weekend ? COLOR_GOLD : COLOR_TEXT,
                                    0);
    }
}

// ===== Time rendering =====
void DesktopUI::FlipDigit(uint8_t index, uint8_t digit, bool animate) {
    if (index >= 4 || !clock_labels_[index]) return;
    if (clock_digits_[index] == digit && animate) return;

    clock_digits_[index] = digit;
    char text[2] = {(char)('0' + digit), 0};
    lv_label_set_text(clock_labels_[index], text);
    lv_obj_align(clock_labels_[index], LV_ALIGN_CENTER, 0, -2);

    if (!animate) return;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, clock_labels_[index]);
    lv_anim_set_values(&anim, -10, -2);
    lv_anim_set_time(&anim, 180);
    lv_anim_set_exec_cb(&anim, ObjYCb);
    lv_anim_start(&anim);

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, clock_labels_[index]);
    lv_anim_set_values(&anim, LV_OPA_50, LV_OPA_COVER);
    lv_anim_set_time(&anim, 180);
    lv_anim_set_exec_cb(&anim, ObjOpaCb);
    lv_anim_start(&anim);
}

void DesktopUI::RenderBigTime(int hour, int minute, bool animate) {
    const uint8_t digits[4] = {
        (uint8_t)((hour / 10) % 10),
        (uint8_t)(hour % 10),
        (uint8_t)((minute / 10) % 10),
        (uint8_t)(minute % 10),
    };
    for (uint8_t i = 0; i < 4; ++i) {
        FlipDigit(i, digits[i], animate);
    }
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

    const bool date_changed = year != current_year_ || month != current_month_ || day != current_day_;
    current_year_ = year;
    current_month_ = month;
    current_day_ = day;
    if (calendar_year_ <= 0 || calendar_month_ <= 0 || calendar_follow_today_) {
        calendar_year_ = year;
        calendar_month_ = month;
        calendar_follow_today_ = true;
    }

    RenderBigTime(hour, minute, true);
    lv_label_set_text(date_label_, date_text);
    lv_label_set_text(week_label_, weekday ? weekday : "---");
    if (calendar_app_status_label_) {
        char app_status[24];
        snprintf(app_status, sizeof(app_status), "%04d/%02d/%02d", year, month, day);
        lv_label_set_text(calendar_app_status_label_, app_status);
    }

    for (size_t i = 0; i < sizeof(status_bar_time_labels_) / sizeof(status_bar_time_labels_[0]); ++i) {
        if (status_bar_time_labels_[i]) {
            lv_label_set_text(status_bar_time_labels_[i], time_text);
        }
    }

    if (date_changed || current_page_ == DesktopPage::CALENDAR) {
        RenderCalendar();
    }
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

    ESP_LOGI(TAG, "Weather visual code=%d clear=%d cloud=%d rain=%d snow=%d storm=%d",
             weather_code, is_clear, show_cloud, is_rain, is_snow, is_storm);
}

void DesktopUI::SetWeather(const char* temperature, const char* summary, int weather_code) {
    if (!weather_temp_label_ || !weather_meta_label_) return;
    lv_label_set_text(weather_temp_label_, temperature ? temperature : "-- C");
    lv_label_set_text(weather_meta_label_, summary ? summary : "Weather pending");
    ApplyWeatherVisual(weather_code);
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

void DesktopUI::SetNetworkStatus(const char* status) {
    if (!network_status_label_ || !status) return;
    lv_label_set_text(network_status_label_, status);
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
