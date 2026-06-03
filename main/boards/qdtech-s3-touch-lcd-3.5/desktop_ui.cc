#include "desktop_ui.h"
#include "application.h"

#include <cstdio>
#include <cstring>
#include <cmath>

#include <esp_log.h>

#define TAG "DesktopUI"

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_48);

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
            open_xiaozhi_with_message("Radio", "Tell XiaoZhi which station or music you want.", true);
            break;
        case 1:
            open_xiaozhi_with_message("Weather", "Ask XiaoZhi for weather, or set a new weather city.", true);
            break;
        case 2:
            open_xiaozhi_with_message("XiaoZhi AI", "Starting conversation...", true);
            break;
        case 3:
            open_xiaozhi_with_message("Calendar", "Ask XiaoZhi about dates, plans, or reminders.", true);
            break;
        case 4:
            open_xiaozhi_with_message("Quote", "Ask XiaoZhi for a daily quote or encouragement.", true);
            break;
        case 5:
            open_xiaozhi_with_message("Settings", "Ask XiaoZhi to adjust screen, volume, WiFi, or weather location.", true);
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

static void weather_card_cb(lv_event_t* event) {
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
    CreateXiaozhiPage(root);

    // Start with main page
    ShowPage(DesktopPage::MAIN);

    // Face animation timer
    lv_timer_create(FaceTimerCb, 100, this);

    ESP_LOGI(TAG, "Desktop UI created");
}

void DesktopUI::ShowPage(DesktopPage page) {
    current_page_ = page;
    lv_obj_add_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);

    switch (page) {
        case DesktopPage::MAIN:
            lv_obj_clear_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show main page");
            break;
        case DesktopPage::APPS:
            lv_obj_clear_flag(apps_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show apps page");
            break;
        case DesktopPage::XIAOZHI:
            lv_obj_clear_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show xiaozhi page");
            break;
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

    if (current_page_ != DesktopPage::APPS) {
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

    // Sun
    lv_obj_t* sun = circle(panel, 54, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align(sun, LV_ALIGN_TOP_LEFT, 56, 32);

    // Clouds
    lv_obj_t* cloud_a = circle(panel, 38, COLOR_CREAM, LV_OPA_30);
    lv_obj_align(cloud_a, LV_ALIGN_TOP_LEFT, 48, 66);
    lv_obj_t* cloud_b = circle(panel, 46, COLOR_CREAM, LV_OPA_30);
    lv_obj_align(cloud_b, LV_ALIGN_TOP_LEFT, 76, 58);
    lv_obj_t* cloud_c = bar(panel, 80, 26, COLOR_CREAM, LV_OPA_30);
    lv_obj_align(cloud_c, LV_ALIGN_TOP_LEFT, 48, 82);

    // Sun animation
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, sun);
    lv_anim_set_values(&anim, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_time(&anim, 1600);
    lv_anim_set_playback_time(&anim, 1600);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, ObjOpaCb);
    lv_anim_start(&anim);

    weather_temp_label_ = label_en(panel, "-- C", &style_en);
    lv_obj_set_style_text_font(weather_temp_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(weather_temp_label_, LV_ALIGN_BOTTOM_LEFT, 12, -27);

    weather_meta_label_ = label_en(panel, "Weather pending", &style_green);
    lv_obj_set_width(weather_meta_label_, 142);
    lv_label_set_long_mode(weather_meta_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(weather_meta_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(weather_meta_label_, LV_ALIGN_BOTTOM_LEFT, 12, -10);
}

void DesktopUI::CreateQuotePanel(lv_obj_t* parent) {
    lv_obj_t* panel = CreatePanel(parent, 438, 94, 20, 214);

    lv_obj_t* title = label_en(panel, "Quote", &style_gold);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 9);

    quote_label_ = label_en(panel, "Every sunrise is a fresh chance to begin again.", &style_en);
    lv_obj_set_width(quote_label_, 405);
    lv_label_set_long_mode(quote_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(quote_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(quote_label_, LV_ALIGN_TOP_LEFT, 16, 32);

    network_status_label_ = label_en(panel, "XiaoZhi AI", &style_muted);
    lv_obj_set_width(network_status_label_, 405);
    lv_label_set_long_mode(network_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(network_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(network_status_label_, LV_ALIGN_BOTTOM_LEFT, 16, -7);
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
        {"WTR", "Weather", "Forecast", COLOR_GREEN, weather_card_cb},
        {"AI", "XiaoZhi", "Online", COLOR_PURPLE, xiaozhi_card_cb},
        {"CAL", "Calendar", "2026/01/01", COLOR_PURPLE, calendar_card_cb},
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

    lv_obj_t* arrow = label_en(box, ">", &style_muted);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -14, 0);
    return box;
}

// ===== XiaoZhi page =====
void DesktopUI::CreateXiaozhiPage(lv_obj_t* root) {
    xiaozhi_page_ = lv_obj_create(root);
    lv_obj_add_style(xiaozhi_page_, &style_screen, 0);
    lv_obj_set_size(xiaozhi_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(xiaozhi_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(xiaozhi_page_, xiaozhi_gesture_cb, LV_EVENT_GESTURE, NULL);

    // Brand
    lv_obj_t* brand = label_en(xiaozhi_page_, "XiaoZhi AI", &style_en);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 18, 10);

    // Status bar
    CreateStatusBar(xiaozhi_page_);

    lv_obj_t* title = label_en(xiaozhi_page_, "XiaoZhi AI", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 48);

    lv_obj_t* back = CreateButton(xiaozhi_page_, "Back", show_apps_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 46);

    // Divider line
    lv_obj_t* top_line = lv_obj_create(xiaozhi_page_);
    lv_obj_remove_style_all(top_line);
    lv_obj_set_size(top_line, 438, 1);
    lv_obj_set_style_bg_color(top_line, COLOR_LINE, 0);
    lv_obj_set_style_bg_opa(top_line, LV_OPA_COVER, 0);
    lv_obj_align(top_line, LV_ALIGN_TOP_MID, 0, 78);
    add_gesture_bubble(top_line);

    // Face
    CreateFaceUI(xiaozhi_page_);

    // State labels
    xiaozhi_state_label_ = label_en(xiaozhi_page_, "Standby", &style_green);
    lv_obj_set_style_text_font(xiaozhi_state_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(xiaozhi_state_label_, LV_ALIGN_TOP_MID, 0, 272);

    xiaozhi_hint_label_ = label_en(xiaozhi_page_, "Ready / Listening / Speaking", &style_muted);
    lv_obj_set_style_text_font(xiaozhi_hint_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(xiaozhi_hint_label_, LV_ALIGN_TOP_MID, 0, 300);

    xiaozhi_message_label_ = label_en(xiaozhi_page_, "Tap to start conversation", &style_muted);
    lv_obj_set_width(xiaozhi_message_label_, 420);
    lv_label_set_long_mode(xiaozhi_message_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(xiaozhi_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(xiaozhi_message_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(xiaozhi_message_label_, LV_ALIGN_TOP_MID, 0, 320);

    // Talk button
    lv_obj_t* talk = CreateButton(xiaozhi_page_, "Talk", xiaozhi_toggle_cb);
    lv_obj_set_size(talk, 126, 38);
    lv_obj_align(talk, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void DesktopUI::CreateFaceUI(lv_obj_t* parent) {
    face_container_ = lv_obj_create(parent);
    lv_obj_set_size(face_container_, 176, 176);
    lv_obj_align(face_container_, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_radius(face_container_, 88, 0);
    lv_obj_set_style_bg_color(face_container_, LV_COLOR_MAKE(0x16, 0x19, 0x1c), 0);
    lv_obj_set_style_border_color(face_container_, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(face_container_, 2, 0);
    lv_obj_clear_flag(face_container_, LV_OBJ_FLAG_SCROLLABLE);
    add_gesture_bubble(face_container_);

    // Eyes
    eye_left_ = lv_obj_create(face_container_);
    lv_obj_set_size(eye_left_, 30, 40);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -42, -18);
    lv_obj_set_style_radius(eye_left_, 15, 0);
    lv_obj_set_style_bg_color(eye_left_, COLOR_CREAM, 0);
    lv_obj_set_style_border_width(eye_left_, 0, 0);
    add_gesture_bubble(eye_left_);

    eye_right_ = lv_obj_create(face_container_);
    lv_obj_set_size(eye_right_, 30, 40);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, 42, -18);
    lv_obj_set_style_radius(eye_right_, 15, 0);
    lv_obj_set_style_bg_color(eye_right_, COLOR_CREAM, 0);
    lv_obj_set_style_border_width(eye_right_, 0, 0);
    add_gesture_bubble(eye_right_);

    // Mouth
    mouth_ = lv_obj_create(face_container_);
    lv_obj_set_size(mouth_, 62, 12);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 44);
    lv_obj_set_style_radius(mouth_, 6, 0);
    lv_obj_set_style_bg_color(mouth_, COLOR_GOLD, 0);
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
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    add_gesture_bubble(btn);

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
    if (!face_container_ || !eye_left_ || !eye_right_ || !mouth_) return;

    const DeviceState state = Application::GetInstance().GetDeviceState();
    const bool speaking = state == kDeviceStateSpeaking;
    const bool listening = state == kDeviceStateListening || state == kDeviceStateConnecting;

    // Simple eye height animation
    int eye_h = 40;
    if (speaking) {
        eye_h = 35 + (int)(5 * sin(anim_tick_ * 0.3));
    } else if (listening) {
        eye_h = 45;
    } else {
        // Blink
        const int blink_phase = anim_tick_ % 32;
        if (blink_phase >= 28) {
            eye_h = 8;
        }
    }

    // Update eye sizes
    lv_obj_set_height(eye_left_, eye_h);
    lv_obj_set_height(eye_right_, eye_h);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -42, -18);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, 42, -18);

    // Update mouth based on state
    if (speaking) {
        static constexpr int mouth_heights[] = {12, 28, 22, 18, 24};
        const int phase = anim_tick_ % 5;
        lv_obj_set_height(mouth_, mouth_heights[phase]);
        lv_obj_set_style_bg_color(mouth_, COLOR_GREEN, 0);
    } else if (listening) {
        lv_obj_set_height(mouth_, 16);
        lv_obj_set_style_bg_color(mouth_, COLOR_GOLD, 0);
    } else {
        lv_obj_set_height(mouth_, 12);
        lv_obj_set_style_bg_color(mouth_, COLOR_GOLD, 0);
    }
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 44);

    // Update face border color based on emotion
    if (strcmp(emotion_, "happy") == 0) {
        lv_obj_set_style_border_color(face_container_, COLOR_GREEN, 0);
    } else if (strcmp(emotion_, "sad") == 0) {
        lv_obj_set_style_border_color(face_container_, LV_COLOR_MAKE(0xff, 0x79, 0x79), 0);
    } else if (strcmp(emotion_, "thinking") == 0) {
        lv_obj_set_style_border_color(face_container_, COLOR_PURPLE, 0);
    } else {
        lv_obj_set_style_border_color(face_container_, COLOR_GREEN, 0);
    }
}

// ===== Public API =====
void DesktopUI::SetTime(int hour, int minute, int month, int day, const char* weekday) {
    if (!date_label_ || !week_label_) return;

    char date_text[24];
    char time_text[8];
    snprintf(date_text, sizeof(date_text), "%02d / %02d     |", month, day);
    snprintf(time_text, sizeof(time_text), "%02d:%02d", hour, minute);

    RenderBigTime(hour, minute, true);
    lv_label_set_text(date_label_, date_text);
    lv_label_set_text(week_label_, weekday ? weekday : "---");

    for (size_t i = 0; i < sizeof(status_bar_time_labels_) / sizeof(status_bar_time_labels_[0]); ++i) {
        if (status_bar_time_labels_[i]) {
            lv_label_set_text(status_bar_time_labels_[i], time_text);
        }
    }
}

void DesktopUI::SetWeather(const char* temperature, const char* summary, int weather_code) {
    if (!weather_temp_label_ || !weather_meta_label_) return;
    lv_label_set_text(weather_temp_label_, temperature ? temperature : "-- C");
    lv_label_set_text(weather_meta_label_, summary ? summary : "Weather pending");
    (void)weather_code;
}

void DesktopUI::SetDailyQuote(const char* quote) {
    if (!quote_label_ || !quote) return;
    lv_label_set_text(quote_label_, quote);
}

void DesktopUI::SetNetworkStatus(const char* status) {
    if (!network_status_label_ || !status) return;
    lv_label_set_text(network_status_label_, status);
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
