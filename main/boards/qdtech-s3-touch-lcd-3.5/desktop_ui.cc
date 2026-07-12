#include "desktop_ui.h"
#include "application.h"
#include "assets/lang_config.h"
#include "config.h"
#include "audio_codecs/audio_codec.h"
#include "boards/common/board.h"
#include "firmware_update_service.h"
#include "qd_user_config.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <ssid_manager.h>
#include <wifi_station.h>
#include <libs/gif/lv_gif.h>
#include <libs/qrcode/lv_qrcode.h>

#define TAG "DesktopUI"

static constexpr int64_t kMusicLyricHoldMs = 12000;
static constexpr int64_t kMusicControlDebounceMs = 450;

LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_48);
LV_FONT_DECLARE(qd_font_clock_72);
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(qd_font_lxgw_16);
LV_FONT_DECLARE(qd_font_lxgw_20);
LV_IMAGE_DECLARE(qd_weather_clear_scene);
LV_IMAGE_DECLARE(qd_weather_cloudy_scene);
LV_IMAGE_DECLARE(qd_weather_rain_scene);
LV_IMAGE_DECLARE(qd_weather_snow_scene);
LV_IMAGE_DECLARE(qd_weather_fog_scene);
LV_IMAGE_DECLARE(qd_weather_storm_scene);
LV_IMAGE_DECLARE(qd_brand_earth);
LV_IMAGE_DECLARE(qd_music_vinyl);
LV_IMAGE_DECLARE(qd_podcast_avatar);
LV_IMAGE_DECLARE(qd_cat_daily);
LV_IMAGE_DECLARE(qd_classic_daily);
LV_IMAGE_DECLARE(qd_classic_bot_standby);
LV_IMAGE_DECLARE(qd_classic_bot_listening);
LV_IMAGE_DECLARE(qd_classic_bot_speaking);
LV_IMAGE_DECLARE(qd_cat_standby);
LV_IMAGE_DECLARE(qd_cat_listening);
LV_IMAGE_DECLARE(qd_cat_speaking);
LV_IMAGE_DECLARE(qd_cat_thinking);
LV_IMAGE_DECLARE(qd_cat_happy);
LV_IMAGE_DECLARE(qd_cat_surprised);
LV_IMAGE_DECLARE(qd_cat_sad);
LV_IMAGE_DECLARE(qd_cat_angry);
LV_IMAGE_DECLARE(qd_cat_sleepy);
LV_IMAGE_DECLARE(qd_tupi_bot_standby);
LV_IMAGE_DECLARE(qd_tupi_bot_listening);
LV_IMAGE_DECLARE(qd_tupi_bot_speaking);
LV_IMAGE_DECLARE(qd_tupi_bot_thinking);
LV_IMAGE_DECLARE(qd_tupi_bot_happy);
LV_IMAGE_DECLARE(qd_tupi_bot_surprised);
LV_IMAGE_DECLARE(qd_tupi_bot_sad);
LV_IMAGE_DECLARE(qd_tupi_bot_angry);
LV_IMAGE_DECLARE(qd_tupi_bot_sleepy);
LV_IMAGE_DECLARE(qd_hourglass_body);

static const lv_font_t* qd_cn_font_16() {
    return &font_puhui_16_4;
}

static const lv_font_t* qd_cn_font_20() {
    // Puhui has broader Chinese coverage than the compact LXGW subsets.
    return &font_puhui_16_4;
}

static std::string clean_subtitle_text(const char* text, size_t max_codepoints = 42) {
    if (!text) {
        return {};
    }

    std::string out;
    out.reserve(128);
    size_t count = 0;
    const auto* p = reinterpret_cast<const uint8_t*>(text);
    while (*p && count < max_codepoints) {
        uint32_t cp = 0;
        size_t len = 0;
        if (*p < 0x80) {
            cp = *p;
            len = 1;
        } else if ((*p & 0xe0) == 0xc0) {
            cp = *p & 0x1f;
            len = 2;
        } else if ((*p & 0xf0) == 0xe0) {
            cp = *p & 0x0f;
            len = 3;
        } else if ((*p & 0xf8) == 0xf0) {
            cp = *p & 0x07;
            len = 4;
        } else {
            ++p;
            continue;
        }

        bool valid = true;
        for (size_t i = 1; i < len; ++i) {
            if ((p[i] & 0xc0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (p[i] & 0x3f);
        }
        if (!valid || (len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) ||
            (len == 4 && (cp < 0x10000 || cp > 0x10ffff)) ||
            (cp >= 0xd800 && cp <= 0xdfff)) {
            ++p;
            continue;
        }

        if (cp == '\r' || cp == '\n' || cp == '\t') {
            if (!out.empty() && out.back() != ' ') {
                out.push_back(' ');
                ++count;
            }
        } else if (cp >= 0x20 && cp != 0x7f) {
            out.append(reinterpret_cast<const char*>(p), len);
            ++count;
        }
        p += len;
    }

    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

// Theme palette
enum class UiTheme : uint8_t {
    CLASSIC = 0,
    CAT = 1,
    TUPI_WARM = 2,
};

struct ThemePalette {
    const char* name;
    lv_color_t bg;
    lv_color_t surface;
    lv_color_t surface2;
    lv_color_t text;
    lv_color_t cream;
    lv_color_t muted;
    lv_color_t line;
    lv_color_t gold;
    lv_color_t green;
    lv_color_t purple;
    lv_color_t blue;
    lv_color_t clock_dot;
};

static constexpr ThemePalette THEMES[] = {
    {
        "Classic",
        LV_COLOR_MAKE(0x00, 0x00, 0x00),
        LV_COLOR_MAKE(0x0b, 0x0c, 0x0d),
        LV_COLOR_MAKE(0x12, 0x14, 0x13),
        LV_COLOR_MAKE(0xf6, 0xef, 0xdf),
        LV_COLOR_MAKE(0xff, 0xf4, 0xd8),
        LV_COLOR_MAKE(0x8a, 0x8a, 0x82),
        LV_COLOR_MAKE(0x34, 0x35, 0x31),
        LV_COLOR_MAKE(0xff, 0xbd, 0x55),
        LV_COLOR_MAKE(0x82, 0xd7, 0x78),
        LV_COLOR_MAKE(0xaa, 0x78, 0xff),
        LV_COLOR_MAKE(0x68, 0x9d, 0xff),
        LV_COLOR_MAKE(0xd7, 0xde, 0xe3),
    },
    {
        "Cat",
        LV_COLOR_MAKE(0xf6, 0xdb, 0xe8),
        LV_COLOR_MAKE(0xff, 0xf7, 0xfb),
        LV_COLOR_MAKE(0xff, 0xeb, 0xf3),
        LV_COLOR_MAKE(0x4d, 0x3d, 0x50),
        LV_COLOR_MAKE(0xff, 0xfd, 0xfe),
        LV_COLOR_MAKE(0x9a, 0x6f, 0x88),
        LV_COLOR_MAKE(0xff, 0x8f, 0xb5),
        LV_COLOR_MAKE(0xff, 0xa9, 0x58),
        LV_COLOR_MAKE(0x68, 0xd1, 0xa2),
        LV_COLOR_MAKE(0xff, 0x6f, 0xa2),
        LV_COLOR_MAKE(0x7b, 0xc7, 0xff),
        LV_COLOR_MAKE(0xff, 0x77, 0xaa),
    },
    {
        "Tupi Warm",
        LV_COLOR_MAKE(0xf6, 0xf0, 0xe6),
        LV_COLOR_MAKE(0xfb, 0xf7, 0xee),
        LV_COLOR_MAKE(0xf4, 0xee, 0xe3),
        LV_COLOR_MAKE(0x1c, 0x1b, 0x19),
        LV_COLOR_MAKE(0xff, 0xfc, 0xf4),
        LV_COLOR_MAKE(0x6f, 0x72, 0x5f),
        LV_COLOR_MAKE(0xc9, 0xbf, 0xae),
        LV_COLOR_MAKE(0xd9, 0x90, 0x2f),
        LV_COLOR_MAKE(0x7f, 0x87, 0x6d),
        LV_COLOR_MAKE(0xb9, 0x79, 0x5a),
        LV_COLOR_MAKE(0x8d, 0x8b, 0x7d),
        LV_COLOR_MAKE(0x7f, 0x87, 0x6d),
    },
};

static UiTheme current_theme = UiTheme::CLASSIC;

static const ThemePalette& theme() {
    return THEMES[static_cast<uint8_t>(current_theme)];
}

static bool is_cat_theme() {
    return current_theme == UiTheme::CAT;
}

static bool is_tupi_warm_theme() {
    return current_theme == UiTheme::TUPI_WARM;
}

static bool is_classic_theme() {
    return current_theme == UiTheme::CLASSIC;
}

static bool is_themed_face_gif_theme() {
    return true;
}

static void load_theme() {
    nvs_handle_t handle;
    uint8_t value = 0;
    if (nvs_open("qd_ui", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, "theme", &value);
        nvs_close(handle);
    }
    if (value >= sizeof(THEMES) / sizeof(THEMES[0])) {
        value = 0;
    }
    current_theme = static_cast<UiTheme>(value);
}

static void save_theme(UiTheme next_theme) {
    nvs_handle_t handle;
    if (nvs_open("qd_ui", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "theme", static_cast<uint8_t>(next_theme));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static lv_color_t themed_color(lv_color_t classic, lv_color_t cat) {
    return is_cat_theme() ? cat : classic;
}

static lv_color_t cat_card_shadow() {
    return LV_COLOR_MAKE(0xe9, 0xc8, 0xd8);
}

static lv_color_t tupi_warm_shadow() {
    return LV_COLOR_MAKE(0xd8, 0xcd, 0xbc);
}

#define COLOR_BG (theme().bg)
#define COLOR_SURFACE (theme().surface)
#define COLOR_SURFACE_2 (theme().surface2)
#define COLOR_TEXT (theme().text)
#define COLOR_CREAM (theme().cream)
#define COLOR_MUTED (theme().muted)
#define COLOR_LINE (theme().line)
#define COLOR_GOLD (theme().gold)
#define COLOR_GREEN (theme().green)
#define COLOR_PURPLE (theme().purple)
#define COLOR_BLUE (theme().blue)
#define COLOR_CLOCK_DOT (theme().clock_dot)
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

static lv_color_t cat_fur_shadow() { return LV_COLOR_MAKE(0xeb, 0x90, 0x42); }
static lv_color_t cat_nose_color() { return LV_COLOR_MAKE(0xff, 0x70, 0x76); }

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

static void fit_brand_label(lv_obj_t* label, int16_t width, bool owner) {
    const lv_font_t* font = lv_obj_get_style_text_font(label, 0);
    const int16_t line_height = font ? (lv_font_get_line_height(font) + 2) : 20;
    // Use WRAP mode to allow multi-line display for long text
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(label, 0, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    // Set max height for 2 lines
    lv_obj_set_style_max_height(label, line_height * 2, 0);
    if (owner) {
        lv_obj_set_style_text_color(label,
                                    is_tupi_warm_theme() ? COLOR_GREEN :
                                    (is_cat_theme() ? COLOR_PURPLE : COLOR_GOLD), 0);
    } else {
        lv_obj_set_style_text_color(label,
                                    is_tupi_warm_theme() ? COLOR_TEXT :
                                    (is_cat_theme() ? COLOR_TEXT : COLOR_CREAM), 0);
    }
}

static lv_obj_t* circle(lv_obj_t* parent, int16_t size, lv_color_t color, lv_opa_t opa);

struct FaceMetrics {
    int eye_x;
    int eye_y;
    int eye_w;
    int eye_h;
    int eye_h_speaking_base;
    int eye_h_speaking_amp;
    int eye_h_listening;
    int eye_h_blink;
    int eye_radius;
    int pupil_w;
    int pupil_h;
    int pupil_y;
    int pupil_move_x;
    int pupil_move_y;
    int highlight_x;
    int highlight_y;
    int highlight_size;
    int eyebrow_x;
    int eyebrow_y;
    int eyebrow_speaking_y;
    int eyebrow_listening_y;
    int eyebrow_sad_y;
    int mouth_y;
    int mouth_idle_w;
    int mouth_idle_h;
    int mouth_listening_w;
    int mouth_listening_h;
    int mouth_speaking_w;
    int mouth_speaking_w_amp;
    int mouth_speaking_h[6];
};

static FaceMetrics face_metrics() {
    if (is_cat_theme()) {
        return {
            42, -46,
            42, 48,
            43, 4,
            50,
            8,
            22,
            15, 18,
            5,
            4, 3,
            -6, -10,
            7,
            42, -72, -75, -78, -66,
            8,
            28, 7,
            40, 10,
            44, 8,
            {10, 17, 13, 19, 12, 16},
        };
    }

    return {
        80, -40,
        60, 70,
        65, 5,
        75,
        10,
        30,
        28, 35,
        4,
        6, 4,
        -8, -10,
        10,
        80, -90, -92, -95, -82,
        60,
        60, 16,
        70, 25,
        80, 15,
        {20, 35, 28, 40, 24, 32},
    };
}

static void create_tupi_dot_mark(lv_obj_t* parent, int16_t x, int16_t y, int16_t dot = 8, int16_t gap = 4) {
    const lv_color_t colors[4] = {
        COLOR_TEXT, COLOR_TEXT, COLOR_GREEN, COLOR_MUTED
    };
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 2; ++c) {
            lv_obj_t* d = circle(parent, dot, colors[r * 2 + c], LV_OPA_COVER);
            lv_obj_align(d, LV_ALIGN_TOP_LEFT, x + c * (dot + gap), y + r * (dot + gap));
        }
    }
}

static void create_brand_mark(lv_obj_t* parent, int16_t x = 18, int16_t y = 4,
                              lv_obj_t** logo_label = nullptr, lv_obj_t** owner_label = nullptr) {
    const auto profile = QdLoadUserProfile();
    if (is_cat_theme()) {
        lv_obj_t* brand_a = lv_label_create(parent);
        lv_label_set_text(brand_a, profile.logo.c_str());
        lv_obj_add_style(brand_a, &style_en, 0);
        lv_obj_set_style_text_font(brand_a, &font_puhui_16_4, 0);
        fit_brand_label(brand_a, 170, false);
        lv_obj_align(brand_a, LV_ALIGN_TOP_LEFT, x, y);
        add_gesture_bubble(brand_a);
        if (logo_label) {
            *logo_label = brand_a;
        }

        lv_obj_t* brand_b = lv_label_create(parent);
        lv_label_set_text(brand_b, profile.owner.c_str());
        lv_obj_add_style(brand_b, &style_gold, 0);
        lv_obj_set_style_text_font(brand_b, &font_puhui_16_4, 0);
        fit_brand_label(brand_b, 170, true);
        lv_obj_align_to(brand_b, brand_a, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
        add_gesture_bubble(brand_b);
        if (owner_label) {
            *owner_label = brand_b;
        }
        return;
    }

    if (is_tupi_warm_theme()) {
        create_tupi_dot_mark(parent, x, y + 2, 8, 4);

        lv_obj_t* brand = label_en(parent, profile.logo.c_str(), &style_en);
        lv_obj_set_style_text_font(brand, &font_puhui_16_4, 0);
        fit_brand_label(brand, 180, false);
        lv_obj_align(brand, LV_ALIGN_TOP_LEFT, x + 34, y);
        if (logo_label) {
            *logo_label = brand;
        }

        lv_obj_t* sub = label_en(parent, profile.owner.c_str(), &style_muted);
        lv_obj_set_style_text_font(sub, qd_cn_font_16(), 0);
        fit_brand_label(sub, 180, true);
        lv_obj_align_to(sub, brand, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
        if (owner_label) {
            *owner_label = sub;
        }
        return;
    }

    lv_obj_t* brand_a = label_en(parent, profile.logo.c_str(), &style_en);
    lv_obj_set_style_text_font(brand_a, &font_puhui_16_4, 0);
    fit_brand_label(brand_a, 170, false);
    lv_obj_align(brand_a, LV_ALIGN_TOP_LEFT, x, y);
    if (logo_label) {
        *logo_label = brand_a;
    }

    lv_obj_t* brand_b = label_en(parent, profile.owner.c_str(), &style_gold);
    lv_obj_set_style_text_font(brand_b, &font_puhui_16_4, 0);
    fit_brand_label(brand_b, 170, true);
    lv_obj_align_to(brand_b, brand_a, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
    if (owner_label) {
        *owner_label = brand_b;
    }
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
    lv_style_set_radius(&style_panel, is_tupi_warm_theme() ? 10 : 6);
    lv_style_set_pad_all(&style_panel, 0);
    if (is_tupi_warm_theme()) {
        lv_style_set_shadow_width(&style_panel, 8);
        lv_style_set_shadow_color(&style_panel, tupi_warm_shadow());
        lv_style_set_shadow_opa(&style_panel, LV_OPA_20);
        lv_style_set_shadow_ofs_y(&style_panel, 2);
    }

    lv_style_init(&style_clock_card);
    lv_style_set_bg_color(&style_clock_card,
                          is_tupi_warm_theme() ? COLOR_SURFACE :
                          themed_color(LV_COLOR_MAKE(0x08, 0x08, 0x08), COLOR_SURFACE));
    lv_style_set_bg_opa(&style_clock_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_clock_card,
                              is_tupi_warm_theme() ? COLOR_LINE :
                              themed_color(LV_COLOR_MAKE(0x2a, 0x28, 0x22), COLOR_LINE));
    lv_style_set_border_width(&style_clock_card, 1);
    lv_style_set_radius(&style_clock_card, is_tupi_warm_theme() ? 12 : 5);
    lv_style_set_pad_all(&style_clock_card, 0);
}

// ===== Animation callbacks =====
void DesktopUI::ObjOpaCb(void* obj, int32_t value) {
    auto* target = static_cast<lv_obj_t*>(obj);
    if (target && lv_obj_is_visible(target)) {
        lv_obj_set_style_opa(target, value, 0);
    }
}

void DesktopUI::ObjXCb(void* obj, int32_t value) {
    auto* target = static_cast<lv_obj_t*>(obj);
    if (target && lv_obj_is_visible(target)) {
        lv_obj_set_x(target, value);
    }
}

void DesktopUI::ObjYCb(void* obj, int32_t value) {
    auto* target = static_cast<lv_obj_t*>(obj);
    if (target && lv_obj_is_visible(target)) {
        lv_obj_set_y(target, value);
    }
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

static void SaveFocusStats(uint16_t count, uint32_t date_key) {
    nvs_handle_t handle;
    if (nvs_open("focus", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u16(handle, "completed", count);
        nvs_set_u32(handle, "date", date_key);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static uint16_t LoadFocusStats(uint32_t* date_key) {
    nvs_handle_t handle;
    uint16_t count = 0;
    if (date_key) {
        *date_key = 0;
    }
    if (nvs_open("focus", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u16(handle, "completed", &count);
        if (date_key) {
            nvs_get_u32(handle, "date", date_key);
        }
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
            self->ReconcileFocusDate(false);
            self->focus_completed_count_++;
            SaveFocusStats(self->focus_completed_count_, self->focus_count_date_);
            ESP_LOGI(TAG, "Focus session completed count=%u date=%lu",
                     self->focus_completed_count_,
                     static_cast<unsigned long>(self->focus_count_date_));
        } else {
            ESP_LOGI(TAG, "Focus break completed");
        }
    }
    self->UpdateFocusUI();
}

void DesktopUI::HourglassTickCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->hourglass_running_) {
        return;
    }
    if (self->hourglass_remaining_sec_ > 0) {
        self->hourglass_remaining_sec_--;
        if (self->hourglass_remaining_sec_ <= 5 || self->hourglass_remaining_sec_ % 60 == 0) {
            ESP_LOGI(TAG, "Hourglass tick remaining=%lu",
                     static_cast<unsigned long>(self->hourglass_remaining_sec_));
        }
    }
    if (self->hourglass_remaining_sec_ == 0) {
        self->hourglass_running_ = false;
        self->hourglass_done_ = true;
        if (!self->hourglass_alarm_played_) {
            self->hourglass_alarm_played_ = true;
            Application::GetInstance().Schedule([]() {
                ESP_LOGI(TAG, "Hourglass alarm sound requested");
                Application::GetInstance().PlayNotificationSound(Lang::Sounds::P3_SUCCESS);
            });
        }
        if (self->hourglass_tick_timer_) {
            lv_timer_pause(self->hourglass_tick_timer_);
        }
        if (self->hourglass_anim_timer_) {
            lv_timer_pause(self->hourglass_anim_timer_);
        }
        ESP_LOGI(TAG, "Hourglass timer done");
    }
    self->UpdateHourglassUI();
}

void DesktopUI::HourglassAnimCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->hourglass_running_) {
        return;
    }
    self->hourglass_anim_tick_++;
    static constexpr int16_t kStreamX[7] = {159, 156, 161, 158, 162, 157, 160};
    for (int i = 0; i < 7; ++i) {
        lv_obj_t* dot = self->hourglass_falling_dots_[i];
        if (!dot) {
            continue;
        }
        const int phase = (self->hourglass_anim_tick_ * 8 + i * 17) % 96;
        const lv_opa_t opa = phase < 78 ? static_cast<lv_opa_t>(LV_OPA_70 + ((i % 3) * 10))
                                        : static_cast<lv_opa_t>(LV_OPA_TRANSP);
        lv_obj_align(dot, LV_ALIGN_TOP_LEFT, kStreamX[i], 191 + phase);
        lv_obj_set_style_bg_opa(dot, opa, 0);
        lv_obj_set_style_shadow_opa(dot, opa > LV_OPA_50 ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }

    static constexpr int16_t kSparkleX[5] = {126, 141, 175, 191, 207};
    static constexpr int16_t kSparkleY[5] = {307, 295, 284, 301, 311};
    for (int i = 7; i < 12; ++i) {
        lv_obj_t* dot = self->hourglass_falling_dots_[i];
        if (!dot) {
            continue;
        }
        const int j = i - 7;
        const int pulse = (self->hourglass_anim_tick_ * 9 + j * 31) % 120;
        const lv_opa_t opa = pulse < 60 ? static_cast<lv_opa_t>(LV_OPA_30 + pulse / 2)
                                        : static_cast<lv_opa_t>(LV_OPA_60 - (pulse - 60) / 2);
        lv_obj_align(dot, LV_ALIGN_TOP_LEFT, kSparkleX[j] + ((pulse / 30) & 1), kSparkleY[j]);
        lv_obj_set_style_bg_opa(dot, opa, 0);
        lv_obj_set_style_shadow_opa(dot, LV_OPA_20, 0);
    }
}

// ===== Page navigation =====
static DesktopUI* g_desktop_ui = nullptr;

static void navigate_back_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->NavigateBack();
    }
}

static void qr_overlay_close_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->HideQrCode();
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
                if (g_desktop_ui->fc_stop_other_media_) {
                    g_desktop_ui->fc_stop_other_media_();
                }
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
        case 8:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::MUSIC);
            }
            break;
        case 9:
            if (g_desktop_ui) {
                if (g_desktop_ui->podcast_stop_other_media_) {
                    g_desktop_ui->podcast_stop_other_media_();
                }
                g_desktop_ui->ShowPage(DesktopPage::PODCAST);
                g_desktop_ui->ShowPodcastDetail(false);
                if (g_desktop_ui->podcast_activate_) {
                    g_desktop_ui->podcast_activate_();
                }
            }
            break;
        case 10:
            if (g_desktop_ui) {
                g_desktop_ui->ShowPage(DesktopPage::SHAKE_LAB);
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

static void music_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(8);
    }
}

static void radio_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_app_card(0);
    }
}

static void podcast_card_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        if (g_desktop_ui->podcast_stop_other_media_) {
            g_desktop_ui->podcast_stop_other_media_();
        }
        g_desktop_ui->ShowPage(DesktopPage::PODCAST);
        g_desktop_ui->ShowPodcastDetail(false);
        if (g_desktop_ui->podcast_activate_) {
            g_desktop_ui->podcast_activate_();
        }
    }
}

static void podcast_open_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ShowPodcastDetail(true);
    }
}

static void podcast_list_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ShowPodcastDetail(false);
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
    if (!indev || !g_desktop_ui) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_RIGHT) {
        g_desktop_ui->ShowPage(DesktopPage::MAIN);
    } else if (dir == LV_DIR_LEFT) {
        g_desktop_ui->ShowPage(DesktopPage::MEDIA);
    }
}

static void media_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev || !g_desktop_ui) {
        return;
    }
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void podcast_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::MEDIA);
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

static void music_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

static void music_talk_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->StartMusicAsk();
    }
}

static void music_face_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->StartMusicAsk();
    }
}

static void music_again_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->ReplayMusicRecent(0);
    }
}

static void music_stop_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        if (g_desktop_ui->radio_stop_) {
            g_desktop_ui->radio_stop_();
        }
        g_desktop_ui->ClearMusicLyric();
    }
}

static void music_pause_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui &&
        g_desktop_ui->TryAcceptMusicControlTap() && g_desktop_ui->music_pause_) {
        g_desktop_ui->music_pause_();
    }
}

static void music_play_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui &&
        g_desktop_ui->TryAcceptMusicControlTap() && g_desktop_ui->music_play_) {
        g_desktop_ui->music_play_();
    }
}

static void music_next_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui &&
        g_desktop_ui->TryAcceptMusicControlTap()) {
        g_desktop_ui->ReplayNextMusicRecent();
    }
}

static void music_recent_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        const auto index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
        g_desktop_ui->ReplayMusicRecent(static_cast<size_t>(index));
    }
}

static void music_recent_clear_cb(lv_event_t* event) {
    if (!g_desktop_ui) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CLICKED) {
        g_desktop_ui->ClearMusicRecent();
    }
}

static void music_recent_remove_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED && g_desktop_ui) {
        const auto index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
        g_desktop_ui->RemoveMusicRecent(static_cast<size_t>(index));
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

static void podcast_prev_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->podcast_prev_) {
        g_desktop_ui->podcast_prev_();
    }
}

static void podcast_play_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->podcast_play_pause_) {
        g_desktop_ui->podcast_play_pause_();
    }
}

static void podcast_stop_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->podcast_stop_) {
        g_desktop_ui->podcast_stop_();
    }
}

static void podcast_next_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->podcast_next_) {
        g_desktop_ui->podcast_next_();
    }
}

static void podcast_up_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->podcast_up_) {
        g_desktop_ui->podcast_up_();
    }
}

static void podcast_down_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui && g_desktop_ui->podcast_down_) {
        g_desktop_ui->podcast_down_();
    }
}

static void podcast_seek_cb(lv_event_t* event) {
    if (g_desktop_ui) {
        g_desktop_ui->HandlePodcastSeekEvent(event);
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

static void diagnostics_gesture_cb(lv_event_t* event) {
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

static void diagnostics_open_cb(lv_event_t* event) {
    if ((lv_event_get_code(event) == LV_EVENT_LONG_PRESSED ||
         lv_event_get_code(event) == LV_EVENT_CLICKED) && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::DIAGNOSTICS);
    }
}

static void settings_theme_cb(lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && g_desktop_ui) {
        g_desktop_ui->CycleTheme();
    }
}

static void main_gesture_cb(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_LEFT && g_desktop_ui) {
        g_desktop_ui->ShowPage(DesktopPage::APPS);
    }
}

// ===== DesktopUI implementation =====
void DesktopUI::Create() {
    g_desktop_ui = this;
    load_theme();
    init_styles();

    lv_obj_t* root = lv_scr_act();
    lv_obj_set_style_bg_color(root, COLOR_BG, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    brand_label_count_ = 0;
    brand_earth_gif_ = nullptr;
    memset(brand_logo_labels_, 0, sizeof(brand_logo_labels_));
    memset(brand_owner_labels_, 0, sizeof(brand_owner_labels_));
    memset(status_bar_time_labels_, 0, sizeof(status_bar_time_labels_));
    memset(status_bar_battery_labels_, 0, sizeof(status_bar_battery_labels_));
    memset(app_status_labels_, 0, sizeof(app_status_labels_));
    memset(app_status_dots_, 0, sizeof(app_status_dots_));
    memset(music_recent_buttons_, 0, sizeof(music_recent_buttons_));
    memset(music_recent_labels_, 0, sizeof(music_recent_labels_));
    memset(diagnostics_labels_, 0, sizeof(diagnostics_labels_));
    LoadMusicRecent();

    CreateMainPage(root);
    CreateAppsPage(root);
    CreatePhotoPage(root);
    CreateFcPage(root);
    CreateCalendarPage(root);
    CreateRadioPage(root);
    CreateMusicPage(root);
    CreateMediaPage(root);
    CreatePodcastPage(root);
    CreateFocusPage(root);
    CreateXiaozhiPage(root);
    CreateNetworkPage(root);
    CreateSettingsPage(root);
    CreateDiagnosticsPage(root);
    CreateQrOverlay(root);

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
    const bool was_xiaozhi = current_page_ == DesktopPage::XIAOZHI;
    const bool was_shake_lab = current_page_ == DesktopPage::SHAKE_LAB;
    current_page_ = page;
    if (was_xiaozhi && page != DesktopPage::XIAOZHI) {
        ReleaseThemedFaceGif();
    }
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
    if (hourglass_page_) {
        lv_obj_add_flag(hourglass_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (shake_lab_page_) {
        lv_obj_add_flag(shake_lab_page_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(radio_page_, LV_OBJ_FLAG_HIDDEN);
    if (music_page_) {
        lv_obj_add_flag(music_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (media_page_) {
        lv_obj_add_flag(media_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (podcast_page_) {
        lv_obj_add_flag(podcast_page_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
    if (network_page_) {
        lv_obj_add_flag(network_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings_page_) {
        lv_obj_add_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (diagnostics_page_) {
        lv_obj_add_flag(diagnostics_page_, LV_OBJ_FLAG_HIDDEN);
    }

    switch (page) {
        case DesktopPage::MAIN:
            lv_obj_clear_flag(main_page_, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Show main page");
            break;
        case DesktopPage::APPS:
            SetAppsMoreVisible(false);
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
        case DesktopPage::MUSIC:
            if (music_page_) {
                lv_obj_clear_flag(music_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Show music page");
            break;
        case DesktopPage::MEDIA:
            if (media_page_) {
                lv_obj_clear_flag(media_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Show media page");
            break;
        case DesktopPage::PODCAST:
            if (podcast_page_) {
                lv_obj_clear_flag(podcast_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Show podcast page");
            break;
        case DesktopPage::FOCUS:
            if (focus_page_) {
                lv_obj_clear_flag(focus_page_, LV_OBJ_FLAG_HIDDEN);
                UpdateFocusUI();
            }
            ESP_LOGI(TAG, "Show focus page");
            break;
        case DesktopPage::HOURGLASS:
            if (!hourglass_page_) {
                CreateHourglassPage(lv_scr_act());
            }
            if (hourglass_page_) {
                lv_obj_clear_flag(hourglass_page_, LV_OBJ_FLAG_HIDDEN);
                UpdateHourglassUI();
            }
            ESP_LOGI(TAG, "Show hourglass page");
            break;
        case DesktopPage::SHAKE_LAB:
            if (!shake_lab_page_) {
                CreateShakeLabPage(lv_scr_act());
            }
            if (shake_lab_page_) {
                lv_obj_clear_flag(shake_lab_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Shake Lab page enter");
            break;
        case DesktopPage::XIAOZHI:
            EnsureThemedFaceGif();
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
        case DesktopPage::DIAGNOSTICS:
            if (diagnostics_page_) {
                RefreshDiagnostics();
                lv_obj_clear_flag(diagnostics_page_, LV_OBJ_FLAG_HIDDEN);
            }
            ESP_LOGI(TAG, "Show diagnostics page");
            break;
    }

    // Do not keep weather particles mutating hidden objects behind full-screen apps.
    // They are resumed by ApplyWeatherVisual when the main-page weather is updated.
    if (weather_particle_timer_ && page != DesktopPage::MAIN) {
        lv_timer_pause(weather_particle_timer_);
    }

    // Hidden media pages must not keep mutating dozens of LVGL objects.  The
    // radio spectrum alone touched 16 bars every 100 ms and competed with the
    // MP3 decoder even while XiaoZhi's lyric page covered it.
    if (radio_anim_timer_) {
        if (page == DesktopPage::RADIO) {
            lv_timer_resume(radio_anim_timer_);
        } else {
            lv_timer_pause(radio_anim_timer_);
        }
    }
    if (music_cover_timer_) {
        if (page == DesktopPage::MUSIC) {
            lv_timer_resume(music_cover_timer_);
        } else {
            lv_timer_pause(music_cover_timer_);
        }
    }

    if (was_shake_lab && page != DesktopPage::SHAKE_LAB) {
        ESP_LOGI(TAG, "Shake Lab page exit");
        ReleaseShakeLabPage();
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
    // Changing page-root hidden flags already invalidates the affected areas.
    // Avoid forcing an extra full 480x320 redraw on every page transition.
}

void DesktopUI::HandleSwipe(int16_t dx, int16_t dy) {
    const int16_t min_dx = 70;
    const int16_t max_dy = 90;

    if (current_page_ == DesktopPage::PHOTO && LV_ABS(dx) > 35 && LV_ABS(dx) > LV_ABS(dy)) {
        NavigateBack();
        return;
    }

    if (LV_ABS(dy) >= max_dy) {
        return;
    }

    if (current_page_ == DesktopPage::MAIN && dx < -min_dx) {
        ShowPage(DesktopPage::APPS);
    } else if (current_page_ == DesktopPage::APPS && LV_ABS(dx) > min_dx) {
        NavigateBack();
    } else if (current_page_ != DesktopPage::MAIN && current_page_ != DesktopPage::APPS && LV_ABS(dx) > min_dx) {
        NavigateBack();
    }
}

void DesktopUI::NavigateBack() {
    if (current_page_ == DesktopPage::MAIN) {
        return;
    }
    if (current_page_ == DesktopPage::FOCUS && focus_auto_rotated_) {
        if (focus_rotation_callback_) {
            focus_rotation_callback_(false);
        }
        focus_auto_rotated_ = false;
        ESP_LOGI(TAG, "Focus display rotation restored");
    }

    DesktopPage target = DesktopPage::APPS;
    if (current_page_ == DesktopPage::APPS) {
        target = DesktopPage::MAIN;
    } else if (current_page_ == DesktopPage::MEDIA) {
        target = DesktopPage::APPS;
    } else if (current_page_ == DesktopPage::PODCAST) {
        target = DesktopPage::MEDIA;
    }
    ESP_LOGI(TAG, "Navigate back page=%d target=%d",
             static_cast<int>(current_page_), static_cast<int>(target));
    ShowPage(target);
}

void DesktopUI::HandleTouchRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y,
                                   int64_t duration_ms) {
    const int16_t dx = static_cast<int16_t>(end_x) - static_cast<int16_t>(start_x);
    const int16_t dy = static_cast<int16_t>(end_y) - static_cast<int16_t>(start_y);
    constexpr int64_t kTapMaxDurationMs = 350;
    constexpr int64_t kSwipeMaxDurationMs = 900;

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

    if (current_page_ == DesktopPage::PHOTO && LV_ABS(dx) > 35 && LV_ABS(dx) > LV_ABS(dy)) {
        photo_segment_swipe_active_ = false;
        HandleSwipe(dx, dy);
        return;
    }

    if (current_page_ == DesktopPage::PHOTO) {
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (!photo_segment_swipe_active_ || now_ms - photo_segment_last_ms_ > 850) {
            photo_segment_swipe_active_ = true;
            photo_segment_start_x_ = end_x;
            photo_segment_start_y_ = end_y;
            photo_segment_min_x_ = end_x;
            photo_segment_max_x_ = end_x;
        } else {
            photo_segment_min_x_ = std::min<uint16_t>(photo_segment_min_x_, end_x);
            photo_segment_max_x_ = std::max<uint16_t>(photo_segment_max_x_, end_x);
            const int16_t segment_dx = static_cast<int16_t>(end_x) - static_cast<int16_t>(photo_segment_start_x_);
            const int16_t segment_dy = static_cast<int16_t>(end_y) - static_cast<int16_t>(photo_segment_start_y_);
            const uint16_t span_x = photo_segment_max_x_ - photo_segment_min_x_;
            if (span_x > 110 && LV_ABS(segment_dx) > 70 && LV_ABS(segment_dy) < 90) {
                ESP_LOGI(TAG, "Photo segmented swipe dx=%d dy=%d span=%u",
                         segment_dx, segment_dy, span_x);
                photo_segment_swipe_active_ = false;
                HandleSwipe(segment_dx, segment_dy);
                return;
            }
        }
        photo_segment_last_ms_ = now_ms;
        return;
    }
    photo_segment_swipe_active_ = false;

    if (duration_ms < kTapMaxDurationMs && LV_ABS(dx) < 30 && LV_ABS(dy) < 30) {
        HandleTap(end_x, end_y);
    } else if (duration_ms < kSwipeMaxDurationMs) {
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
    auto hit = [x, y](uint16_t left, uint16_t top, uint16_t width, uint16_t height) {
        return x >= left && x < left + width && y >= top && y < top + height;
    };

    if (current_page_ == DesktopPage::HOURGLASS) {
        HandleHourglassTap(x, y);
        return;
    }

    if (current_page_ == DesktopPage::SHAKE_LAB) {
        HandleShakeLabTap(x, y);
        return;
    }

    if (current_page_ == DesktopPage::MAIN) {
        if (x >= 376 && x < 470 && y >= 8 && y < 62) {
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
        // Use the board's completed-tap path for radio controls.  The touch
        // controller can briefly reset on I2C errors, which means LVGL may see a
        // press without the matching release and never emit LV_EVENT_CLICKED.
        // Radio buttons intentionally have no LVGL callbacks (see CreateRadioPage),
        // so each completed tap is dispatched exactly once here.
        if (hit(372, 28, 96, 56)) {
            ESP_LOGI(TAG, "Radio Back tap");
            NavigateBack();
        } else if (hit(24, 180, 76, 32) && radio_prev_) {
            ESP_LOGI(TAG, "Radio Prev tap");
            radio_prev_();
        } else if (hit(124, 180, 76, 32) && radio_play_pause_) {
            ESP_LOGI(TAG, "Radio PlayPause tap");
            radio_play_pause_();
        } else if (hit(224, 180, 76, 32) && radio_stop_) {
            ESP_LOGI(TAG, "Radio Stop tap");
            radio_stop_();
        } else if (hit(324, 180, 76, 32) && radio_next_) {
            ESP_LOGI(TAG, "Radio Next tap");
            radio_next_();
        }
        return;
    }

    if (current_page_ == DesktopPage::MUSIC) {
        if (hit(404, 4, 70, 32)) {
            NavigateBack();
        } else if (hit(414, 36, 52, 22) && music_pause_) {
            music_pause_();
        } else if (hit(414, 64, 52, 22) && music_play_) {
            music_play_();
        } else if (hit(414, 92, 52, 22) && music_next_) {
            music_next_();
        } else if (hit(18, 18, 128, 112)) {
            StartMusicAsk();
        } else if (hit(252, 224, 58, 22)) {
            ClearMusicRecent();
        } else if (hit(330, 226, 134, 24)) {
            StartMusicAsk();
        } else if (hit(330, 258, 134, 24)) {
            ReplayNextMusicRecent();
        } else if (hit(330, 290, 134, 24)) {
            ClearMusicLyric();
            if (music_pause_) {
                music_pause_();
            }
        } else if (x >= 18 && x < 308 && y >= 248 && y < 248 + kMusicRecentCount * 22) {
            const size_t index = (y - 248) / 22;
            if (index < kMusicRecentCount) {
                ReplayMusicRecent(index);
            }
        }
        return;
    }

    if (current_page_ == DesktopPage::MEDIA) {
        if (hit(372, 28, 96, 56)) {
            NavigateBack();
        } else if (hit(24, 92, 432, 160)) {
            open_app_card(9);
        }
        return;
    }

    if (current_page_ == DesktopPage::PODCAST) {
        if (!podcast_detail_view_) {
            if (hit(12, 276, 76, 32)) {
                NavigateBack();
            } else if (hit(138, 276, 76, 32) && podcast_up_) {
                podcast_up_();
            } else if (hit(264, 276, 76, 32)) {
                ShowPodcastDetail(true);
            } else if (hit(392, 276, 76, 32) && podcast_down_) {
                podcast_down_();
            }
        } else {
            if (hit(12, 276, 76, 32)) {
                ShowPodcastDetail(false);
            } else if (hit(88, 276, 76, 32) && podcast_prev_) {
                podcast_prev_();
            } else if (hit(166, 276, 76, 32) && podcast_play_pause_) {
                podcast_play_pause_();
            } else if (hit(244, 276, 76, 32) && podcast_stop_) {
                podcast_stop_();
            } else if (hit(392, 276, 76, 32) && podcast_next_) {
                podcast_next_();
            }
        }
        return;
    }

    if (current_page_ == DesktopPage::PHOTO) {
        return;
    }

    if (current_page_ == DesktopPage::FC) {
        return;
    }

    if (current_page_ == DesktopPage::CALENDAR) {
        if (hit(376, 36, 76, 28) || hit(192, 278, 96, 34)) {
            ShowTodayCalendar();
        } else if (hit(18, 278, 96, 34)) {
            AdjustCalendarMonth(-1);
        } else if (hit(366, 278, 96, 34)) {
            AdjustCalendarMonth(1);
        }
        return;
    }

    if (current_page_ == DesktopPage::FOCUS) {
        uint16_t fx = x;
        uint16_t fy = y;
        if (focus_auto_rotated_) {
            fx = 480 - 1 - fx;
            fy = 320 - 1 - fy;
        }
        ESP_LOGI(TAG, "Focus tap fallback raw=%u,%u logical=%u,%u rotated=%d",
                 x, y, fx, fy, focus_auto_rotated_);
        if (fx >= 414 && fx < 466 && fy >= 42 && fy < 66) {
            ESP_LOGI(TAG, "Focus tap fallback back");
            NavigateBack();
            return;
        }
        if (fx >= 342 && fx < 450 && fy >= 82 && fy < 124) {
            ESP_LOGI(TAG, "Focus tap fallback work");
            SetFocusMode(true);
            return;
        }
        if (fx >= 342 && fx < 450 && fy >= 134 && fy < 176) {
            ESP_LOGI(TAG, "Focus tap fallback break");
            SetFocusMode(false);
            return;
        }
        if (fx >= 128 && fx < 260 && fy >= 262 && fy < 306) {
            ESP_LOGI(TAG, "Focus tap fallback start");
            ToggleFocusTimer();
            return;
        }
        if (fx >= 276 && fx < 388 && fy >= 262 && fy < 306) {
            ESP_LOGI(TAG, "Focus tap fallback reset");
            ResetFocusTimer();
            return;
        }
        return;
    }

    if (current_page_ == DesktopPage::SETTINGS) {
        if (x >= 360 && x < 470 && y >= 35 && y < 90) {
            NavigateBack();
        }
        return;
    }

    if (current_page_ == DesktopPage::NETWORK || current_page_ == DesktopPage::DIAGNOSTICS) {
        if (hit(372, 28, 96, 56)) {
            NavigateBack();
        }
        return;
    }

    if (current_page_ != DesktopPage::APPS) {
        return;
    }

    if (hit(372, 28, 96, 56)) {
        NavigateBack();
        return;
    }
    if (hit(248, 42, 102, 30)) {
        SetAppsMoreVisible(!apps_showing_more_);
        return;
    }
    if (apps_showing_more_) {
        if (hit(24, 90, 432, 142)) {
            ShowPage(DesktopPage::SHAKE_LAB);
        }
        return;
    }
    if (x >= 24 && x < 446 && y >= 76 && y < 76 + 5 * 45) {
        const uint8_t col = x >= 242 ? 1 : 0;
        const uint8_t row = (y - 76) / 45;
        const uint8_t index = row * 2 + col;
        const uint16_t tile_x = 24 + col * 218;
        const uint16_t tile_y = 76 + row * 45;
        if (index < 10 && hit(tile_x, tile_y, 204, 42)) {
            ESP_LOGI(TAG, "Apps tap fallback index=%u", index);
            open_app_card(index);
        }
    }
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
    auto attach_brand_earth = [this](lv_obj_t* logo, int16_t x_offset = 6, int16_t y_offset = 7) {
        if (!logo) {
            return;
        }
        brand_earth_gif_ = lv_gif_create(main_page_);
        lv_gif_set_src(brand_earth_gif_, &qd_brand_earth);
        lv_obj_set_size(brand_earth_gif_, 46, 46);
        lv_obj_set_style_bg_opa(brand_earth_gif_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(brand_earth_gif_, 0, 0);
        lv_obj_align_to(brand_earth_gif_, logo, LV_ALIGN_OUT_RIGHT_MID, x_offset, y_offset);
        add_gesture_bubble(brand_earth_gif_);
    };

    if (is_tupi_warm_theme()) {
        lv_obj_t* logo = nullptr;
        lv_obj_t* owner = nullptr;
        create_brand_mark(main_page_, 20, 10, &logo, &owner);
        RegisterBrandLabels(logo, owner);
        attach_brand_earth(logo, 6, 7);

        lv_obj_t* wifi = label_en(main_page_, "WiFi", &style_en);
        lv_obj_set_style_text_font(wifi, &lv_font_montserrat_14, 0);
        lv_obj_align(wifi, LV_ALIGN_TOP_LEFT, 278, 14);

        lv_obj_t* battery = label_en(main_page_, "--%", &style_en);
        lv_obj_set_style_text_color(battery, COLOR_TEXT, 0);
        lv_obj_align(battery, LV_ALIGN_TOP_LEFT, 340, 14);
        for (size_t i = 0; i < sizeof(status_bar_battery_labels_) / sizeof(status_bar_battery_labels_[0]); ++i) {
            if (!status_bar_battery_labels_[i]) {
                status_bar_battery_labels_[i] = battery;
                break;
            }
        }
        SetBatteryStatus(battery_level_, battery_charging_, battery_level_ >= 0);
    } else if (is_cat_theme()) {
        lv_obj_t* logo = nullptr;
        lv_obj_t* owner = nullptr;
        create_brand_mark(main_page_, 20, 4, &logo, &owner);
        RegisterBrandLabels(logo, owner);
        attach_brand_earth(logo, 6, 7);
    } else {
        const auto profile = QdLoadUserProfile();
        lv_obj_t* brand_a = label_en(main_page_, profile.logo.c_str(), &style_en);
        lv_obj_set_style_text_font(brand_a, qd_cn_font_20(), 0);
        fit_brand_label(brand_a, 170, false);
        lv_obj_align(brand_a, LV_ALIGN_TOP_LEFT, 20, 10);

        lv_obj_t* brand_b = label_en(main_page_, profile.owner.c_str(), &style_gold);
        lv_obj_set_style_text_font(brand_b, qd_cn_font_16(), 0);
        fit_brand_label(brand_b, 170, true);
        lv_obj_align_to(brand_b, brand_a, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
        RegisterBrandLabels(brand_a, brand_b);
        attach_brand_earth(brand_a, 6, 7);
    }

    CreateBigTime(main_page_);

    // Date and weekday
    date_label_ = label_en(main_page_, "01 / 01     |", &style_en);
    lv_obj_set_style_text_color(date_label_, is_cat_theme() ? COLOR_TEXT : COLOR_CREAM, 0);
    lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(date_label_, LV_ALIGN_TOP_LEFT, 52, 174);

    week_label_ = label_en(main_page_, "MON", &style_green);
    lv_obj_set_style_text_font(week_label_, qd_cn_font_20(), 0);
    lv_obj_align(week_label_, LV_ALIGN_TOP_LEFT, 178, 174);

    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(date_label_, COLOR_TEXT, 0);
        lv_obj_set_style_text_font(date_label_, &lv_font_montserrat_20, 0);
        lv_obj_align(date_label_, LV_ALIGN_TOP_LEFT, 60, 165);
        lv_obj_set_style_text_color(week_label_, COLOR_GREEN, 0);
        lv_obj_set_style_text_font(week_label_, qd_cn_font_20(), 0);
        lv_obj_align(week_label_, LV_ALIGN_TOP_LEFT, 182, 164);
    }

    CreateWeatherPanel(main_page_);
    CreateQuotePanel(main_page_);

    // Menu button
    lv_obj_t* menu = CreateButton(main_page_, "Menu", show_apps_cb);
    if (is_tupi_warm_theme()) {
        lv_obj_set_size(menu, 58, 30);
        lv_obj_set_style_bg_color(menu, COLOR_SURFACE, 0);
        lv_obj_set_style_border_color(menu, COLOR_LINE, 0);
        lv_obj_set_style_radius(menu, 15, 0);
    }
    lv_obj_align(menu, LV_ALIGN_TOP_RIGHT, -18, 10);
}

void DesktopUI::CreateBigTime(lv_obj_t* parent) {
    lv_obj_t* time_group = lv_obj_create(parent);
    lv_obj_remove_style_all(time_group);
    lv_obj_set_size(time_group, is_tupi_warm_theme() ? 254 : 254,
                    is_tupi_warm_theme() ? 142 : (is_cat_theme() ? 106 : 154));
    lv_obj_align(time_group, LV_ALIGN_TOP_LEFT, 20,
                 is_tupi_warm_theme() ? 58 : (is_cat_theme() ? 66 : 18));
    lv_obj_clear_flag(time_group, LV_OBJ_FLAG_SCROLLABLE);
    if (is_cat_theme() || is_tupi_warm_theme()) {
        lv_obj_set_style_radius(time_group, is_tupi_warm_theme() ? 12 : 18, 0);
        lv_obj_set_style_bg_color(time_group, COLOR_SURFACE, 0);
        lv_obj_set_style_bg_opa(time_group, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(time_group, COLOR_LINE, 0);
        lv_obj_set_style_border_width(time_group, 1, 0);
        lv_obj_set_style_shadow_width(time_group, is_tupi_warm_theme() ? 8 : 14, 0);
        lv_obj_set_style_shadow_color(time_group,
                                      is_tupi_warm_theme() ? tupi_warm_shadow() : cat_card_shadow(), 0);
        lv_obj_set_style_shadow_opa(time_group, is_tupi_warm_theme() ? LV_OPA_20 : LV_OPA_40, 0);
    }
    add_gesture_bubble(time_group);

    clock_hour_label_ = lv_label_create(time_group);
    lv_label_set_text(clock_hour_label_, "00");
    lv_obj_set_size(clock_hour_label_, is_tupi_warm_theme() ? 104 : 108,
                    is_tupi_warm_theme() ? 78 : (is_cat_theme() ? 78 : 60));
    lv_obj_set_style_text_font(clock_hour_label_, &qd_font_clock_72, 0);
    lv_obj_set_style_text_color(clock_hour_label_,
                                is_tupi_warm_theme() ? COLOR_TEXT :
                                (is_cat_theme() ? COLOR_PURPLE : COLOR_CREAM), 0);
    lv_obj_set_style_text_align(clock_hour_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(clock_hour_label_, LV_ALIGN_TOP_LEFT,
                 is_tupi_warm_theme() ? 18 : 0,
                 is_tupi_warm_theme() ? 22 : (is_cat_theme() ? 18 : 77));
    add_gesture_bubble(clock_hour_label_);

    clock_minute_label_ = lv_label_create(time_group);
    lv_label_set_text(clock_minute_label_, "00");
    lv_obj_set_size(clock_minute_label_, is_tupi_warm_theme() ? 100 : 110,
                    is_tupi_warm_theme() ? 78 : (is_cat_theme() ? 78 : 60));
    lv_obj_set_style_text_font(clock_minute_label_, &qd_font_clock_72, 0);
    lv_obj_set_style_text_color(clock_minute_label_, COLOR_GOLD, 0);
    lv_obj_set_style_text_align(clock_minute_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(clock_minute_label_, LV_ALIGN_TOP_LEFT,
                 is_tupi_warm_theme() ? 150 : 142,
                 is_tupi_warm_theme() ? 22 : (is_cat_theme() ? 18 : 77));
    add_gesture_bubble(clock_minute_label_);

    if (is_tupi_warm_theme()) {
        lv_obj_t* colon = lv_label_create(time_group);
        lv_label_set_text(colon, ":");
        lv_obj_set_style_text_font(colon, &qd_font_clock_72, 0);
        lv_obj_set_style_text_color(colon, COLOR_TEXT, 0);
        lv_obj_set_size(colon, 26, 78);
        lv_obj_set_style_text_align(colon, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(colon, LV_ALIGN_TOP_LEFT, 124, 22);
        add_gesture_bubble(colon);
    } else {
        clock_colon_dots_[0] = circle(time_group, 18, COLOR_CLOCK_DOT, LV_OPA_COVER);
        lv_obj_align(clock_colon_dots_[0], LV_ALIGN_TOP_LEFT, 118, is_cat_theme() ? 18 : 73);
        clock_colon_dots_[1] = circle(time_group, 18, COLOR_CLOCK_DOT, LV_OPA_COVER);
        lv_obj_align(clock_colon_dots_[1], LV_ALIGN_TOP_LEFT, 118, is_cat_theme() ? 60 : 116);
        lv_timer_create(ColonTimerCb, 500, this);
    }
    if (is_tupi_warm_theme()) {
        lv_obj_t* divider = bar(time_group, 190, 1, COLOR_LINE, LV_OPA_70);
        lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 32, 96);
        lv_obj_t* dot = circle(time_group, 5, COLOR_GREEN, LV_OPA_COVER);
        lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 124, 94);
        lv_obj_t* date_pill = bar(time_group, 196, 28, COLOR_SURFACE_2, LV_OPA_COVER);
        lv_obj_set_style_border_color(date_pill, COLOR_LINE, 0);
        lv_obj_set_style_border_width(date_pill, 1, 0);
        lv_obj_align(date_pill, LV_ALIGN_TOP_LEFT, 30, 106);
    }
    RenderBigTime(0, 0, false);
}

void DesktopUI::CreateWeatherPanel(lv_obj_t* parent) {
    lv_obj_t* panel = CreatePanel(parent, 166, 154, 294, 50);
    lv_obj_set_style_bg_color(panel, COLOR_SURFACE_2, 0);
    lv_obj_set_style_clip_corner(panel, true, 0);

    lv_obj_t* title = label_en(panel, "Weather", &style_muted);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    weather_horizon_ = bar(panel, 132, 1, COLOR_LINE, LV_OPA_60);
    lv_obj_align(weather_horizon_, LV_ALIGN_TOP_LEFT, 17, 105);

    weather_glow_ = circle(panel, 82, COLOR_GOLD, LV_OPA_20);
    lv_obj_align(weather_glow_, LV_ALIGN_TOP_LEFT, 38, 18);

    const int16_t ray_pos[6][3] = {
        {78, 22, 0}, {108, 34, 450}, {112, 66, 900},
        {76, 92, 0}, {44, 70, 450}, {42, 36, 900},
    };
    for (int i = 0; i < 6; ++i) {
        weather_rays_[i] = bar(panel, 4, 16, COLOR_GOLD, LV_OPA_60);
        lv_obj_align(weather_rays_[i], LV_ALIGN_TOP_LEFT, ray_pos[i][0], ray_pos[i][1]);
        lv_obj_set_style_transform_rotation(weather_rays_[i], ray_pos[i][2], 0);
    }

    weather_sun_ = circle(panel, 46, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align(weather_sun_, LV_ALIGN_TOP_LEFT, 58, 42);
    lv_obj_t* sun_highlight = circle(weather_sun_, 14, COLOR_CREAM, LV_OPA_40);
    lv_obj_align(sun_highlight, LV_ALIGN_TOP_LEFT, 9, 8);

    weather_cloud_shadow_ = bar(panel, 94, 24, COLOR_LINE, LV_OPA_30);
    lv_obj_align(weather_cloud_shadow_, LV_ALIGN_TOP_LEFT, 36, 82);

    weather_cloud_[0] = circle(panel, 40, COLOR_CREAM, LV_OPA_COVER);
    lv_obj_align(weather_cloud_[0], LV_ALIGN_TOP_LEFT, 38, 68);
    weather_cloud_[1] = circle(panel, 50, COLOR_CREAM, LV_OPA_COVER);
    lv_obj_align(weather_cloud_[1], LV_ALIGN_TOP_LEFT, 67, 58);
    weather_cloud_[2] = bar(panel, 96, 28, COLOR_CREAM, LV_OPA_COVER);
    lv_obj_align(weather_cloud_[2], LV_ALIGN_TOP_LEFT, 34, 84);
    if (is_tupi_warm_theme()) {
        for (int i = 0; i < 3; ++i) {
            lv_obj_set_style_border_color(weather_cloud_[i], COLOR_LINE, 0);
            lv_obj_set_style_border_width(weather_cloud_[i], 1, 0);
        }
        lv_obj_set_style_bg_opa(weather_glow_, LV_OPA_30, 0);
    }

    for (int i = 0; i < 6; ++i) {
        weather_rain_[i] = bar(panel, 3, i % 2 == 0 ? 15 : 11, COLOR_BLUE, LV_OPA_70);
        lv_obj_align(weather_rain_[i], LV_ALIGN_TOP_LEFT, 40 + i * 15, 86 + (i % 2) * 5);
        lv_obj_set_style_transform_rotation(weather_rain_[i], 180, 0);

        weather_snow_[i] = circle(panel, i % 2 == 0 ? 7 : 5, COLOR_CREAM, LV_OPA_COVER);
        lv_obj_align(weather_snow_[i], LV_ALIGN_TOP_LEFT, 40 + i * 15, 88 + (i % 3) * 5);
    }

    weather_storm_[0] = circle(panel, 74, COLOR_GOLD, LV_OPA_20);
    lv_obj_align(weather_storm_[0], LV_ALIGN_TOP_LEFT, 45, 39);
    lv_obj_move_background(weather_storm_[0]);
    weather_storm_[1] = bar(panel, 24, 4, COLOR_GOLD, LV_OPA_70);
    lv_obj_align(weather_storm_[1], LV_ALIGN_TOP_LEFT, 70, 70);
    lv_obj_set_style_transform_rotation(weather_storm_[1], 250, 0);
    weather_storm_[2] = bar(panel, 18, 3, COLOR_GOLD, LV_OPA_50);
    lv_obj_align(weather_storm_[2], LV_ALIGN_TOP_LEFT, 91, 60);
    lv_obj_set_style_transform_rotation(weather_storm_[2], 650, 0);
    weather_storm_[3] = bar(panel, 16, 3, COLOR_CREAM, LV_OPA_50);
    lv_obj_align(weather_storm_[3], LV_ALIGN_TOP_LEFT, 56, 82);
    lv_obj_set_style_transform_rotation(weather_storm_[3], 420, 0);

    weather_scene_gif_ = lv_gif_create(panel);
    lv_gif_set_src(weather_scene_gif_, &qd_weather_cloudy_scene);
    lv_obj_set_size(weather_scene_gif_, 142, 84);
    lv_obj_align(weather_scene_gif_, LV_ALIGN_TOP_LEFT, 12, 22);
    lv_obj_set_style_bg_opa(weather_scene_gif_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_scene_gif_, 0, 0);
    lv_obj_add_flag(weather_scene_gif_, LV_OBJ_FLAG_HIDDEN);
    add_gesture_bubble(weather_scene_gif_);

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

    lv_anim_t storm_anim;
    lv_anim_init(&storm_anim);
    lv_anim_set_var(&storm_anim, weather_storm_[0]);
    lv_anim_set_values(&storm_anim, LV_OPA_10, LV_OPA_30);
    lv_anim_set_time(&storm_anim, 420);
    lv_anim_set_playback_time(&storm_anim, 820);
    lv_anim_set_repeat_count(&storm_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&storm_anim, ObjOpaCb);
    lv_anim_start(&storm_anim);
    
    // Weather particle animation timer
    weather_particle_timer_ = lv_timer_create(WeatherParticleCb, 260, this);
    lv_timer_pause(weather_particle_timer_);

    weather_temp_label_ = label_en(panel, "-- C", &style_en);
    lv_obj_set_style_text_font(weather_temp_label_, &lv_font_montserrat_20, 0);
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(weather_temp_label_, COLOR_GREEN, 0);
        lv_obj_set_style_text_font(weather_temp_label_, &lv_font_montserrat_20, 0);
    }
    lv_obj_set_width(weather_temp_label_, 142);
    lv_label_set_long_mode(weather_temp_label_, LV_LABEL_LONG_CLIP);
    lv_obj_align(weather_temp_label_, LV_ALIGN_TOP_LEFT, 14, 112);

    weather_meta_label_ = label_en(panel, "Weather pending", &style_green);
    lv_obj_set_width(weather_meta_label_, 142);
    lv_label_set_long_mode(weather_meta_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(weather_meta_label_, &font_puhui_16_4, 0);
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(weather_meta_label_, COLOR_GREEN, 0);
    }
    lv_obj_align(weather_meta_label_, LV_ALIGN_TOP_LEFT, 14, 134);

    ApplyWeatherVisual(-1);
}

void DesktopUI::CreateQuotePanel(lv_obj_t* parent) {
    daily_card_panel_ = CreatePanel(parent, 438, is_tupi_warm_theme() ? 76 : 94,
                                    20, is_tupi_warm_theme() ? 232 : 214);
    if (is_cat_theme()) {
        lv_obj_set_style_bg_color(daily_card_panel_, COLOR_SURFACE, 0);
        lv_obj_set_style_border_color(daily_card_panel_, COLOR_LINE, 0);
        lv_obj_set_style_shadow_width(daily_card_panel_, 12, 0);
        lv_obj_set_style_shadow_color(daily_card_panel_, cat_card_shadow(), 0);
        lv_obj_set_style_shadow_opa(daily_card_panel_, LV_OPA_40, 0);
    } else if (is_tupi_warm_theme()) {
        lv_obj_set_style_bg_color(daily_card_panel_, COLOR_SURFACE, 0);
        lv_obj_set_style_border_color(daily_card_panel_, COLOR_LINE, 0);
        lv_obj_set_style_shadow_width(daily_card_panel_, 8, 0);
        lv_obj_set_style_shadow_color(daily_card_panel_, tupi_warm_shadow(), 0);
        lv_obj_set_style_shadow_opa(daily_card_panel_, LV_OPA_20, 0);

        lv_obj_t* note = label_en(daily_card_panel_, "tupi note", &style_muted);
        lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(note, COLOR_GREEN, 0);
        lv_obj_align(note, LV_ALIGN_TOP_LEFT, 22, 8);
        create_tupi_dot_mark(daily_card_panel_, 98, 10, 6, 3);
    }

    daily_card_date_label_ = label_en(daily_card_panel_, "--/--", &style_gold);
    lv_obj_set_width(daily_card_date_label_, 92);
    lv_obj_set_style_text_align(daily_card_date_label_,
                                is_tupi_warm_theme() ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(daily_card_date_label_,
                               is_tupi_warm_theme() ? &lv_font_montserrat_16 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(daily_card_date_label_,
                                is_tupi_warm_theme() ? COLOR_GREEN : COLOR_GOLD, 0);
    lv_obj_align(daily_card_date_label_, LV_ALIGN_TOP_LEFT,
                 is_tupi_warm_theme() ? 24 : 16, is_tupi_warm_theme() ? 33 : 15);

    daily_card_title_label_ = label_en(daily_card_panel_, "今日", &style_muted);
    lv_obj_set_width(daily_card_title_label_, is_tupi_warm_theme() ? 104 : 108);
    lv_label_set_long_mode(daily_card_title_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(daily_card_title_label_,
                                is_tupi_warm_theme() ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(daily_card_title_label_, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(daily_card_title_label_,
                               is_tupi_warm_theme() ? &qd_font_lxgw_16 : &qd_font_lxgw_20, 0);
    lv_obj_align(daily_card_title_label_, LV_ALIGN_TOP_LEFT,
                 is_tupi_warm_theme() ? 24 : 16,
                 is_tupi_warm_theme() ? 52 : (is_cat_theme() ? 45 : 47));

    lv_obj_t* divider = bar(daily_card_panel_, 2, 62, COLOR_LINE, LV_OPA_COVER);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT,
                 is_tupi_warm_theme() ? 124 :
                 ((is_cat_theme() || is_classic_theme()) ? 120 : 132),
                 is_tupi_warm_theme() ? 10 : 16);

    if (is_cat_theme()) {
        lv_obj_t* cat = lv_image_create(daily_card_panel_);
        lv_image_set_src(cat, &qd_cat_daily);
        lv_obj_align(cat, LV_ALIGN_TOP_LEFT, 128, 21);
        lv_obj_add_flag(cat, LV_OBJ_FLAG_EVENT_BUBBLE);
    } else if (is_classic_theme()) {
        lv_obj_t* bot = lv_image_create(daily_card_panel_);
        lv_image_set_src(bot, &qd_classic_daily);
        lv_obj_align(bot, LV_ALIGN_TOP_LEFT, 126, 18);
        lv_obj_add_flag(bot, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    quote_label_ = label_en(daily_card_panel_, "正在同步今日卡片", &style_en);
    lv_obj_set_width(quote_label_,
                     is_tupi_warm_theme() ? 278 :
                     ((is_cat_theme() || is_classic_theme()) ? 220 : 266));
    lv_label_set_long_mode(quote_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(quote_label_, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(quote_label_, &qd_font_lxgw_20, 0);
    lv_obj_set_style_text_line_space(quote_label_,
                                     is_tupi_warm_theme() ? 2 :
                                     ((is_cat_theme() || is_classic_theme()) ? 1 : 0), 0);
    lv_obj_align(quote_label_, LV_ALIGN_TOP_LEFT,
                 is_tupi_warm_theme() ? 146 :
                 ((is_cat_theme() || is_classic_theme()) ? 200 : 152),
                 is_tupi_warm_theme() ? 16 : 10);

    network_status_label_ = label_en(daily_card_panel_, "XiaoZhi AI", &style_muted);
    lv_obj_set_width(network_status_label_,
                     is_tupi_warm_theme() ? 278 :
                     ((is_cat_theme() || is_classic_theme()) ? 218 : 266));
    lv_label_set_long_mode(network_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(network_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(network_status_label_, LV_ALIGN_BOTTOM_LEFT,
                 is_tupi_warm_theme() ? 148 :
                 ((is_cat_theme() || is_classic_theme()) ? 202 : 152),
                 is_tupi_warm_theme() ? -9 : -7);
    if (is_tupi_warm_theme()) {
        lv_obj_add_flag(network_status_label_, LV_OBJ_FLAG_HIDDEN);
    }
    
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
    lv_obj_set_style_bg_color(apps_page_,
                              is_tupi_warm_theme() ? COLOR_BG :
                              themed_color(LV_COLOR_MAKE(0x0e, 0x08, 0x05), COLOR_BG), 0);

    lv_obj_t* logo = nullptr;
    lv_obj_t* owner = nullptr;
    create_brand_mark(apps_page_, 18, 4, &logo, &owner);
    RegisterBrandLabels(logo, owner);

    CreateStatusBar(apps_page_);

    lv_obj_t* title = label_en(apps_page_, "Apps", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    }
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(apps_page_, "App Center", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 86, 53);

    lv_obj_t* back = CreateButton(apps_page_, "Back", navigate_back_cb);
    lv_obj_set_style_bg_color(back,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x24, 0x16, 0x0f), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(back,
                                  is_tupi_warm_theme() ? COLOR_LINE :
                                  themed_color(LV_COLOR_MAKE(0x78, 0x48, 0x26), COLOR_LINE), 0);
    lv_obj_set_style_radius(back, 16, 0);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    apps_primary_group_ = lv_obj_create(apps_page_);
    lv_obj_remove_style_all(apps_primary_group_);
    lv_obj_set_size(apps_primary_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(apps_primary_group_, LV_OBJ_FLAG_SCROLLABLE);

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
        {"AI", "XiaoZhi", "Online", is_tupi_warm_theme() ? COLOR_GREEN : COLOR_PURPLE, xiaozhi_card_cb},
        {"FC", "NES", "SD ROMs", COLOR_GREEN, fc_card_cb},
        {"CAL", "Calendar", "Today", is_tupi_warm_theme() ? COLOR_GOLD : COLOR_PURPLE, calendar_card_cb},
        {"FOC", "Focus", "25 min", COLOR_GOLD, focus_card_cb},
        {"NET", "Network", "WiFi Hub", is_tupi_warm_theme() ? COLOR_GREEN : COLOR_BLUE, network_card_cb},
        {"SET", "Settings", "System", COLOR_GOLD, settings_card_cb},
        {"MUS", "Music", "Ask song", is_tupi_warm_theme() ? COLOR_GREEN : COLOR_PURPLE, music_card_cb},
        {"POD", "Podcast", "Episodes", COLOR_GOLD, podcast_card_cb},
    };

    for (uint8_t i = 0; i < sizeof(apps) / sizeof(apps[0]); ++i) {
        lv_obj_t* tile = CreateAppTile(apps_primary_group_, i, apps[i].cn, apps[i].en, apps[i].status, apps[i].color);
        if (apps[i].cb) {
            lv_obj_add_event_cb(tile, apps[i].cb, LV_EVENT_CLICKED, NULL);
        }
        if (i == 7) {
            lv_obj_add_event_cb(tile, diagnostics_open_cb, LV_EVENT_LONG_PRESSED, NULL);
        }
    }

    apps_more_button_ = CreateButton(apps_page_, "More", nullptr);
    lv_obj_set_size(apps_more_button_, 92, 26);
    lv_obj_set_style_radius(apps_more_button_, 10, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(apps_more_button_, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(apps_more_button_, LV_ALIGN_TOP_LEFT, 248, 45);

    apps_more_group_ = lv_obj_create(apps_page_);
    lv_obj_remove_style_all(apps_more_group_);
    lv_obj_set_size(apps_more_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(apps_more_group_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(apps_more_group_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* shake_card = CreatePanel(apps_more_group_, 432, 142, 24, 90);
    lv_obj_set_style_bg_color(shake_card, COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(shake_card, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(shake_card, 2, 0);
    lv_obj_t* shake_title = label_en(shake_card, "Shake Lab", &style_en);
    lv_obj_set_style_text_font(shake_title, &lv_font_montserrat_20, 0);
    lv_obj_align(shake_title, LV_ALIGN_TOP_LEFT, 22, 20);
    lv_obj_t* shake_cn = label_en(shake_card, "摇一摇实验室", &style_gold);
    lv_obj_set_style_text_font(shake_cn, qd_cn_font_20(), 0);
    lv_obj_align(shake_cn, LV_ALIGN_TOP_LEFT, 22, 50);
    lv_obj_t* shake_detail = label_en(shake_card, "Ask Ball  ·  Dice", &style_muted);
    lv_obj_align(shake_detail, LV_ALIGN_TOP_LEFT, 22, 88);
    lv_obj_t* shake_mark = circle(shake_card, 54, COLOR_GREEN, LV_OPA_50);
    lv_obj_align(shake_mark, LV_ALIGN_RIGHT_MID, -30, 0);
    lv_obj_t* shake_dot = circle(shake_card, 18, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align_to(shake_dot, shake_mark, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* hint = label_en(apps_page_, "Swipe right: Home", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    RefreshAppTileStatuses();
}

lv_obj_t* DesktopUI::CreateAppTile(lv_obj_t* parent, uint8_t index, const char* cn, const char* en, const char* status, lv_color_t color) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_add_style(box, &style_panel, 0);
    lv_obj_set_size(box, 204, 42);
    const int16_t x = 24 + (index % 2) * 218;
    const int16_t y = 76 + (index / 2) * 45;
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(box, apps_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(box,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x18, 0x0f, 0x0a), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(box,
                                  is_tupi_warm_theme() ? COLOR_LINE :
                                  themed_color(LV_COLOR_MAKE(0x68, 0x3d, 0x22), COLOR_LINE), 0);
    lv_obj_set_style_radius(box, is_tupi_warm_theme() ? 8 : 6, 0);
    if (is_cat_theme()) {
        lv_obj_set_style_shadow_width(box, 10, 0);
        lv_obj_set_style_shadow_color(box, cat_card_shadow(), 0);
        lv_obj_set_style_shadow_opa(box, LV_OPA_30, 0);
    } else if (is_tupi_warm_theme()) {
        lv_obj_set_style_shadow_width(box, 6, 0);
        lv_obj_set_style_shadow_color(box, tupi_warm_shadow(), 0);
        lv_obj_set_style_shadow_opa(box, LV_OPA_20, 0);
    }
    add_gesture_bubble(box);

    lv_obj_t* icon_box = lv_obj_create(box);
    lv_obj_remove_style_all(icon_box);
    lv_obj_set_size(icon_box, 36, 30);
    lv_obj_set_style_radius(icon_box, 6, 0);
    lv_obj_set_style_bg_color(icon_box,
                              is_tupi_warm_theme() ? COLOR_SURFACE_2 :
                              themed_color(LV_COLOR_MAKE(0x1b, 0x11, 0x0b), COLOR_SURFACE_2), 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(icon_box, is_tupi_warm_theme() ? COLOR_LINE : COLOR_GOLD, 0);
    lv_obj_set_style_border_width(icon_box, 1, 0);
    lv_obj_align(icon_box, LV_ALIGN_TOP_LEFT, 10, 6);
    add_gesture_bubble(icon_box);

    lv_obj_t* cn_label = label_en(icon_box, cn, &style_gold);
    lv_obj_set_style_text_color(cn_label, is_tupi_warm_theme() ? color : COLOR_GOLD, 0);
    lv_obj_set_style_text_font(cn_label, &lv_font_montserrat_12, 0);
    lv_obj_center(cn_label);

    lv_obj_t* en_label = label_en(box, en, &style_gold);
    lv_obj_set_style_text_color(en_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(en_label, &lv_font_montserrat_16, 0);
    lv_obj_align(en_label, LV_ALIGN_TOP_LEFT, 58, 5);

    lv_obj_t* dot = circle(box, 5, color, LV_OPA_COVER);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 58, 29);
    lv_obj_t* status_label = label_en(box, status, &style_muted);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_width(status_label, 82);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 67, 25);
    if (index < sizeof(app_status_labels_) / sizeof(app_status_labels_[0])) {
        app_status_labels_[index] = status_label;
        app_status_dots_[index] = dot;
    }
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
            lv_obj_set_style_bg_color(frame,
                                      themed_color(LV_COLOR_MAKE(0x8b, 0x6c, 0x45),
                                                   LV_COLOR_MAKE(0xff, 0xe8, 0xf0)), 0);
            lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(frame,
                                          themed_color(LV_COLOR_MAKE(0xe8, 0xc9, 0x8e), COLOR_LINE), 0);
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
            lv_obj_t* face = circle(box, 28,
                                    themed_color(LV_COLOR_MAKE(0x20, 0x14, 0x0d), COLOR_CREAM),
                                    LV_OPA_COVER);
            lv_obj_set_style_border_color(face, is_tupi_warm_theme() ? COLOR_GREEN : COLOR_GOLD, 0);
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
            lv_obj_t* cart = bar(box, 42, 18,
                                 themed_color(LV_COLOR_MAKE(0x8d, 0xa7, 0xb4), COLOR_BLUE),
                                 LV_OPA_COVER);
            lv_obj_set_style_radius(cart, 2, 0);
            lv_obj_set_style_border_color(cart,
                                          themed_color(LV_COLOR_MAKE(0x5c, 0x3a, 0x24), COLOR_LINE), 0);
            lv_obj_set_style_border_width(cart, 2, 0);
            lv_obj_align(cart, LV_ALIGN_TOP_RIGHT, -28, 15);
            lv_obj_t* led = circle(box, 6,
                                   themed_color(LV_COLOR_MAKE(0xc5, 0x6e, 0x4c), COLOR_PURPLE),
                                   LV_OPA_COVER);
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
            lv_obj_t* ring = circle(box, 36,
                                    themed_color(LV_COLOR_MAKE(0x1c, 0x11, 0x0b), COLOR_CREAM),
                                    LV_OPA_COVER);
            lv_obj_set_style_border_color(ring,
                                          themed_color(LV_COLOR_MAKE(0xe0, 0x8d, 0x4d), COLOR_PURPLE), 0);
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

    photo_bg_a_ = lv_image_create(frame);
    photo_bg_b_ = lv_image_create(frame);
    photo_image_a_ = lv_image_create(frame);
    photo_image_b_ = lv_image_create(frame);
    lv_obj_set_style_opa(photo_bg_a_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(photo_bg_b_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(photo_image_a_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(photo_image_b_, LV_OPA_TRANSP, 0);
    lv_obj_center(photo_bg_a_);
    lv_obj_center(photo_bg_b_);
    lv_obj_center(photo_image_a_);
    lv_obj_center(photo_image_b_);
    add_gesture_bubble(photo_bg_a_);
    add_gesture_bubble(photo_bg_b_);
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
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(fc_title_label_, COLOR_TEXT, 0);
    }
    lv_obj_set_style_text_font(fc_title_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(fc_title_label_, LV_ALIGN_TOP_LEFT, 24, 14);

    fc_detail_label_ = label_en(fc_list_group_, "Scanning SD card", &style_muted);
    lv_obj_set_style_text_font(fc_detail_label_, &font_puhui_16_4, 0);
    lv_obj_set_width(fc_detail_label_, 300);
    lv_label_set_long_mode(fc_detail_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(fc_detail_label_, LV_ALIGN_TOP_LEFT, 132, 19);

    lv_obj_t* list_panel = CreatePanel(fc_list_group_, 432, 204, 24, 52);
    lv_obj_set_style_bg_color(list_panel,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x05, 0x07, 0x09), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(list_panel,
                                  is_tupi_warm_theme() ? COLOR_LINE :
                                  themed_color(LV_COLOR_MAKE(0x26, 0x31, 0x3c), COLOR_LINE), 0);
    lv_obj_set_style_border_width(list_panel, 1, 0);
    lv_obj_set_style_radius(list_panel, 6, 0);

    fc_list_label_ = label_en(list_panel, "No .nes\n/sdcard/nes", &style_en);
    lv_obj_set_style_text_font(fc_list_label_, &font_puhui_16_4, 0);
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(fc_list_label_, COLOR_TEXT, 0);
    }
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
    lv_obj_set_style_bg_color(screen,
                              is_tupi_warm_theme() ? COLOR_BG :
                              themed_color(LV_COLOR_MAKE(0x02, 0x03, 0x05), LV_COLOR_MAKE(0xff, 0xfb, 0xfc)), 0);
    lv_obj_set_style_border_color(screen,
                                  is_tupi_warm_theme() ? COLOR_LINE :
                                  themed_color(LV_COLOR_MAKE(0x26, 0x31, 0x3c), COLOR_LINE), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_radius(screen, 0, 0);

    fc_screen_image_ = lv_image_create(screen);
    lv_obj_center(fc_screen_image_);
    add_gesture_bubble(fc_screen_image_);

    lv_obj_t* controls = CreatePanel(fc_game_group_, 480, 80, 0, 240);
    lv_obj_set_style_bg_color(controls,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x05, 0x07, 0x09), COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_radius(controls, 0, 0);

    auto make_key = [&](const char* text, int x, int y, uint8_t mask, int w = 64, int h = 44) {
        lv_obj_t* key = lv_obj_create(fc_game_group_);
        lv_obj_add_style(key, &style_panel, 0);
        lv_obj_set_size(key, w, h);
        lv_obj_align(key, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_clear_flag(key, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(key,
                                  themed_color(LV_COLOR_MAKE(0x12, 0x1a, 0x20), COLOR_SURFACE_2), 0);
        lv_obj_set_style_bg_opa(key, LV_OPA_80, 0);
        lv_obj_set_style_border_color(key,
                                      themed_color(LV_COLOR_MAKE(0x47, 0xb3, 0xff), COLOR_BLUE), 0);
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
        lv_obj_set_style_bg_color(key,
                                  themed_color(LV_COLOR_MAKE(0x1c, 0x18, 0x14), COLOR_SURFACE_2), 0);
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

    lv_obj_set_style_bg_color(calendar_page_,
                              themed_color(LV_COLOR_MAKE(0x2a, 0x16, 0x0c), COLOR_BG), 0);

    lv_obj_t* glow = circle(calendar_page_, 146,
                            themed_color(LV_COLOR_MAKE(0xb0, 0x6c, 0x36), COLOR_PURPLE),
                            is_cat_theme() ? LV_OPA_20 : LV_OPA_30);
    lv_obj_align(glow, LV_ALIGN_BOTTOM_LEFT, -48, 34);

    lv_obj_t* today_card = CreatePanel(calendar_page_, 146, 252, 10, 18);
    lv_obj_set_style_bg_color(today_card,
                              themed_color(LV_COLOR_MAKE(0xff, 0xf3, 0xdb), COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(today_card, 0, 0);
    lv_obj_set_style_radius(today_card, 8, 0);

    lv_obj_t* card_sun = circle(today_card, 112,
                                themed_color(LV_COLOR_MAKE(0xff, 0xc8, 0x78), COLOR_PURPLE),
                                LV_OPA_30);
    lv_obj_align(card_sun, LV_ALIGN_TOP_RIGHT, 38, -32);

    lv_obj_t* card_shadow = circle(today_card, 96,
                                   themed_color(LV_COLOR_MAKE(0xed, 0xa0, 0x54), COLOR_BLUE),
                                   LV_OPA_20);
    lv_obj_align(card_shadow, LV_ALIGN_BOTTOM_LEFT, -58, 26);

    lv_obj_t* card_today_cn = label_en(today_card, "\xE4\xBB\x8A""\xE6\x97\xA5", &style_gold);
    lv_obj_set_style_text_font(card_today_cn,
                               is_cat_theme() ? qd_cn_font_20() : qd_cn_font_16(), 0);
    lv_obj_align(card_today_cn, LV_ALIGN_TOP_LEFT, 20, is_cat_theme() ? 18 : 22);

    lv_obj_t* card_today_en = label_en(today_card, "TODAY", &style_muted);
    lv_obj_set_style_text_color(card_today_en,
                                themed_color(LV_COLOR_MAKE(0x55, 0x4c, 0x42), COLOR_MUTED), 0);
    lv_obj_align(card_today_en, LV_ALIGN_TOP_LEFT, 20, 54);

    calendar_card_day_label_ = label_en(today_card, "--", &style_en);
    lv_obj_set_style_text_color(calendar_card_day_label_,
                                themed_color(LV_COLOR_MAKE(0x20, 0x16, 0x10), COLOR_PURPLE), 0);
    lv_obj_set_style_text_font(calendar_card_day_label_, &lv_font_montserrat_48, 0);
    lv_obj_align(calendar_card_day_label_, LV_ALIGN_TOP_MID, 0, 92);

    calendar_card_weekday_label_ = label_en(today_card, "--", &style_en);
    lv_obj_set_style_text_color(calendar_card_weekday_label_,
                                themed_color(LV_COLOR_MAKE(0x2e, 0x21, 0x18), COLOR_TEXT), 0);
    lv_obj_set_style_text_font(calendar_card_weekday_label_,
                               is_cat_theme() ? qd_cn_font_20() : qd_cn_font_16(), 0);
    lv_obj_align(calendar_card_weekday_label_, LV_ALIGN_TOP_LEFT, 22, is_cat_theme() ? 160 : 164);

    calendar_card_date_label_ = label_en(today_card, "---- / --", &style_gold);
    lv_obj_set_style_text_font(calendar_card_date_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(calendar_card_date_label_, LV_ALIGN_TOP_LEFT, 22, 198);

    lv_obj_t* panel = CreatePanel(calendar_page_, 304, 252, 166, 18);
    lv_obj_set_style_bg_color(panel,
                              themed_color(LV_COLOR_MAKE(0x18, 0x0f, 0x0a), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(panel,
                                  themed_color(LV_COLOR_MAKE(0x63, 0x43, 0x2b), COLOR_LINE), 0);
    lv_obj_set_style_radius(panel, 8, 0);

    calendar_title_label_ = label_en(panel, "Month ----", &style_en);
    lv_obj_set_style_text_color(calendar_title_label_,
                                themed_color(LV_COLOR_MAKE(0xff, 0xf5, 0xe4), COLOR_TEXT), 0);
    lv_obj_set_style_text_font(calendar_title_label_, &lv_font_montserrat_20, 0);
    lv_obj_align(calendar_title_label_, LV_ALIGN_TOP_LEFT, 18, 18);

    lv_obj_t* top_today = CreateButton(panel, "Today", calendar_today_cb);
    lv_obj_set_size(top_today, 76, 28);
    lv_obj_set_style_bg_color(top_today,
                              themed_color(LV_COLOR_MAKE(0xff, 0xc1, 0x70), COLOR_CREAM), 0);
    lv_obj_set_style_border_width(top_today, 0, 0);
    lv_obj_align(top_today, LV_ALIGN_TOP_RIGHT, -18, 18);

    calendar_today_label_ = label_en(panel, "Minimal monthly view", &style_muted);
    lv_obj_set_width(calendar_today_label_, 180);
    lv_label_set_long_mode(calendar_today_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(calendar_today_label_,
                                themed_color(LV_COLOR_MAKE(0x9a, 0x76, 0x5e), COLOR_MUTED), 0);
    lv_obj_set_style_text_font(calendar_today_label_, &lv_font_montserrat_14, 0);
    lv_obj_align(calendar_today_label_, LV_ALIGN_TOP_LEFT, 18, 50);

    lv_obj_t* divider = bar(panel, 268, 1,
                            themed_color(LV_COLOR_MAKE(0x6b, 0x48, 0x2e), COLOR_LINE),
                            LV_OPA_COVER);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 18, 76);

    static constexpr const char* kWeekdays[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
    for (int col = 0; col < 7; ++col) {
        lv_obj_t* label = label_en(panel, kWeekdays[col], &style_muted);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        const lv_color_t weekday_color = col >= 5 ? COLOR_GOLD : themed_color(lv_color_make(0xa8, 0x86, 0x6e), COLOR_MUTED);
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
        lv_obj_set_style_bg_color(cell,
                                  themed_color(LV_COLOR_MAKE(0x24, 0x18, 0x10), COLOR_SURFACE_2), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_50, 0);
        lv_obj_set_style_border_color(cell,
                                      themed_color(LV_COLOR_MAKE(0x5d, 0x40, 0x2b), COLOR_LINE), 0);
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
    lv_obj_set_style_bg_color(prev,
                              themed_color(LV_COLOR_MAKE(0x20, 0x14, 0x0d), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(prev,
                                  themed_color(LV_COLOR_MAKE(0x5b, 0x3c, 0x27), COLOR_LINE), 0);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 18, 278);

    lv_obj_t* today = CreateButton(calendar_page_, "Today", calendar_today_cb);
    lv_obj_set_size(today, 96, 34);
    lv_obj_set_ext_click_area(today, 12);
    lv_obj_set_style_bg_color(today,
                              themed_color(LV_COLOR_MAKE(0xff, 0xc1, 0x70), COLOR_CREAM), 0);
    lv_obj_set_style_border_width(today, 0, 0);
    lv_obj_align(today, LV_ALIGN_TOP_MID, 0, 278);

    lv_obj_t* next = CreateButton(calendar_page_, "Next", calendar_next_cb);
    lv_obj_set_size(next, 96, 34);
    lv_obj_set_ext_click_area(next, 12);
    lv_obj_set_style_bg_color(next,
                              themed_color(LV_COLOR_MAKE(0x20, 0x14, 0x0d), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(next,
                                  themed_color(LV_COLOR_MAKE(0x5b, 0x3c, 0x27), COLOR_LINE), 0);
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
    lv_obj_t* logo = nullptr;
    lv_obj_t* owner = nullptr;
    create_brand_mark(radio_page_, 18, 4, &logo, &owner);
    RegisterBrandLabels(logo, owner);

    CreateStatusBar(radio_page_);

    // 标题
    lv_obj_t* title = label_en(radio_page_, "Radio", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    // 返回按钮
    lv_obj_t* back = CreateButton(radio_page_, "Back", nullptr);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    // 当前电台信息
    radio_station_label_ = label_en(radio_page_, "CNR China Voice", &style_gold);
    lv_obj_set_style_text_font(radio_station_label_, &font_puhui_16_4, 0);
    lv_obj_align(radio_station_label_, LV_ALIGN_TOP_LEFT, 24, 88);

    radio_state_label_ = label_en(radio_page_, "Ready", &style_green);
    lv_obj_set_style_text_font(radio_state_label_, &font_puhui_16_4, 0);
    lv_obj_align(radio_state_label_, LV_ALIGN_TOP_LEFT, 24, 118);

    radio_meta_label_ = label_en(radio_page_, "MP3 64 kbps", &style_muted);
    lv_obj_set_style_text_font(radio_meta_label_, &font_puhui_16_4, 0);
    lv_obj_align(radio_meta_label_, LV_ALIGN_TOP_LEFT, 24, 144);

    // 播放控制按钮
    lv_obj_t* prev = CreateButton(radio_page_, "Prev", nullptr);
    lv_obj_align(prev, LV_ALIGN_TOP_LEFT, 24, 180);

    lv_obj_t* play = CreateButton(radio_page_, "Play", nullptr);
    lv_obj_align(play, LV_ALIGN_TOP_LEFT, 124, 180);

    lv_obj_t* stop = CreateButton(radio_page_, "Stop", nullptr);
    lv_obj_align(stop, LV_ALIGN_TOP_LEFT, 224, 180);

    lv_obj_t* next = CreateButton(radio_page_, "Next", nullptr);
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

void DesktopUI::CreateMusicPage(lv_obj_t* root) {
    music_page_ = lv_obj_create(root);
    lv_obj_add_style(music_page_, &style_screen, 0);
    lv_obj_set_size(music_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(music_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(music_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(music_page_, music_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(music_page_);

    lv_obj_set_style_bg_color(music_page_,
                              is_tupi_warm_theme() ? COLOR_BG :
                              themed_color(LV_COLOR_MAKE(0x09, 0x0c, 0x13), COLOR_BG), 0);

    lv_obj_t* back = CreateButton(music_page_, "Back", navigate_back_cb);
    lv_obj_set_size(back, 52, 22);
    lv_obj_set_style_text_font(lv_obj_get_child(back, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -14, 8);

    lv_obj_t* pause = CreateButton(music_page_, "Pause", music_pause_cb);
    lv_obj_set_size(pause, 52, 22);
    lv_obj_set_style_radius(pause, 8, 0);
    lv_obj_set_style_border_color(pause, COLOR_GOLD, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(pause, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(pause, LV_ALIGN_TOP_RIGHT, -14, 36);

    lv_obj_t* play = CreateButton(music_page_, "Play", music_play_cb);
    lv_obj_set_size(play, 52, 22);
    lv_obj_set_style_radius(play, 8, 0);
    lv_obj_set_style_border_color(play, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(play, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(play, LV_ALIGN_TOP_RIGHT, -14, 64);

    lv_obj_t* next = CreateButton(music_page_, "Next", music_next_cb);
    lv_obj_set_size(next, 52, 22);
    lv_obj_set_style_radius(next, 8, 0);
    lv_obj_set_style_border_color(next, COLOR_PURPLE, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(next, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(next, LV_ALIGN_TOP_RIGHT, -14, 92);

    lv_obj_t* cover = CreatePanel(music_page_, 128, 112, 18, 18);
    lv_obj_set_style_bg_color(cover,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x11, 0x17, 0x22), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(cover, COLOR_GOLD, 0);
    lv_obj_set_style_border_opa(cover, LV_OPA_60, 0);
    lv_obj_set_style_radius(cover, 8, 0);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cover, music_face_cb, LV_EVENT_CLICKED, NULL);
    add_gesture_bubble(cover);

    music_cover_disc_ = lv_gif_create(cover);
    lv_gif_set_src(music_cover_disc_, &qd_music_vinyl);
    lv_obj_set_size(music_cover_disc_, 84, 84);
    lv_obj_set_style_bg_opa(music_cover_disc_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(music_cover_disc_, 0, 0);
    lv_obj_align(music_cover_disc_, LV_ALIGN_TOP_MID, 0, 2);

    static constexpr int16_t kBarX[] = {20, 40, 60, 80};
    for (size_t i = 0; i < sizeof(music_cover_bars_) / sizeof(music_cover_bars_[0]); ++i) {
        lv_obj_t* bar = lv_obj_create(cover);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 9, 18 + static_cast<int16_t>(i) * 4);
        lv_obj_set_style_radius(bar, 5, 0);
        lv_obj_set_style_bg_color(bar, i % 2 == 0 ? COLOR_GOLD : COLOR_GREEN, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_70, 0);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, kBarX[i], -10);
        music_cover_bars_[i] = bar;
    }

    lv_obj_t* panel = CreatePanel(music_page_, 220, 112, 158, 18);
    lv_obj_set_style_bg_color(panel,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x12, 0x16, 0x22), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(panel,
                                  is_tupi_warm_theme() ? COLOR_LINE :
                                  themed_color(LV_COLOR_MAKE(0x42, 0x55, 0x78), COLOR_LINE), 0);
    lv_obj_set_style_radius(panel, 8, 0);

    music_title_label_ = label_en(panel, music_title_.c_str(), &style_en);
    lv_obj_set_style_text_font(music_title_label_, qd_cn_font_20(), 0);
    lv_obj_set_width(music_title_label_, 190);
    lv_label_set_long_mode(music_title_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(music_title_label_, LV_ALIGN_TOP_LEFT, 14, 14);

    music_artist_label_ = label_en(panel, music_artist_.c_str(), &style_muted);
    lv_obj_set_style_text_font(music_artist_label_, qd_cn_font_16(), 0);
    lv_obj_set_width(music_artist_label_, 190);
    lv_label_set_long_mode(music_artist_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(music_artist_label_, LV_ALIGN_TOP_LEFT, 14, 46);

    music_line_label_ = label_en(panel, "Ready", &style_gold);
    lv_obj_set_style_text_font(music_line_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_width(music_line_label_, 190);
    lv_obj_set_height(music_line_label_, 22);
    lv_obj_set_style_text_letter_space(music_line_label_, 0, 0);
    lv_label_set_long_mode(music_line_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(music_line_label_, LV_ALIGN_BOTTOM_LEFT, 14, -12);

    lv_obj_t* lyric_panel = CreatePanel(music_page_, 446, 74, 18, 144);
    lv_obj_set_style_bg_color(lyric_panel,
                              is_tupi_warm_theme() ? COLOR_SURFACE_2 :
                              themed_color(LV_COLOR_MAKE(0x0f, 0x17, 0x24), COLOR_SURFACE_2), 0);
    lv_obj_set_style_border_color(lyric_panel, COLOR_GOLD, 0);
    lv_obj_set_style_border_opa(lyric_panel, LV_OPA_50, 0);
    lv_obj_set_style_radius(lyric_panel, 8, 0);

    lv_obj_t* lyric_accent = lv_obj_create(lyric_panel);
    lv_obj_remove_style_all(lyric_accent);
    lv_obj_set_size(lyric_accent, 3, 50);
    lv_obj_set_style_radius(lyric_accent, 2, 0);
    lv_obj_set_style_bg_color(lyric_accent, COLOR_GOLD, 0);
    lv_obj_set_style_bg_opa(lyric_accent, LV_OPA_80, 0);
    lv_obj_align(lyric_accent, LV_ALIGN_LEFT_MID, 14, 0);

    music_side_lyric_label_ = label_en(lyric_panel, music_line_.c_str(), &style_en);
    lv_obj_set_style_text_font(music_side_lyric_label_, qd_cn_font_20(), 0);
    lv_obj_set_width(music_side_lyric_label_, 390);
    lv_obj_set_height(music_side_lyric_label_, 48);
    lv_obj_set_style_text_align(music_side_lyric_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(music_side_lyric_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(music_side_lyric_label_, LV_ALIGN_RIGHT_MID, -18, 0);

    lv_obj_t* recent_title = label_en(music_page_, "Recent", &style_muted);
    lv_obj_set_style_text_font(recent_title, &lv_font_montserrat_12, 0);
    lv_obj_align(recent_title, LV_ALIGN_TOP_LEFT, 22, 226);
    music_recent_clear_button_ = CreateButton(music_page_, "Clear", music_recent_clear_cb);
    lv_obj_set_size(music_recent_clear_button_, 58, 22);
    lv_obj_set_style_radius(music_recent_clear_button_, 8, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(music_recent_clear_button_, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(music_recent_clear_button_, LV_ALIGN_TOP_LEFT, 252, 224);
    for (size_t i = 0; i < kMusicRecentCount; ++i) {
        lv_obj_t* row = lv_obj_create(music_page_);
        lv_obj_add_style(row, &style_panel, 0);
        lv_obj_set_size(row, 290, 19);
        lv_obj_set_style_radius(row, 5, 0);
        lv_obj_set_style_bg_color(row,
                                  is_tupi_warm_theme() ? COLOR_SURFACE :
                                  themed_color(LV_COLOR_MAKE(0x12, 0x16, 0x22), COLOR_SURFACE), 0);
        lv_obj_set_style_border_color(row, i == 0 ? COLOR_GOLD : COLOR_LINE, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, music_recent_cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(i));
        lv_obj_add_event_cb(row, music_recent_remove_cb, LV_EVENT_LONG_PRESSED, reinterpret_cast<void*>(i));
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 18, 248 + static_cast<int16_t>(i) * 22);
        add_gesture_bubble(row);
        music_recent_buttons_[i] = row;

        lv_obj_t* label = label_en(row, "--", &style_muted);
        lv_obj_set_style_text_font(label, qd_cn_font_16(), 0);
        lv_obj_set_width(label, 270);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        music_recent_labels_[i] = label;
    }
    RefreshMusicRecent();

    lv_obj_t* talk = CreateButton(music_page_, "Ask", music_talk_cb);
    lv_obj_set_size(talk, 134, 24);
    lv_obj_set_style_bg_color(talk, COLOR_GOLD, 0);
    lv_obj_set_style_border_width(talk, 0, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(talk, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(talk, LV_ALIGN_TOP_LEFT, 330, 226);

    lv_obj_t* again = CreateButton(music_page_, "Again", music_again_cb);
    lv_obj_set_size(again, 134, 24);
    lv_obj_set_style_border_color(again, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(again, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(again, LV_ALIGN_TOP_LEFT, 330, 258);

    lv_obj_t* stop = CreateButton(music_page_, "Stop", music_stop_cb);
    lv_obj_set_size(stop, 134, 24);
    lv_obj_set_style_border_color(stop, lv_color_make(0xff, 0x88, 0x68), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(stop, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(stop, LV_ALIGN_TOP_LEFT, 330, 290);

    music_cover_timer_ = lv_timer_create(MusicCoverTimerCb, 180, this);
}

void DesktopUI::CreateMediaPage(lv_obj_t* root) {
    media_page_ = lv_obj_create(root);
    lv_obj_add_style(media_page_, &style_screen, 0);
    lv_obj_set_size(media_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(media_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(media_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(media_page_, media_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(media_page_);

    lv_obj_t* logo = nullptr;
    lv_obj_t* owner = nullptr;
    create_brand_mark(media_page_, 18, 4, &logo, &owner);
    RegisterBrandLabels(logo, owner);
    CreateStatusBar(media_page_);

    lv_obj_t* title = label_en(media_page_, "Media", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(media_page_, "Third Page", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 92, 53);

    lv_obj_t* back = CreateButton(media_page_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    lv_obj_t* card = lv_obj_create(media_page_);
    lv_obj_add_style(card, &style_panel, 0);
    lv_obj_set_size(card, 432, 160);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 24, 92);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, podcast_card_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card, media_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x18, 0x10, 0x0b), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(card, is_tupi_warm_theme() ? COLOR_GOLD :
                                  themed_color(LV_COLOR_MAKE(0x8a, 0x52, 0x2c), COLOR_LINE), 0);

    lv_obj_t* badge = lv_obj_create(card);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 94, 94);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_radius(badge, 47, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(badge, themed_color(LV_COLOR_MAKE(0x10, 0x0c, 0x08), COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(badge, 3, 0);
    lv_obj_set_style_border_color(badge, COLOR_GOLD, 0);
    lv_obj_set_style_clip_corner(badge, true, 0);

    lv_obj_t* avatar = lv_gif_create(badge);
    lv_gif_set_src(avatar, &qd_podcast_avatar);
    lv_obj_center(avatar);
    lv_obj_set_style_transform_pivot_x(avatar, 43, 0);
    lv_obj_set_style_transform_pivot_y(avatar, 43, 0);
    lv_obj_set_style_transform_width(avatar, 8, 0);
    lv_obj_set_style_transform_height(avatar, 8, 0);

    lv_anim_t avatar_spin;
    lv_anim_init(&avatar_spin);
    lv_anim_set_var(&avatar_spin, avatar);
    lv_anim_set_values(&avatar_spin, 0, 3600);
    lv_anim_set_time(&avatar_spin, 9000);
    lv_anim_set_repeat_count(&avatar_spin, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&avatar_spin, [](void* obj, int32_t value) {
        lv_obj_set_style_transform_rotation(static_cast<lv_obj_t*>(obj), value, 0);
    });
    lv_anim_start(&avatar_spin);

    lv_obj_t* name = label_en(card, "Nothing Impossible", &style_gold);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 128, 22);

    lv_obj_t* cn = label_en(card, "我的播客", &style_en);
    lv_obj_set_style_text_font(cn, qd_cn_font_20(), 0);
    lv_obj_align(cn, LV_ALIGN_TOP_LEFT, 128, 54);

    lv_obj_t* desc = label_en(card, "只要你想，没有不可能", &style_muted);
    lv_obj_set_style_text_font(desc, qd_cn_font_16(), 0);
    lv_obj_set_style_text_color(desc, COLOR_TEXT, 0);
    lv_obj_set_width(desc, 270);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_align(desc, LV_ALIGN_TOP_LEFT, 128, 88);

    lv_obj_t* hint = label_en(media_page_, "Tap card to open list  |  Swipe right: Apps", &style_muted);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void DesktopUI::CreatePodcastPage(lv_obj_t* root) {
    podcast_page_ = lv_obj_create(root);
    lv_obj_add_style(podcast_page_, &style_screen, 0);
    lv_obj_set_size(podcast_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(podcast_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(podcast_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(podcast_page_, podcast_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(podcast_page_);

    podcast_list_group_ = lv_obj_create(podcast_page_);
    lv_obj_remove_style_all(podcast_list_group_);
    lv_obj_set_size(podcast_list_group_, 456, 296);
    lv_obj_align(podcast_list_group_, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_clear_flag(podcast_list_group_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* list_title = label_en(podcast_list_group_, "Nothing Impossible", &style_gold);
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_20, 0);
    lv_obj_align(list_title, LV_ALIGN_TOP_LEFT, 0, 0);

    podcast_state_label_ = label_en(podcast_list_group_, "Ready", &style_green);
    lv_obj_set_style_text_font(podcast_state_label_, qd_cn_font_16(), 0);
    lv_obj_align(podcast_state_label_, LV_ALIGN_TOP_LEFT, 196, 4);

    podcast_meta_label_ = label_en(podcast_list_group_, "Select one episode", &style_muted);
    lv_obj_set_width(podcast_meta_label_, 220);
    lv_label_set_long_mode(podcast_meta_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(podcast_meta_label_, LV_ALIGN_TOP_RIGHT, 0, 7);

    podcast_list_label_ = label_en(podcast_list_group_, "No episodes loaded", &style_en);
    lv_obj_set_style_text_font(podcast_list_label_, qd_cn_font_16(), 0);
    lv_obj_set_style_text_line_space(podcast_list_label_, 5, 0);
    lv_obj_set_width(podcast_list_label_, 456);
    lv_obj_set_height(podcast_list_label_, 214);
    lv_label_set_long_mode(podcast_list_label_, LV_LABEL_LONG_WRAP);
    lv_obj_align(podcast_list_label_, LV_ALIGN_TOP_LEFT, 0, 34);

    lv_obj_t* back = CreateButton(podcast_list_group_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t* up = CreateButton(podcast_list_group_, "Up", podcast_up_cb);
    lv_obj_align(up, LV_ALIGN_BOTTOM_LEFT, 126, 0);
    lv_obj_t* open = CreateButton(podcast_list_group_, "Open", podcast_open_cb);
    lv_obj_align(open, LV_ALIGN_BOTTOM_LEFT, 252, 0);
    lv_obj_t* down = CreateButton(podcast_list_group_, "Down", podcast_down_cb);
    lv_obj_align(down, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    podcast_detail_group_ = lv_obj_create(podcast_page_);
    lv_obj_remove_style_all(podcast_detail_group_);
    lv_obj_set_size(podcast_detail_group_, 456, 296);
    lv_obj_align(podcast_detail_group_, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_clear_flag(podcast_detail_group_, LV_OBJ_FLAG_SCROLLABLE);

    podcast_cover_image_ = lv_image_create(podcast_detail_group_);
    lv_obj_set_size(podcast_cover_image_, 150, 150);
    lv_obj_align(podcast_cover_image_, LV_ALIGN_TOP_LEFT, 0, 8);
    lv_obj_set_style_radius(podcast_cover_image_, 8, 0);
    lv_obj_set_style_clip_corner(podcast_cover_image_, true, 0);
    lv_obj_set_style_bg_color(podcast_cover_image_, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(podcast_cover_image_, LV_OPA_COVER, 0);

    podcast_title_label_ = label_en(podcast_detail_group_, "Scanning SD card", &style_en);
    lv_obj_set_style_text_font(podcast_title_label_, qd_cn_font_20(), 0);
    lv_obj_set_width(podcast_title_label_, 290);
    lv_label_set_long_mode(podcast_title_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(podcast_title_label_, LV_ALIGN_TOP_LEFT, 166, 4);

    podcast_summary_label_ = label_en(podcast_detail_group_, "Select an episode from list.", &style_muted);
    lv_obj_set_style_text_font(podcast_summary_label_, qd_cn_font_16(), 0);
    lv_obj_set_width(podcast_summary_label_, 290);
    lv_obj_set_height(podcast_summary_label_, 156);
    lv_label_set_long_mode(podcast_summary_label_, LV_LABEL_LONG_WRAP);
    lv_obj_align(podcast_summary_label_, LV_ALIGN_TOP_LEFT, 166, 38);

    podcast_progress_slider_ = lv_slider_create(podcast_detail_group_);
    lv_obj_set_size(podcast_progress_slider_, 230, 12);
    lv_obj_align(podcast_progress_slider_, LV_ALIGN_TOP_LEFT, 166, 212);
    lv_slider_set_range(podcast_progress_slider_, 0, 100);
    lv_slider_set_value(podcast_progress_slider_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(podcast_progress_slider_, COLOR_SURFACE_2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(podcast_progress_slider_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(podcast_progress_slider_, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(podcast_progress_slider_, COLOR_GOLD, LV_PART_KNOB);
    lv_obj_add_event_cb(podcast_progress_slider_, podcast_seek_cb, LV_EVENT_ALL, NULL);

    podcast_progress_label_ = label_en(podcast_detail_group_, "0%", &style_muted);
    lv_obj_set_width(podcast_progress_label_, 48);
    lv_label_set_long_mode(podcast_progress_label_, LV_LABEL_LONG_CLIP);
    lv_obj_align(podcast_progress_label_, LV_ALIGN_TOP_LEFT, 405, 205);

    lv_obj_t* list = CreateButton(podcast_detail_group_, "List", podcast_list_cb);
    lv_obj_align(list, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t* prev = CreateButton(podcast_detail_group_, "Prev", podcast_prev_cb);
    lv_obj_align(prev, LV_ALIGN_BOTTOM_LEFT, 76, 0);
    lv_obj_t* play_btn = CreateButton(podcast_detail_group_, "Play", podcast_play_cb);
    lv_obj_align(play_btn, LV_ALIGN_BOTTOM_MID, -36, 0);
    lv_obj_t* stop = CreateButton(podcast_detail_group_, "Stop", podcast_stop_cb);
    lv_obj_align(stop, LV_ALIGN_BOTTOM_MID, 42, 0);
    lv_obj_t* next = CreateButton(podcast_detail_group_, "Next", podcast_next_cb);
    lv_obj_align(next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    ShowPodcastDetail(false);
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
    lv_obj_t* back = CreateButton(focus_page_, "Back", navigate_back_cb);
    lv_obj_set_size(back, 52, 24);
    lv_obj_set_style_text_font(lv_obj_get_child(back, 0), &lv_font_montserrat_12, 0);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -14, 42);

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
    focus_completed_count_ = LoadFocusStats(&focus_count_date_);
    ReconcileFocusDate(true);
    UpdateFocusUI();
}

void DesktopUI::CreateHourglassPage(lv_obj_t* root) {
    hourglass_page_ = lv_obj_create(root);
    lv_obj_remove_style_all(hourglass_page_);
    lv_obj_set_size(hourglass_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(hourglass_page_, LV_COLOR_MAKE(0xff, 0xf7, 0xe8), 0);
    lv_obj_set_style_bg_opa(hourglass_page_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(hourglass_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hourglass_page_, LV_OBJ_FLAG_HIDDEN);

    hourglass_portrait_ = lv_obj_create(hourglass_page_);
    lv_obj_remove_style_all(hourglass_portrait_);
    lv_obj_set_size(hourglass_portrait_, 320, 480);
    lv_obj_align(hourglass_portrait_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(hourglass_portrait_, LV_COLOR_MAKE(0xff, 0xf7, 0xe8), 0);
    lv_obj_set_style_bg_opa(hourglass_portrait_, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(hourglass_portrait_, 160, 0);
    lv_obj_set_style_transform_pivot_y(hourglass_portrait_, 240, 0);
    lv_obj_set_style_transform_rotation(hourglass_portrait_, 900, 0);
    lv_obj_clear_flag(hourglass_portrait_, LV_OBJ_FLAG_SCROLLABLE);

    const lv_color_t brown = LV_COLOR_MAKE(0x4b, 0x2d, 0x16);
    const lv_color_t sand_line = LV_COLOR_MAKE(0xf6, 0xb2, 0x3f);
    const lv_color_t sand_soft = LV_COLOR_MAKE(0xff, 0xd1, 0x67);

    auto rounded = [](lv_obj_t* parent, int x, int y, int w, int h, int radius,
                      lv_color_t bg, lv_opa_t opa, lv_color_t border, int border_w) {
        lv_obj_t* obj = lv_obj_create(parent);
        lv_obj_remove_style_all(obj);
        lv_obj_set_size(obj, w, h);
        lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_set_style_radius(obj, radius, 0);
        lv_obj_set_style_bg_color(obj, bg, 0);
        lv_obj_set_style_bg_opa(obj, opa, 0);
        lv_obj_set_style_border_color(obj, border, 0);
        lv_obj_set_style_border_width(obj, border_w, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    };

    lv_obj_t* body = lv_image_create(hourglass_portrait_);
    lv_image_set_src(body, &qd_hourglass_body);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 0);

    for (int i = 0; i < 16; ++i) {
        const int y = 143 + i * 3;
        const int width = std::max(12, 166 - i * 9);
        const int x = 160 - width / 2 + ((i % 3) - 1);
        const lv_color_t color = (i % 3 == 0) ? sand_soft : sand_line;
        hourglass_top_sand_strips_[i] = rounded(hourglass_portrait_, x, y, width, 4, 2,
                                                color, LV_OPA_80, color, 0);
    }
    for (int i = 0; i < 22; ++i) {
        const int width = std::min(184, 24 + i * 8);
        const int x = 160 - width / 2 + ((i % 2) ? 1 : -1);
        const int y = 323 - i * 3;
        const lv_color_t color = (i % 4 == 0) ? sand_soft : sand_line;
        hourglass_bottom_sand_strips_[i] = rounded(hourglass_portrait_, x, y, width, 4, 2,
                                                   color, LV_OPA_TRANSP, color, 0);
    }
    for (int i = 0; i < 12; ++i) {
        const int size = i < 7 ? (i % 2 ? 6 : 7) : (i % 2 ? 5 : 6);
        hourglass_falling_dots_[i] = circle(hourglass_portrait_, size, i < 7 ? sand_line : sand_soft, LV_OPA_TRANSP);
        lv_obj_set_style_shadow_width(hourglass_falling_dots_[i], 4, 0);
        lv_obj_set_style_shadow_color(hourglass_falling_dots_[i], sand_soft, 0);
        lv_obj_set_style_shadow_opa(hourglass_falling_dots_[i], LV_OPA_30, 0);
    }

    lv_obj_t* time_panel = rounded(hourglass_portrait_, 34, 366, 252, 66, 14,
                                   LV_COLOR_MAKE(0xff, 0xfb, 0xf3), LV_OPA_COVER,
                                   LV_COLOR_MAKE(0xe8, 0xc6, 0xa0), 2);
    lv_obj_set_style_outline_width(time_panel, 1, 0);
    lv_obj_set_style_outline_color(time_panel, LV_COLOR_MAKE(0xf1, 0xd3, 0xae), 0);
    lv_obj_set_style_outline_opa(time_panel, LV_OPA_50, 0);
    lv_obj_set_style_outline_pad(time_panel, -8, 0);

    hourglass_time_label_ = label_en(time_panel, "15:00", &style_en);
    lv_obj_set_width(hourglass_time_label_, 230);
    lv_obj_set_style_text_align(hourglass_time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hourglass_time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(hourglass_time_label_, brown, 0);
    lv_obj_align(hourglass_time_label_, LV_ALIGN_TOP_LEFT, 11, 0);

    hourglass_status_label_ = label_en(time_panel, "倒计时中", &style_gold);
    lv_obj_set_width(hourglass_status_label_, 230);
    lv_obj_set_style_text_align(hourglass_status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hourglass_status_label_, qd_cn_font_16(), 0);
    lv_obj_set_style_text_color(hourglass_status_label_, brown, 0);
    lv_obj_align(hourglass_status_label_, LV_ALIGN_TOP_LEFT, 11, 44);

    static constexpr const char* kLabels[4] = {"5 min", "10 min", "15 min", "20 min"};
    for (int i = 0; i < 4; ++i) {
        hourglass_preset_buttons_[i] = rounded(hourglass_portrait_, 12 + i * 78, 444, 64, 30, 10,
                                               LV_COLOR_MAKE(0xff, 0xf7, 0xe8), LV_OPA_COVER, brown, 2);
        lv_obj_add_flag(hourglass_preset_buttons_[i], LV_OBJ_FLAG_CLICKABLE);
        hourglass_preset_labels_[i] = label_en(hourglass_preset_buttons_[i], kLabels[i], &style_en);
        lv_obj_set_style_text_font(hourglass_preset_labels_[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(hourglass_preset_labels_[i], brown, 0);
        lv_obj_align(hourglass_preset_labels_[i], LV_ALIGN_CENTER, 0, 0);
    }

    hourglass_tick_timer_ = lv_timer_create(HourglassTickCb, 1000, this);
    hourglass_anim_timer_ = lv_timer_create(HourglassAnimCb, 100, this);
    lv_timer_pause(hourglass_tick_timer_);
    lv_timer_pause(hourglass_anim_timer_);
    ResetHourglassToDefault();
}

// ===== XiaoZhi page =====
void DesktopUI::CreateXiaozhiPage(lv_obj_t* root) {
    xiaozhi_page_ = lv_obj_create(root);
    lv_obj_add_style(xiaozhi_page_, &style_screen, 0);
    lv_obj_set_size(xiaozhi_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(xiaozhi_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(xiaozhi_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(xiaozhi_page_, xiaozhi_gesture_cb, LV_EVENT_GESTURE, NULL);

    // 全屏黑色背景，只有面部
    CreateFaceUI(xiaozhi_page_);

    music_lyric_panel_ = lv_obj_create(xiaozhi_page_);
    lv_obj_remove_style_all(music_lyric_panel_);
    lv_obj_set_size(music_lyric_panel_, 452, 58);
    lv_obj_align(music_lyric_panel_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_radius(music_lyric_panel_, 8, 0);
    lv_obj_set_style_bg_color(music_lyric_panel_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(music_lyric_panel_, LV_OPA_80, 0);
    lv_obj_set_style_border_color(music_lyric_panel_, COLOR_GOLD, 0);
    lv_obj_set_style_border_opa(music_lyric_panel_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(music_lyric_panel_, 1, 0);
    lv_obj_clear_flag(music_lyric_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(music_lyric_panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(music_lyric_panel_, LV_OBJ_FLAG_HIDDEN);

    music_lyric_label_ = label_en(music_lyric_panel_, "", &style_en);
    lv_obj_set_width(music_lyric_label_, 420);
    lv_obj_set_style_text_font(music_lyric_label_, qd_cn_font_20(), 0);
    lv_obj_set_style_text_color(music_lyric_label_, lv_color_white(), 0);
    lv_obj_set_style_text_align(music_lyric_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(music_lyric_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(music_lyric_label_);
    lv_obj_clear_flag(music_lyric_label_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(music_lyric_panel_);
}

void DesktopUI::CreateQrOverlay(lv_obj_t* root) {
    qr_overlay_ = lv_obj_create(root);
    lv_obj_remove_style_all(qr_overlay_);
    lv_obj_set_size(qr_overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(qr_overlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(qr_overlay_, LV_OPA_90, 0);
    lv_obj_clear_flag(qr_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(qr_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(qr_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(qr_overlay_, qr_overlay_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* card = lv_obj_create(qr_overlay_);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 328, 304);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    qr_title_label_ = label_en(card, "Scan to login", &style_en);
    lv_obj_set_width(qr_title_label_, 300);
    lv_obj_set_style_text_font(qr_title_label_, qd_cn_font_20(), 0);
    lv_obj_set_style_text_color(qr_title_label_, LV_COLOR_MAKE(0x20, 0x24, 0x28), 0);
    lv_obj_set_style_text_align(qr_title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(qr_title_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(qr_title_label_, LV_ALIGN_TOP_MID, 0, 2);

    qr_code_ = lv_qrcode_create(card);
    lv_qrcode_set_size(qr_code_, 214);
    lv_qrcode_set_dark_color(qr_code_, lv_color_black());
    lv_qrcode_set_light_color(qr_code_, lv_color_white());
    lv_obj_align(qr_code_, LV_ALIGN_TOP_MID, 0, 38);

    qr_hint_label_ = label_en(card, "Tap anywhere to close", &style_en);
    lv_obj_set_width(qr_hint_label_, 292);
    lv_obj_set_style_text_font(qr_hint_label_, qd_cn_font_16(), 0);
    lv_obj_set_style_text_color(qr_hint_label_, LV_COLOR_MAKE(0x4b, 0x55, 0x63), 0);
    lv_obj_set_style_text_align(qr_hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(qr_hint_label_, LV_LABEL_LONG_WRAP);
    lv_obj_align(qr_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -2);
}

bool DesktopUI::ShowQrCode(const char* content, const char* title, const char* hint) {
    if (!qr_overlay_ || !qr_code_ || !content || !content[0]) {
        return false;
    }

    const size_t content_len = std::strlen(content);
    if (content_len > 700) {
        ESP_LOGW(TAG, "ShowQrCode rejected: content too long len=%u",
                 static_cast<unsigned>(content_len));
        return false;
    }

    const lv_result_t result = lv_qrcode_update(qr_code_, content, content_len);
    if (result != LV_RESULT_OK) {
        ESP_LOGW(TAG, "ShowQrCode failed len=%u", static_cast<unsigned>(content_len));
        return false;
    }

    if (qr_title_label_) {
        lv_label_set_text(qr_title_label_, (title && title[0]) ? title : "扫码登录");
    }
    if (qr_hint_label_) {
        lv_label_set_text(qr_hint_label_,
                          (hint && hint[0]) ? hint : "用手机扫码；点屏幕关闭");
    }

    ShowPage(DesktopPage::XIAOZHI);
    lv_obj_clear_flag(qr_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(qr_overlay_);
    lv_obj_invalidate(qr_overlay_);
    ESP_LOGI(TAG, "ShowQrCode len=%u", static_cast<unsigned>(content_len));
    return true;
}

void DesktopUI::HideQrCode() {
    if (!qr_overlay_) {
        return;
    }
    lv_obj_add_flag(qr_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(qr_overlay_);
}

// ===== Network page =====
void DesktopUI::CreateNetworkPage(lv_obj_t* root) {
    network_page_ = lv_obj_create(root);
    lv_obj_add_style(network_page_, &style_screen, 0);
    lv_obj_set_size(network_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(network_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(network_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(network_page_, network_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t* logo = nullptr;
    lv_obj_t* owner = nullptr;
    create_brand_mark(network_page_, 18, 4, &logo, &owner);
    RegisterBrandLabels(logo, owner);

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

// ===== Diagnostics page =====
void DesktopUI::CreateDiagnosticsPage(lv_obj_t* root) {
    diagnostics_page_ = lv_obj_create(root);
    lv_obj_add_style(diagnostics_page_, &style_screen, 0);
    lv_obj_set_size(diagnostics_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(diagnostics_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(diagnostics_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(diagnostics_page_, diagnostics_gesture_cb, LV_EVENT_GESTURE, NULL);
    add_gesture_bubble(diagnostics_page_);

    lv_obj_t* logo = nullptr;
    lv_obj_t* owner = nullptr;
    create_brand_mark(diagnostics_page_, 18, 4, &logo, &owner);
    RegisterBrandLabels(logo, owner);
    CreateStatusBar(diagnostics_page_);

    lv_obj_t* title = label_en(diagnostics_page_, "Diagnostics", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t* sub = label_en(diagnostics_page_, "Long-press Settings", &style_muted);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 146, 53);

    lv_obj_t* back = CreateButton(diagnostics_page_, "Back", navigate_back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -22, 45);

    lv_obj_t* refresh = CreateButton(diagnostics_page_, "Refresh", diagnostics_open_cb);
    lv_obj_set_size(refresh, 92, 30);
    lv_obj_align(refresh, LV_ALIGN_TOP_RIGHT, -118, 46);

    lv_obj_t* panel = CreatePanel(diagnostics_page_, 432, 214, 24, 86);
    lv_obj_set_style_bg_color(panel,
                              is_tupi_warm_theme() ? COLOR_SURFACE :
                              themed_color(LV_COLOR_MAKE(0x0c, 0x10, 0x12), COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(panel,
                                  is_tupi_warm_theme() ? COLOR_LINE :
                                  themed_color(LV_COLOR_MAKE(0x36, 0x47, 0x56), COLOR_LINE), 0);
    lv_obj_set_style_radius(panel, 8, 0);

    for (size_t i = 0; i < sizeof(diagnostics_labels_) / sizeof(diagnostics_labels_[0]); ++i) {
        diagnostics_labels_[i] = label_en(panel, "--", &style_muted);
        lv_obj_set_style_text_font(diagnostics_labels_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(diagnostics_labels_[i], i == 0 ? COLOR_GOLD : COLOR_TEXT, 0);
        lv_obj_set_width(diagnostics_labels_[i], 404);
        lv_label_set_long_mode(diagnostics_labels_[i], LV_LABEL_LONG_DOT);
        lv_obj_align(diagnostics_labels_[i], LV_ALIGN_TOP_LEFT, 14, 8 + i * 20);
    }
}

void DesktopUI::RefreshDiagnostics() {
    if (!diagnostics_page_) {
        return;
    }

    const esp_app_desc_t* app_desc = esp_app_get_description();
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    auto& wifi = WifiStation::GetInstance();
    auto ssid_list = SsidManager::GetInstance().GetSsidList();

    const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t largest_psram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    char rows[10][128] = {};
    snprintf(rows[0], sizeof(rows[0]), "Version: %s  Board: %s",
             app_desc ? app_desc->version : "unknown", BOARD_NAME);
    snprintf(rows[1], sizeof(rows[1]), "Running: %s @0x%06lx size=%luKB",
             running ? running->label : "--",
             running ? static_cast<unsigned long>(running->address) : 0UL,
             running ? static_cast<unsigned long>(running->size / 1024) : 0UL);
    snprintf(rows[2], sizeof(rows[2]), "Next OTA: %s @0x%06lx size=%luKB",
             next ? next->label : "--",
             next ? static_cast<unsigned long>(next->address) : 0UL,
             next ? static_cast<unsigned long>(next->size / 1024) : 0UL);
    if (firmware_update_asset_size_ > 0 && firmware_update_partition_size_ > 0) {
        const long margin_kb = static_cast<long>(firmware_update_partition_size_ / 1024) -
                               static_cast<long>((firmware_update_asset_size_ + 1023) / 1024);
        snprintf(rows[3], sizeof(rows[3]), "OTA file: %uKB  slot=%uKB  margin=%ldKB",
                 static_cast<unsigned>((firmware_update_asset_size_ + 1023) / 1024),
                 static_cast<unsigned>(firmware_update_partition_size_ / 1024),
                 margin_kb);
    } else {
        snprintf(rows[3], sizeof(rows[3]), "OTA file: --  slot=%uKB",
                 static_cast<unsigned>((firmware_update_partition_size_ > 0
                     ? firmware_update_partition_size_
                     : (next ? next->size : 0)) / 1024));
    }
    snprintf(rows[4], sizeof(rows[4]), "Internal heap: free=%uKB largest=%uKB",
             static_cast<unsigned>(free_internal / 1024),
             static_cast<unsigned>(largest_internal / 1024));
    snprintf(rows[5], sizeof(rows[5]), "PSRAM: free=%uKB largest=%uKB",
             static_cast<unsigned>(free_psram / 1024),
             static_cast<unsigned>(largest_psram / 1024));
    if (wifi.IsConnected()) {
        snprintf(rows[6], sizeof(rows[6]), "WiFi: %s  %s  RSSI=%ddBm",
                 wifi.GetSsid().c_str(), wifi.GetIpAddress().c_str(), wifi.GetRssi());
    } else {
        snprintf(rows[6], sizeof(rows[6]), "WiFi: disconnected");
    }
    snprintf(rows[7], sizeof(rows[7]), "Saved WiFi: %u profile%s",
             static_cast<unsigned>(ssid_list.size()), ssid_list.size() == 1 ? "" : "s");
    if (battery_level_ < 0) {
        snprintf(rows[8], sizeof(rows[8]), "Battery: --");
    } else {
        snprintf(rows[8], sizeof(rows[8]), "Battery: %d%%%s",
                 battery_level_, battery_charging_ ? " charging" : "");
    }
    snprintf(rows[9], sizeof(rows[9]), "OTA: %s%s%s",
             firmware_update_status_.c_str(),
             firmware_update_busy_ ? " busy" : "",
             firmware_update_available_ ? " update-ready" : "");

    for (size_t i = 0; i < sizeof(diagnostics_labels_) / sizeof(diagnostics_labels_[0]); ++i) {
        if (diagnostics_labels_[i]) {
            lv_label_set_text(diagnostics_labels_[i], rows[i]);
        }
    }
}

// ===== Settings page =====
void DesktopUI::CreateSettingsPage(lv_obj_t* root) {
    settings_page_ = lv_obj_create(root);
    lv_obj_add_style(settings_page_, &style_screen, 0);
    lv_obj_set_size(settings_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(settings_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settings_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(settings_page_, settings_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t* logo = nullptr;
    lv_obj_t* owner = nullptr;
    create_brand_mark(settings_page_, 18, 4, &logo, &owner);
    RegisterBrandLabels(logo, owner);

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

    lv_obj_t* theme_title = label_en(settings_content_, "Appearance", &style_gold);
    lv_obj_set_style_text_font(theme_title, &lv_font_montserrat_16, 0);
    lv_obj_align(theme_title, LV_ALIGN_TOP_LEFT, 4, 166);

    lv_obj_t* theme_row = lv_obj_create(settings_content_);
    lv_obj_add_style(theme_row, &style_panel, 0);
    lv_obj_set_size(theme_row, 414, 58);
    lv_obj_align(theme_row, LV_ALIGN_TOP_LEFT, 0, 192);
    lv_obj_clear_flag(theme_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* theme_label = label_en(theme_row, "Theme", &style_en);
    lv_obj_align(theme_label, LV_ALIGN_TOP_LEFT, 14, 8);
    settings_theme_value_ = label_en(theme_row, theme().name, &style_muted);
    lv_obj_set_style_text_font(settings_theme_value_, &lv_font_montserrat_12, 0);
    lv_obj_align(settings_theme_value_, LV_ALIGN_BOTTOM_LEFT, 14, -9);
    settings_theme_button_ = lv_btn_create(theme_row);
    lv_obj_set_size(settings_theme_button_, 84, 30);
    lv_obj_set_style_radius(settings_theme_button_, 15, 0);
    lv_obj_set_style_bg_color(settings_theme_button_, COLOR_SURFACE_2, 0);
    lv_obj_set_style_border_color(settings_theme_button_, COLOR_PURPLE, 0);
    lv_obj_set_style_border_width(settings_theme_button_, 1, 0);
    lv_obj_add_event_cb(settings_theme_button_, settings_theme_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(settings_theme_button_, LV_ALIGN_RIGHT_MID, -14, 0);
    settings_theme_button_label_ = label_en(settings_theme_button_, "Switch", &style_en);
    lv_obj_set_style_text_font(settings_theme_button_label_, &lv_font_montserrat_12, 0);
    lv_obj_center(settings_theme_button_label_);

    lv_obj_t* weather_title = label_en(settings_content_, "Weather", &style_gold);
    lv_obj_set_style_text_font(weather_title, &lv_font_montserrat_16, 0);
    lv_obj_align(weather_title, LV_ALIGN_TOP_LEFT, 4, 258);

    lv_obj_t* weather_row = lv_obj_create(settings_content_);
    lv_obj_add_style(weather_row, &style_panel, 0);
    lv_obj_set_size(weather_row, 414, 58);
    lv_obj_align(weather_row, LV_ALIGN_TOP_LEFT, 0, 284);
    lv_obj_clear_flag(weather_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* weather_label = label_en(weather_row, "Location", &style_en);
    lv_obj_align(weather_label, LV_ALIGN_TOP_LEFT, 14, 8);
    settings_weather_value_ = label_en(weather_row, "Zhongshan", &style_muted);
    lv_obj_set_width(settings_weather_value_, 260);
    lv_label_set_long_mode(settings_weather_value_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(settings_weather_value_, qd_cn_font_16(), 0);
    lv_obj_align(settings_weather_value_, LV_ALIGN_BOTTOM_LEFT, 14, -9);
    settings_ble_status_label_ = label_en(weather_row, settings_ble_status_.c_str(), &style_green);
    lv_obj_set_width(settings_ble_status_label_, 116);
    lv_label_set_long_mode(settings_ble_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(settings_ble_status_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(settings_ble_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(settings_ble_status_label_, LV_ALIGN_BOTTOM_RIGHT, -14, -9);

    lv_obj_t* profile_title = label_en(settings_content_, "Phone Sync", &style_gold);
    lv_obj_set_style_text_font(profile_title, &lv_font_montserrat_16, 0);
    lv_obj_align(profile_title, LV_ALIGN_TOP_LEFT, 4, 350);

    lv_obj_t* profile_row = lv_obj_create(settings_content_);
    lv_obj_add_style(profile_row, &style_panel, 0);
    lv_obj_set_size(profile_row, 414, 74);
    lv_obj_align(profile_row, LV_ALIGN_TOP_LEFT, 0, 376);
    lv_obj_clear_flag(profile_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* profile_label = label_en(profile_row, "Profile", &style_en);
    lv_obj_align(profile_label, LV_ALIGN_TOP_LEFT, 14, 9);
    settings_profile_logo_value_ = label_en(profile_row, "nothing impossible", &style_gold);
    lv_obj_set_width(settings_profile_logo_value_, 210);
    lv_label_set_long_mode(settings_profile_logo_value_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(settings_profile_logo_value_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(settings_profile_logo_value_, qd_cn_font_16(), 0);
    lv_obj_align(settings_profile_logo_value_, LV_ALIGN_TOP_RIGHT, -14, 11);
    lv_obj_t* owner_label = label_en(profile_row, "Owner", &style_en);
    lv_obj_align(owner_label, LV_ALIGN_BOTTOM_LEFT, 14, -11);
    settings_profile_owner_value_ = label_en(profile_row, "tupi", &style_green);
    lv_obj_set_width(settings_profile_owner_value_, 230);
    lv_label_set_long_mode(settings_profile_owner_value_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(settings_profile_owner_value_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(settings_profile_owner_value_, qd_cn_font_16(), 0);
    lv_obj_align(settings_profile_owner_value_, LV_ALIGN_BOTTOM_RIGHT, -14, -12);

    lv_obj_t* wifi_sync_row = lv_obj_create(settings_content_);
    lv_obj_add_style(wifi_sync_row, &style_panel, 0);
    lv_obj_set_size(wifi_sync_row, 414, 58);
    lv_obj_align(wifi_sync_row, LV_ALIGN_TOP_LEFT, 0, 458);
    lv_obj_clear_flag(wifi_sync_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* wifi_sync_label = label_en(wifi_sync_row, "Phone Web", &style_en);
    lv_obj_align(wifi_sync_label, LV_ALIGN_TOP_LEFT, 14, 8);
    settings_wifi_config_status_label_ = label_en(wifi_sync_row, settings_wifi_config_status_.c_str(), &style_muted);
    lv_obj_set_width(settings_wifi_config_status_label_, 260);
    lv_label_set_long_mode(settings_wifi_config_status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(settings_wifi_config_status_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(settings_wifi_config_status_label_, LV_ALIGN_BOTTOM_LEFT, 14, -9);

    lv_obj_t* firmware_title = label_en(settings_content_, "Firmware", &style_gold);
    lv_obj_set_style_text_font(firmware_title, &lv_font_montserrat_16, 0);
    lv_obj_align(firmware_title, LV_ALIGN_TOP_LEFT, 4, 550);

    lv_obj_t* firmware_row = lv_obj_create(settings_content_);
    lv_obj_add_style(firmware_row, &style_panel, 0);
    lv_obj_set_size(firmware_row, 414, 74);
    lv_obj_align(firmware_row, LV_ALIGN_TOP_LEFT, 0, 576);
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
    const FaceMetrics metrics = face_metrics();

    if (is_themed_face_gif_theme()) {
        lv_obj_set_style_bg_color(parent, COLOR_SURFACE, 0);
        lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

        lv_obj_t* status_pill = lv_obj_create(parent);
        lv_obj_remove_style_all(status_pill);
        lv_obj_set_size(status_pill, 420, 44);
        lv_obj_set_style_radius(status_pill, 22, 0);
        lv_obj_set_style_bg_color(status_pill, COLOR_SURFACE_2, 0);
        lv_obj_set_style_bg_opa(status_pill, LV_OPA_90, 0);
        lv_obj_set_style_border_color(status_pill, COLOR_LINE, 0);
        lv_obj_set_style_border_width(status_pill, 1, 0);
        lv_obj_align(status_pill, LV_ALIGN_BOTTOM_MID, 0, -12);
        add_gesture_bubble(status_pill);

        lv_obj_t* audio_mark = label_en(status_pill, "|||", &style_gold);
        lv_obj_set_style_text_font(audio_mark, &lv_font_montserrat_20, 0);
        lv_obj_align(audio_mark, LV_ALIGN_LEFT_MID, 18, 0);

        xiaozhi_state_label_ = label_en(status_pill, "Standby", &style_gold);
        lv_obj_set_width(xiaozhi_state_label_, 72);
        lv_obj_set_style_text_font(xiaozhi_state_label_, &lv_font_montserrat_12, 0);
        lv_label_set_long_mode(xiaozhi_state_label_, LV_LABEL_LONG_DOT);
        lv_obj_align(xiaozhi_state_label_, LV_ALIGN_LEFT_MID, 54, 0);

        xiaozhi_message_label_ = label_en(status_pill, "Ready", &style_en);
        lv_obj_set_width(xiaozhi_message_label_, 260);
        lv_obj_set_style_text_font(xiaozhi_message_label_, qd_cn_font_16(), 0);
        lv_obj_set_style_text_align(xiaozhi_message_label_, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(xiaozhi_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(xiaozhi_message_label_, LV_ALIGN_LEFT_MID, 132, 0);
        return;
    }
    // 全屏黑色背景，直接在parent上创建元素

    // 左眼白
    eye_left_ = lv_obj_create(parent);
    lv_obj_set_size(eye_left_, metrics.eye_w, metrics.eye_h);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -metrics.eye_x, metrics.eye_y);
    lv_obj_set_style_radius(eye_left_, metrics.eye_radius, 0);
    lv_obj_set_style_bg_color(eye_left_, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(eye_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_left_, 0, 0);
    add_gesture_bubble(eye_left_);

    // 左瞳孔
    pupil_left_ = lv_obj_create(eye_left_);
    lv_obj_set_size(pupil_left_, metrics.pupil_w, metrics.pupil_h);
    lv_obj_align(pupil_left_, LV_ALIGN_CENTER, 0, metrics.pupil_y);
    lv_obj_set_style_radius(pupil_left_, metrics.pupil_w / 2, 0);
    lv_obj_set_style_bg_color(pupil_left_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pupil_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_left_, 0, 0);

    // 左眼高光
    highlight_left_ = lv_obj_create(eye_left_);
    lv_obj_set_size(highlight_left_, metrics.highlight_size, metrics.highlight_size);
    lv_obj_align(highlight_left_, LV_ALIGN_CENTER, metrics.highlight_x, metrics.highlight_y);
    lv_obj_set_style_radius(highlight_left_, metrics.highlight_size / 2, 0);
    lv_obj_set_style_bg_color(highlight_left_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(highlight_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(highlight_left_, 0, 0);

    // 右眼白
    eye_right_ = lv_obj_create(parent);
    lv_obj_set_size(eye_right_, metrics.eye_w, metrics.eye_h);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, metrics.eye_x, metrics.eye_y);
    lv_obj_set_style_radius(eye_right_, metrics.eye_radius, 0);
    lv_obj_set_style_bg_color(eye_right_, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(eye_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_right_, 0, 0);
    add_gesture_bubble(eye_right_);

    // 右瞳孔
    pupil_right_ = lv_obj_create(eye_right_);
    lv_obj_set_size(pupil_right_, metrics.pupil_w, metrics.pupil_h);
    lv_obj_align(pupil_right_, LV_ALIGN_CENTER, 0, metrics.pupil_y);
    lv_obj_set_style_radius(pupil_right_, metrics.pupil_w / 2, 0);
    lv_obj_set_style_bg_color(pupil_right_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pupil_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_right_, 0, 0);

    // 右眼高光
    highlight_right_ = lv_obj_create(eye_right_);
    lv_obj_set_size(highlight_right_, metrics.highlight_size, metrics.highlight_size);
    lv_obj_align(highlight_right_, LV_ALIGN_CENTER, metrics.highlight_x, metrics.highlight_y);
    lv_obj_set_style_radius(highlight_right_, metrics.highlight_size / 2, 0);
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
    lv_obj_set_size(mouth_, metrics.mouth_idle_w, metrics.mouth_idle_h);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, metrics.mouth_y);
    lv_obj_set_style_radius(mouth_, metrics.mouth_idle_h / 2, 0);
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
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(wifi, COLOR_TEXT, 0);
    }
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
    if (is_tupi_warm_theme()) {
        lv_obj_set_style_text_color(battery, COLOR_TEXT, 0);
    }
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
                lv_obj_set_style_border_color(calendar_day_cells_[i],
                                              themed_color(LV_COLOR_MAKE(0x5d, 0x40, 0x2b), COLOR_LINE), 0);
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

        lv_color_t bg_color = themed_color(lv_color_make(0x17, 0x0f, 0x0a), COLOR_SURFACE_2);
        lv_color_t border_color = themed_color(lv_color_make(0x35, 0x26, 0x1c), COLOR_LINE);
        lv_color_t text_color = themed_color(lv_color_make(0x72, 0x58, 0x44), COLOR_MUTED);
        lv_opa_t bg_opa = LV_OPA_30;
        if (is_today) {
            bg_color = themed_color(lv_color_make(0xff, 0xc1, 0x70), COLOR_PURPLE);
            border_color = bg_color;
            text_color = themed_color(lv_color_make(0x20, 0x16, 0x10), COLOR_CREAM);
            bg_opa = LV_OPA_COVER;
        } else if (weekend && in_current_month) {
            bg_color = themed_color(lv_color_make(0xe7, 0x91, 0x42), COLOR_CREAM);
            border_color = bg_color;
            text_color = themed_color(lv_color_make(0xff, 0xd0, 0x94), COLOR_GOLD);
            bg_opa = LV_OPA_COVER;
        } else if (in_current_month) {
            bg_color = themed_color(lv_color_make(0x24, 0x18, 0x10), COLOR_SURFACE);
            border_color = themed_color(lv_color_make(0x5d, 0x40, 0x2b), COLOR_LINE);
            text_color = themed_color(lv_color_make(0xff, 0xf5, 0xe4), COLOR_TEXT);
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

static const lv_image_dsc_t* cat_face_for_state(DeviceState state, const std::string& emotion) {
    if (state == kDeviceStateSpeaking) {
        return &qd_cat_speaking;
    }
    if (state == kDeviceStateListening || state == kDeviceStateConnecting) {
        return &qd_cat_listening;
    }
    if (emotion == "happy" || emotion == "laughing" || emotion == "funny" ||
        emotion == "loving" || emotion == "delicious" || emotion == "kissy") {
        return &qd_cat_happy;
    }
    if (emotion == "sad" || emotion == "crying") {
        return &qd_cat_sad;
    }
    if (emotion == "angry") {
        return &qd_cat_angry;
    }
    if (emotion == "surprised" || emotion == "shocked") {
        return &qd_cat_surprised;
    }
    if (emotion == "thinking" || emotion == "confused") {
        return &qd_cat_thinking;
    }
    if (emotion == "sleepy" || emotion == "relaxed") {
        return &qd_cat_sleepy;
    }
    return &qd_cat_standby;
}

static const lv_image_dsc_t* classic_bot_face_for_state(DeviceState state, const std::string& emotion) {
    if (state == kDeviceStateSpeaking) {
        return &qd_classic_bot_speaking;
    }
    if (state == kDeviceStateListening || state == kDeviceStateAudioTesting ||
        state == kDeviceStateConnecting || state == kDeviceStateStarting ||
        state == kDeviceStateWifiConfiguring || state == kDeviceStateActivating) {
        return &qd_classic_bot_listening;
    }
    if (emotion == "surprised" || emotion == "shocked") {
        return &qd_classic_bot_speaking;
    }
    return &qd_classic_bot_standby;
}

static const lv_image_dsc_t* tupi_bot_face_for_state(DeviceState state, const std::string& emotion) {
    if (state == kDeviceStateFatalError) {
        return &qd_tupi_bot_sad;
    }
    if (state == kDeviceStateUpgrading) {
        return &qd_tupi_bot_thinking;
    }
    if (state == kDeviceStateSpeaking) {
        return &qd_tupi_bot_speaking;
    }
    if (state == kDeviceStateListening || state == kDeviceStateAudioTesting) {
        return &qd_tupi_bot_listening;
    }
    if (state == kDeviceStateConnecting || state == kDeviceStateStarting ||
        state == kDeviceStateWifiConfiguring || state == kDeviceStateActivating) {
        return &qd_tupi_bot_listening;
    }
    if (emotion == "happy" || emotion == "laughing" || emotion == "funny" ||
        emotion == "loving" || emotion == "delicious" || emotion == "kissy") {
        return &qd_tupi_bot_happy;
    }
    if (emotion == "sad" || emotion == "crying") {
        return &qd_tupi_bot_sad;
    }
    if (emotion == "angry") {
        return &qd_tupi_bot_angry;
    }
    if (emotion == "surprised" || emotion == "shocked") {
        return &qd_tupi_bot_surprised;
    }
    if (emotion == "thinking" || emotion == "confused") {
        return &qd_tupi_bot_thinking;
    }
    if (emotion == "sleepy" || emotion == "relaxed") {
        return &qd_tupi_bot_sleepy;
    }
    return &qd_tupi_bot_standby;
}

static const lv_image_dsc_t* themed_face_for_state(DeviceState state, const std::string& emotion) {
    if (is_tupi_warm_theme()) {
        return tupi_bot_face_for_state(state, emotion);
    }
    if (is_cat_theme()) {
        return cat_face_for_state(state, emotion);
    }
    return classic_bot_face_for_state(state, emotion);
}

void DesktopUI::EnsureThemedFaceGif() {
    if (!is_themed_face_gif_theme() || !xiaozhi_page_ || themed_face_gif_) {
        return;
    }
    const DeviceState state = Application::GetInstance().GetDeviceState();
    themed_face_src_ = themed_face_for_state(state, emotion_);
    themed_face_gif_ = lv_gif_create(xiaozhi_page_);
    lv_gif_set_src(themed_face_gif_, themed_face_src_);
    lv_obj_set_size(themed_face_gif_, 300, 238);
    lv_obj_set_style_bg_color(themed_face_gif_, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(themed_face_gif_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(themed_face_gif_, 0, 0);
    lv_obj_align(themed_face_gif_, LV_ALIGN_CENTER, 0, -18);
    lv_obj_move_to_index(themed_face_gif_, 0);
    add_gesture_bubble(themed_face_gif_);
}

void DesktopUI::ReleaseThemedFaceGif() {
    if (themed_face_gif_) {
        lv_obj_del(themed_face_gif_);
        themed_face_gif_ = nullptr;
        themed_face_src_ = nullptr;
    }
}

static const char* themed_face_state_text(DeviceState state) {
    switch (state) {
        case kDeviceStateStarting:
        case kDeviceStateWifiConfiguring:
        case kDeviceStateActivating:
        case kDeviceStateConnecting:
            return "Connecting";
        case kDeviceStateListening:
        case kDeviceStateAudioTesting:
            return "Listening";
        case kDeviceStateSpeaking:
            return "Speaking";
        case kDeviceStateUpgrading:
            return "Upgrading";
        case kDeviceStateFatalError:
            return "Error";
        default:
            return "Standby";
    }
}

void DesktopUI::UpdateFaceAnimation() {
    // The face page is hidden most of the time. Updating its objects every
    // 100 ms still invalidates LVGL and, with this panel's FULL renderer,
    // causes an otherwise invisible 480x320 transfer on every tick.
    if (current_page_ != DesktopPage::XIAOZHI) {
        return;
    }
    if (is_themed_face_gif_theme()) {
        EnsureThemedFaceGif();
        if (!themed_face_gif_) {
            return;
        }
        const DeviceState state = Application::GetInstance().GetDeviceState();
        const lv_image_dsc_t* next_src = themed_face_for_state(state, emotion_);
        if (next_src != themed_face_src_) {
            themed_face_src_ = next_src;
            lv_gif_set_src(themed_face_gif_, themed_face_src_);
            lv_gif_restart(themed_face_gif_);
        }
        return;
    }

    if (!eye_left_ || !eye_right_ || !mouth_) return;

    const FaceMetrics metrics = face_metrics();
    const DeviceState state = Application::GetInstance().GetDeviceState();
    const bool speaking = state == kDeviceStateSpeaking;
    const bool listening = state == kDeviceStateListening || state == kDeviceStateConnecting;
    const bool cat = is_cat_theme();
    const bool happy = emotion_ == "happy" || emotion_ == "laughing" ||
                       emotion_ == "funny" || emotion_ == "loving" ||
                       emotion_ == "delicious" || emotion_ == "kissy";
    const bool sad = emotion_ == "sad" || emotion_ == "crying";
    const bool angry = emotion_ == "angry";
    const bool surprised = emotion_ == "surprised" || emotion_ == "shocked";
    const bool thinking = emotion_ == "thinking" || emotion_ == "confused";
    const bool sleepy = emotion_ == "sleepy" || emotion_ == "relaxed";
    const bool winking = emotion_ == "winking";

    // ===== 眨眼动画 =====
    int eye_h = metrics.eye_h;
    if (speaking) {
        eye_h = metrics.eye_h_speaking_base + (int)(metrics.eye_h_speaking_amp * sin(anim_tick_ * 0.2));
    } else if (listening) {
        eye_h = metrics.eye_h_listening;
    } else if (cat && (happy || sleepy || winking)) {
        eye_h = metrics.eye_h_blink;
    } else if (cat && sad) {
        eye_h = metrics.eye_h - 8;
    } else if (cat && angry) {
        eye_h = metrics.eye_h - 10;
    } else if (cat && surprised) {
        eye_h = metrics.eye_h + 4;
    } else {
        const int blink_phase = anim_tick_ % 60;
        if (blink_phase >= 55) {
            eye_h = metrics.eye_h_blink;
        }
    }

    lv_obj_set_width(eye_left_, metrics.eye_w);
    lv_obj_set_width(eye_right_, metrics.eye_w);
    lv_obj_set_height(eye_left_, eye_h);
    lv_obj_set_height(eye_right_, eye_h);
    lv_obj_set_style_radius(eye_left_, std::min(metrics.eye_radius, eye_h / 2), 0);
    lv_obj_set_style_radius(eye_right_, std::min(metrics.eye_radius, eye_h / 2), 0);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -metrics.eye_x, metrics.eye_y);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, metrics.eye_x, metrics.eye_y);

    // ===== 瞳孔随机移动 =====
    if (anim_tick_ % 40 == 0) {
        pupil_target_x_ = (float)((rand() % (metrics.pupil_move_x * 2 + 1)) - metrics.pupil_move_x);
        pupil_target_y_ = (float)((rand() % (metrics.pupil_move_y * 2 + 1)) - metrics.pupil_move_y);
    }
    pupil_offset_x_ += (pupil_target_x_ - pupil_offset_x_) * 0.08f;
    pupil_offset_y_ += (pupil_target_y_ - pupil_offset_y_) * 0.08f;
    const int emotion_pupil_y = cat && sad ? 4 : (cat && surprised ? -2 : 0);

    const bool eyes_closed = eye_h <= metrics.eye_h_blink + 2;
    if (pupil_left_) {
        lv_obj_set_size(pupil_left_, metrics.pupil_w, metrics.pupil_h);
        lv_obj_set_style_radius(pupil_left_, metrics.pupil_w / 2, 0);
        if (eyes_closed) {
            lv_obj_add_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(pupil_left_, LV_ALIGN_CENTER, (int)pupil_offset_x_,
                         metrics.pupil_y + (int)pupil_offset_y_ + emotion_pupil_y);
        }
    }
    if (pupil_right_) {
        lv_obj_set_size(pupil_right_, metrics.pupil_w, metrics.pupil_h);
        lv_obj_set_style_radius(pupil_right_, metrics.pupil_w / 2, 0);
        if (eyes_closed) {
            lv_obj_add_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(pupil_right_, LV_ALIGN_CENTER, (int)pupil_offset_x_,
                         metrics.pupil_y + (int)pupil_offset_y_ + emotion_pupil_y);
        }
    }
    if (highlight_left_) {
        if (eyes_closed) {
            lv_obj_add_flag(highlight_left_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(highlight_left_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(highlight_left_, LV_ALIGN_CENTER,
                         metrics.highlight_x + (int)pupil_offset_x_,
                         metrics.highlight_y + (int)pupil_offset_y_ + emotion_pupil_y);
        }
    }
    if (highlight_right_) {
        if (eyes_closed) {
            lv_obj_add_flag(highlight_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(highlight_right_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(highlight_right_, LV_ALIGN_CENTER,
                         metrics.highlight_x + (int)pupil_offset_x_,
                         metrics.highlight_y + (int)pupil_offset_y_ + emotion_pupil_y);
        }
    }

    // ===== 眉毛动画 =====
    int eyebrow_y = metrics.eyebrow_y;
    if (speaking) {
        eyebrow_y = metrics.eyebrow_speaking_y + (int)(3 * sin(anim_tick_ * 0.15));
    } else if (listening) {
        eyebrow_y = metrics.eyebrow_listening_y;
    } else if (sad) {
        eyebrow_y = metrics.eyebrow_sad_y;
    } else if (cat && angry) {
        eyebrow_y = metrics.eyebrow_y + 10;
    } else if (cat && surprised) {
        eyebrow_y = metrics.eyebrow_y - 7;
    }

    if (eyebrow_left_) {
        if (cat) {
            lv_obj_set_size(eyebrow_left_, 30, 5);
            lv_obj_set_style_radius(eyebrow_left_, 3, 0);
            lv_obj_set_style_transform_rotation(eyebrow_left_, angry ? -140 : (sad ? 120 : 0), 0);
        }
        lv_obj_align(eyebrow_left_, LV_ALIGN_CENTER, -metrics.eyebrow_x, eyebrow_y);
    }
    if (eyebrow_right_) {
        if (cat) {
            lv_obj_set_size(eyebrow_right_, 30, 5);
            lv_obj_set_style_radius(eyebrow_right_, 3, 0);
            lv_obj_set_style_transform_rotation(eyebrow_right_, angry ? 140 : (sad ? -120 : 0), 0);
        }
        lv_obj_align(eyebrow_right_, LV_ALIGN_CENTER, metrics.eyebrow_x, eyebrow_y);
    }

    if (blush_left_ && blush_right_) {
        const lv_opa_t blush_opa = (cat && (happy || speaking || listening)) ? LV_OPA_60 :
                                   (cat && sad) ? LV_OPA_30 : LV_OPA_40;
        lv_obj_set_style_bg_opa(blush_left_, blush_opa, 0);
        lv_obj_set_style_bg_opa(blush_right_, blush_opa, 0);
        lv_obj_align(blush_left_, LV_ALIGN_CENTER, -62, cat && sad ? 6 : 0);
        lv_obj_align(blush_right_, LV_ALIGN_CENTER, 62, cat && sad ? 6 : 0);
    }

    if (cat_status_mark_1_) {
        const bool show = cat && (thinking || listening || sad);
        lv_label_set_text(cat_status_mark_1_, thinking ? "?" : (sad ? "." : "("));
        if (show) lv_obj_clear_flag(cat_status_mark_1_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(cat_status_mark_1_, LV_OBJ_FLAG_HIDDEN);
    }
    if (cat_status_mark_2_) {
        const bool show = cat && (surprised || angry);
        lv_label_set_text(cat_status_mark_2_, angry ? "!!" : "!");
        lv_obj_set_style_text_color(cat_status_mark_2_, angry ? cat_nose_color() : cat_fur_shadow(), 0);
        if (show) lv_obj_clear_flag(cat_status_mark_2_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(cat_status_mark_2_, LV_OBJ_FLAG_HIDDEN);
    }
    if (cat_status_mark_3_) {
        const bool show = cat && sleepy;
        lv_label_set_text(cat_status_mark_3_, (anim_tick_ / 18) % 2 ? "z" : "Z");
        if (show) lv_obj_clear_flag(cat_status_mark_3_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(cat_status_mark_3_, LV_OBJ_FLAG_HIDDEN);
    }

    // ===== 嘴巴动画 =====
    if (speaking) {
        const int phase = anim_tick_ % 6;
        const int mouth_h = metrics.mouth_speaking_h[phase];
        lv_obj_set_height(mouth_, mouth_h);
        lv_obj_set_width(mouth_, metrics.mouth_speaking_w + (int)(metrics.mouth_speaking_w_amp * sin(anim_tick_ * 0.3)));
        lv_obj_set_style_bg_color(mouth_, cat ? cat_nose_color() : COLOR_GREEN, 0);
        lv_obj_set_style_radius(mouth_, mouth_h / 2, 0);
    } else if (listening) {
        lv_obj_set_height(mouth_, metrics.mouth_listening_h);
        lv_obj_set_width(mouth_, metrics.mouth_listening_w);
        lv_obj_set_style_bg_color(mouth_, cat ? cat_nose_color() : COLOR_GOLD, 0);
        lv_obj_set_style_radius(mouth_, metrics.mouth_listening_h / 2, 0);
    } else if (cat && surprised) {
        lv_obj_set_height(mouth_, 22);
        lv_obj_set_width(mouth_, 22);
        lv_obj_set_style_bg_color(mouth_, cat_nose_color(), 0);
        lv_obj_set_style_radius(mouth_, 11, 0);
    } else if (cat && happy) {
        lv_obj_set_height(mouth_, 14);
        lv_obj_set_width(mouth_, 38);
        lv_obj_set_style_bg_color(mouth_, cat_nose_color(), 0);
        lv_obj_set_style_radius(mouth_, 7, 0);
    } else if (cat && (sad || angry || thinking || sleepy)) {
        lv_obj_set_height(mouth_, sad ? 5 : 6);
        lv_obj_set_width(mouth_, sad ? 24 : (angry ? 30 : 20));
        lv_obj_set_style_bg_color(mouth_, cat_nose_color(), 0);
        lv_obj_set_style_radius(mouth_, 3, 0);
    } else {
        lv_obj_set_height(mouth_, metrics.mouth_idle_h);
        lv_obj_set_width(mouth_, metrics.mouth_idle_w);
        lv_obj_set_style_bg_color(mouth_, cat ? cat_nose_color() : COLOR_GOLD, 0);
        lv_obj_set_style_radius(mouth_, metrics.mouth_idle_h / 2, 0);
    }
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, metrics.mouth_y);
    if (cat_nose_) {
        lv_obj_align(cat_nose_, LV_ALIGN_CENTER, 0, cat && sad ? 10 : 7);
    }
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
    if (!minute_changed && !date_changed) {
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
    if (date_changed) {
        ReconcileFocusDate(false);
    }
    if (calendar_year_ <= 0 || calendar_month_ <= 0 || calendar_follow_today_) {
        calendar_year_ = year;
        calendar_month_ = month;
        calendar_follow_today_ = true;
    }

    if (minute_changed) {
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
    if (date_changed) {
        char app_status[24];
        snprintf(app_status, sizeof(app_status), "%04d/%02d/%02d", year, month, day);
        SetAppTileStatus(4, app_status, is_tupi_warm_theme() ? COLOR_GOLD : COLOR_PURPLE);
    }

    if (date_changed) {
        RenderCalendar();
    }
    if (current_page_ == DesktopPage::MAIN && main_page_ && (minute_changed || date_changed)) {
        lv_obj_invalidate(main_page_);
    }
}

void DesktopUI::SetMainPageCallback(std::function<void()> callback) {
    main_page_callback_ = std::move(callback);
}

void DesktopUI::SetFocusRotationCallback(std::function<void(bool)> callback) {
    focus_rotation_callback_ = std::move(callback);
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
    if (photo_title_label_) {
        lv_obj_add_flag(photo_title_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (photo_detail_label_) {
        lv_obj_add_flag(photo_detail_label_, LV_OBJ_FLAG_HIDDEN);
    }

    std::string state = title ? title : "";
    lv_color_t color = COLOR_GREEN;
    if (state == "Photos") {
        state = "Ready";
    } else if (state == "Refreshing" || state == "Scanning") {
        color = COLOR_GOLD;
    } else if (state.find("No ") == 0 || state.find("failed") != std::string::npos ||
               state.find("Failed") != std::string::npos || state == "SD card not ready" ||
               state == "Decode failed" || state == "Photos unavailable") {
        color = lv_color_make(0xff, 0x88, 0x68);
    } else if (state.empty()) {
        state = detail && detail[0] ? detail : "SD Slideshow";
    }
    photo_app_status_ = clean_subtitle_text(state.c_str(), 18);
    if (photo_app_status_.empty()) {
        photo_app_status_ = "SD Slideshow";
    }
    photo_app_color_ = color;
    SetAppTileStatus(1, photo_app_status_.c_str(),
                     photo_app_status_ == "SD Slideshow" ? COLOR_GREEN : photo_app_color_);
}

void DesktopUI::SetPhotoFrame(const lv_img_dsc_t* image, const lv_img_dsc_t* background,
                              const char* title, const char* detail) {
    SetPhotoState(title, detail);
    if (!photo_bg_a_ || !photo_bg_b_ || !photo_image_a_ || !photo_image_b_ || !image) {
        if (photo_bg_a_) {
            lv_obj_set_style_opa(photo_bg_a_, LV_OPA_TRANSP, 0);
        }
        if (photo_bg_b_) {
            lv_obj_set_style_opa(photo_bg_b_, LV_OPA_TRANSP, 0);
        }
        if (photo_image_a_) {
            lv_obj_set_style_opa(photo_image_a_, LV_OPA_TRANSP, 0);
        }
        if (photo_image_b_) {
            lv_obj_set_style_opa(photo_image_b_, LV_OPA_TRANSP, 0);
        }
        return;
    }

    lv_obj_t* next_bg = photo_show_a_ ? photo_bg_b_ : photo_bg_a_;
    lv_obj_t* prev_bg = photo_show_a_ ? photo_bg_a_ : photo_bg_b_;
    lv_obj_t* next = photo_show_a_ ? photo_image_b_ : photo_image_a_;
    lv_obj_t* prev = photo_show_a_ ? photo_image_a_ : photo_image_b_;
    photo_show_a_ = !photo_show_a_;

    if (photo_title_label_) {
        lv_obj_add_flag(photo_title_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (photo_detail_label_) {
        lv_obj_add_flag(photo_detail_label_, LV_OBJ_FLAG_HIDDEN);
    }

    if (background) {
        lv_image_set_src(next_bg, background);
        int32_t bg_scale_x = background->header.w > 0 ? (DISPLAY_WIDTH * 256) / background->header.w : 256;
        int32_t bg_scale_y = background->header.h > 0 ? (DISPLAY_HEIGHT * 256) / background->header.h : 256;
        int32_t bg_scale = LV_MAX(bg_scale_x, bg_scale_y);
        if (bg_scale <= 0) {
            bg_scale = 256;
        }
        lv_image_set_scale(next_bg, bg_scale);
        lv_obj_center(next_bg);
        lv_obj_set_style_opa(next_bg, LV_OPA_TRANSP, 0);
        lv_obj_move_background(next_bg);
    } else {
        lv_obj_set_style_opa(next_bg, LV_OPA_TRANSP, 0);
    }

    lv_image_set_src(next, image);
    int32_t scale_x = image->header.w > 0 ? (DISPLAY_WIDTH * 256) / image->header.w : 256;
    int32_t scale_y = image->header.h > 0 ? (DISPLAY_HEIGHT * 256) / image->header.h : 256;
    int32_t scale = background ? LV_MIN(scale_x, scale_y) : LV_MAX(scale_x, scale_y);
    if (scale <= 0) {
        scale = 256;
    }
    lv_image_set_scale(next, scale);
    lv_obj_center(next);
    lv_obj_set_style_opa(next, LV_OPA_TRANSP, 0);

    lv_obj_t* fade_in_objs[] = {next_bg, next};
    for (lv_obj_t* obj : fade_in_objs) {
        lv_anim_t fade_in;
        lv_anim_init(&fade_in);
        lv_anim_set_var(&fade_in, obj);
        lv_anim_set_values(&fade_in, LV_OPA_TRANSP, obj == next_bg && !background ? LV_OPA_TRANSP : LV_OPA_COVER);
        lv_anim_set_time(&fade_in, 650);
        lv_anim_set_exec_cb(&fade_in, ObjOpaCb);
        lv_anim_start(&fade_in);
    }

    lv_obj_t* fade_out_objs[] = {prev_bg, prev};
    for (lv_obj_t* obj : fade_out_objs) {
        lv_anim_t fade_out;
        lv_anim_init(&fade_out);
        lv_anim_set_var(&fade_out, obj);
        lv_anim_set_values(&fade_out, lv_obj_get_style_opa(obj, 0), LV_OPA_TRANSP);
        lv_anim_set_time(&fade_out, 650);
        lv_anim_set_exec_cb(&fade_out, ObjOpaCb);
        lv_anim_start(&fade_out);
    }
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

    if (!fc_playing_view_) {
        std::string state = title ? title : "";
        std::string detail_text = detail ? detail : "";
        lv_color_t color = COLOR_GREEN;
        if (state == "Select ROM") {
            state = detail_text.empty() ? "Ready" : clean_subtitle_text(detail, 18);
        } else if (state == "Scanning" || state == "Loading ROM") {
            color = COLOR_GOLD;
        } else if (state.find("No ") == 0 || state.find("failed") != std::string::npos ||
                   state.find("Failed") != std::string::npos ||
                   state.find("Unsupported") != std::string::npos ||
                   state.find("Bad ") == 0 || state == "Open failed" ||
                   state == "ROM too large" || state == "FC unavailable" ||
                   state == "SD card not ready") {
            color = lv_color_make(0xff, 0x88, 0x68);
        } else if (state.empty()) {
            state = "SD ROMs";
        }
        fc_app_status_ = clean_subtitle_text(state.c_str(), 18);
        if (fc_app_status_.empty()) {
            fc_app_status_ = "SD ROMs";
        }
        fc_app_color_ = color;
        SetAppTileStatus(3, fc_app_status_.c_str(), fc_app_color_);
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
    if (playing) {
        fc_app_status_ = "Playing";
        fc_app_color_ = COLOR_GREEN;
    } else if (fc_app_status_ == "Playing") {
        fc_app_status_ = "Ready";
        fc_app_color_ = COLOR_GREEN;
    }
    SetAppTileStatus(3, fc_app_status_.c_str(),
                     fc_app_status_ == "SD ROMs" ? COLOR_GREEN : fc_app_color_);
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
    current_weather_code_ = weather_code;
    const bool is_clear = weather_code == 0;
    const bool is_fog = weather_code == 45 || weather_code == 48;
    const bool is_cloud = weather_code == 1 || weather_code == 2 || weather_code == 3 ||
                          is_fog || weather_code < 0;
    const bool is_rain = (weather_code >= 51 && weather_code <= 67) ||
                         (weather_code >= 80 && weather_code <= 82);
    const bool is_snow = weather_code >= 71 && weather_code <= 77;
    const bool is_storm = weather_code >= 95;
    const bool use_weather_scene = weather_scene_gif_ != nullptr;

    int next_scene = 1;  // Default to cloudy while weather is pending.
    const lv_image_dsc_t* scene_src = &qd_weather_cloudy_scene;
    if (is_clear) {
        next_scene = 0;
        scene_src = &qd_weather_clear_scene;
    } else if (is_storm) {
        next_scene = 5;
        scene_src = &qd_weather_storm_scene;
    } else if (is_snow) {
        next_scene = 3;
        scene_src = &qd_weather_snow_scene;
    } else if (is_rain) {
        next_scene = 2;
        scene_src = &qd_weather_rain_scene;
    } else if (is_fog) {
        next_scene = 4;
        scene_src = &qd_weather_fog_scene;
    }

    if (weather_scene_gif_ && current_weather_scene_ != next_scene) {
        lv_gif_set_src(weather_scene_gif_, scene_src);
        lv_gif_restart(weather_scene_gif_);
        current_weather_scene_ = next_scene;
    }
    set_weather_part_visible(weather_scene_gif_, use_weather_scene);

    const bool show_sun = is_clear || weather_code == 1 || weather_code == 2;
    set_weather_part_visible(weather_glow_, show_sun && !use_weather_scene);
    set_weather_part_visible(weather_sun_, show_sun && !use_weather_scene);
    for (auto* ray : weather_rays_) {
        set_weather_part_visible(ray, show_sun && !is_storm && !use_weather_scene);
    }
    if (weather_sun_) {
        lv_obj_set_style_opa(weather_sun_, is_clear ? LV_OPA_COVER : LV_OPA_50, 0);
    }
    if (weather_glow_) {
        lv_obj_set_style_opa(weather_glow_, is_clear ? LV_OPA_30 : LV_OPA_20, 0);
    }

    const bool show_cloud = is_cloud || is_rain || is_snow || is_storm;
    set_weather_part_visible(weather_cloud_shadow_, show_cloud && !use_weather_scene);
    for (auto* cloud : weather_cloud_) {
        set_weather_part_visible(cloud, show_cloud && !use_weather_scene);
        if (cloud) {
            lv_obj_set_style_opa(cloud, is_storm ? LV_OPA_80 : LV_OPA_COVER, 0);
        }
    }

    for (auto* rain : weather_rain_) {
        set_weather_part_visible(rain, (is_rain || is_storm) && !use_weather_scene);
    }
    for (auto* snow : weather_snow_) {
        set_weather_part_visible(snow, is_snow && !use_weather_scene);
    }
    for (auto* storm : weather_storm_) {
        set_weather_part_visible(storm, is_storm && !use_weather_scene);
    }
    
    // Start/stop particle animation based on weather
    if (weather_particle_timer_) {
        if (use_weather_scene) {
            lv_timer_pause(weather_particle_timer_);
        } else {
            lv_timer_resume(weather_particle_timer_);
        }
    }

    ESP_LOGI(TAG, "Weather visual code=%d clear=%d cloud=%d rain=%d snow=%d fog=%d storm=%d scene=%d",
             weather_code, is_clear, show_cloud, is_rain, is_snow, is_fog, is_storm, next_scene);
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

void DesktopUI::SetBluetoothConfigStatus(const char* status) {
    settings_ble_status_ = status ? status : "BLE idle";
    if (settings_ble_status_label_) {
        lv_label_set_text(settings_ble_status_label_, settings_ble_status_.c_str());
    }
}

void DesktopUI::SetWifiConfigStatus(const char* status) {
    settings_wifi_config_status_ = status ? status : "WiFi config idle";
    if (settings_wifi_config_status_label_) {
        lv_label_set_text(settings_wifi_config_status_label_, settings_wifi_config_status_.c_str());
    }
}

void DesktopUI::RegisterBrandLabels(lv_obj_t* logo, lv_obj_t* owner) {
    if ((!logo && !owner) || brand_label_count_ >= sizeof(brand_logo_labels_) / sizeof(brand_logo_labels_[0])) {
        return;
    }
    brand_logo_labels_[brand_label_count_] = logo;
    brand_owner_labels_[brand_label_count_] = owner;
    ++brand_label_count_;
}

void DesktopUI::RefreshBrandLabels() {
    const auto profile = QdLoadUserProfile();
    for (size_t i = 0; i < brand_label_count_; ++i) {
        if (brand_logo_labels_[i]) {
            lv_label_set_text(brand_logo_labels_[i], profile.logo.c_str());
        }
        if (brand_owner_labels_[i]) {
            lv_label_set_text(brand_owner_labels_[i], profile.owner.c_str());
        }
    }
}

void DesktopUI::ReloadUserProfile() {
    const DesktopPage page = current_page_;
    lv_obj_t* root = lv_scr_act();
    lv_obj_clean(root);

    main_page_ = nullptr;
    apps_page_ = nullptr;
    photo_page_ = nullptr;
    fc_page_ = nullptr;
    calendar_page_ = nullptr;
    radio_page_ = nullptr;
    music_page_ = nullptr;
    music_title_label_ = nullptr;
    music_artist_label_ = nullptr;
    music_line_label_ = nullptr;
    music_side_lyric_label_ = nullptr;
    music_cover_disc_ = nullptr;
    music_cover_note_ = nullptr;
    memset(music_cover_bars_, 0, sizeof(music_cover_bars_));
    music_cover_timer_ = nullptr;
    music_hint_label_ = nullptr;
    music_recent_clear_button_ = nullptr;
    memset(music_recent_buttons_, 0, sizeof(music_recent_buttons_));
    memset(music_recent_labels_, 0, sizeof(music_recent_labels_));
    focus_page_ = nullptr;
    hourglass_page_ = nullptr;
    hourglass_portrait_ = nullptr;
    hourglass_time_label_ = nullptr;
    hourglass_status_label_ = nullptr;
    hourglass_top_sand_ = nullptr;
    hourglass_bottom_sand_ = nullptr;
    memset(hourglass_top_sand_strips_, 0, sizeof(hourglass_top_sand_strips_));
    memset(hourglass_bottom_sand_strips_, 0, sizeof(hourglass_bottom_sand_strips_));
    memset(hourglass_falling_dots_, 0, sizeof(hourglass_falling_dots_));
    memset(hourglass_preset_buttons_, 0, sizeof(hourglass_preset_buttons_));
    memset(hourglass_preset_labels_, 0, sizeof(hourglass_preset_labels_));
    xiaozhi_page_ = nullptr;
    music_lyric_panel_ = nullptr;
    music_lyric_label_ = nullptr;
    qr_overlay_ = nullptr;
    qr_title_label_ = nullptr;
    qr_hint_label_ = nullptr;
    qr_code_ = nullptr;
    network_page_ = nullptr;
    settings_page_ = nullptr;
    diagnostics_page_ = nullptr;
    memset(diagnostics_labels_, 0, sizeof(diagnostics_labels_));

    weather_particle_timer_ = nullptr;
    radio_anim_timer_ = nullptr;
    focus_timer_ = nullptr;
    hourglass_tick_timer_ = nullptr;
    hourglass_anim_timer_ = nullptr;

    Create();
    ShowPage(page);
    RefreshSettingsControls();
}

void DesktopUI::CycleTheme() {
    const uint8_t count = sizeof(THEMES) / sizeof(THEMES[0]);
    uint8_t next = static_cast<uint8_t>(current_theme) + 1;
    if (next >= count) {
        next = 0;
    }
    save_theme(static_cast<UiTheme>(next));
    ESP_LOGI(TAG, "Theme switched to %s, recreating UI", THEMES[next].name);

    lv_obj_t* root = lv_scr_act();
    lv_obj_clean(root);
    
    main_page_ = nullptr;
    apps_page_ = nullptr;
    photo_page_ = nullptr;
    fc_page_ = nullptr;
    calendar_page_ = nullptr;
    radio_page_ = nullptr;
    music_page_ = nullptr;
    music_title_label_ = nullptr;
    music_artist_label_ = nullptr;
    music_line_label_ = nullptr;
    music_side_lyric_label_ = nullptr;
    music_cover_disc_ = nullptr;
    music_cover_note_ = nullptr;
    memset(music_cover_bars_, 0, sizeof(music_cover_bars_));
    music_cover_timer_ = nullptr;
    music_hint_label_ = nullptr;
    music_recent_clear_button_ = nullptr;
    memset(music_recent_buttons_, 0, sizeof(music_recent_buttons_));
    memset(music_recent_labels_, 0, sizeof(music_recent_labels_));
    focus_page_ = nullptr;
    hourglass_page_ = nullptr;
    hourglass_portrait_ = nullptr;
    hourglass_time_label_ = nullptr;
    hourglass_status_label_ = nullptr;
    hourglass_top_sand_ = nullptr;
    hourglass_bottom_sand_ = nullptr;
    memset(hourglass_top_sand_strips_, 0, sizeof(hourglass_top_sand_strips_));
    memset(hourglass_bottom_sand_strips_, 0, sizeof(hourglass_bottom_sand_strips_));
    memset(hourglass_falling_dots_, 0, sizeof(hourglass_falling_dots_));
    memset(hourglass_preset_buttons_, 0, sizeof(hourglass_preset_buttons_));
    memset(hourglass_preset_labels_, 0, sizeof(hourglass_preset_labels_));
    xiaozhi_page_ = nullptr;
    music_lyric_panel_ = nullptr;
    music_lyric_label_ = nullptr;
    qr_overlay_ = nullptr;
    qr_title_label_ = nullptr;
    qr_hint_label_ = nullptr;
    qr_code_ = nullptr;
    network_page_ = nullptr;
    settings_page_ = nullptr;
    diagnostics_page_ = nullptr;
    memset(diagnostics_labels_, 0, sizeof(diagnostics_labels_));
    
    weather_particle_timer_ = nullptr;
    radio_anim_timer_ = nullptr;
    focus_timer_ = nullptr;
    hourglass_tick_timer_ = nullptr;
    hourglass_anim_timer_ = nullptr;
    
    Create();
    ShowPage(DesktopPage::SETTINGS);
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

    if (settings_theme_value_) {
        lv_label_set_text(settings_theme_value_, theme().name);
    }

    if (settings_profile_logo_value_ || settings_profile_owner_value_ || brand_label_count_ > 0) {
        const auto profile = QdLoadUserProfile();
        RefreshBrandLabels();
        if (settings_profile_logo_value_) {
            lv_label_set_text(settings_profile_logo_value_, profile.logo.c_str());
        }
        if (settings_profile_owner_value_) {
            lv_label_set_text(settings_profile_owner_value_, profile.owner.c_str());
        }
    }

    if (settings_weather_value_) {
        const auto weather = QdLoadWeatherConfig();
        char text[96];
        snprintf(text, sizeof(text), "%s (%s, %s)",
                 weather.city.c_str(), weather.latitude.c_str(), weather.longitude.c_str());
        lv_label_set_text(settings_weather_value_, text);
    }

    if (settings_wifi_config_status_label_) {
        lv_label_set_text(settings_wifi_config_status_label_, settings_wifi_config_status_.c_str());
    }
    if (settings_ble_status_label_) {
        lv_label_set_text(settings_ble_status_label_, settings_ble_status_.c_str());
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
        focus_is_work_ = !focus_is_work_;
        focus_total_sec_ = focus_is_work_ ? 25 * 60 : 5 * 60;
        focus_remaining_sec_ = focus_total_sec_;
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

void DesktopUI::ResetHourglassToDefault() {
    hourglass_selected_index_ = 2;
    hourglass_total_sec_ = 15 * 60;
    hourglass_remaining_sec_ = hourglass_total_sec_;
    hourglass_running_ = false;
    hourglass_done_ = false;
    hourglass_alarm_played_ = false;
    hourglass_anim_tick_ = 0;
    if (hourglass_tick_timer_) {
        lv_timer_pause(hourglass_tick_timer_);
    }
    if (hourglass_anim_timer_) {
        lv_timer_pause(hourglass_anim_timer_);
    }
    UpdateHourglassUI();
}

void DesktopUI::EnterHourglassMode() {
    if (current_page_ != DesktopPage::HOURGLASS) {
        hourglass_return_page_ = current_page_;
    }
    if (hourglass_return_page_ == DesktopPage::HOURGLASS) {
        hourglass_return_page_ = DesktopPage::MAIN;
    }
    hourglass_motion_active_ = true;
    ResetHourglassToDefault();
    ShowPage(DesktopPage::HOURGLASS);
}

void DesktopUI::ExitHourglassMode() {
    if (current_page_ != DesktopPage::HOURGLASS && !hourglass_motion_active_) {
        return;
    }
    hourglass_motion_active_ = false;
    hourglass_running_ = false;
    hourglass_done_ = false;
    hourglass_alarm_played_ = false;
    if (hourglass_tick_timer_) {
        lv_timer_delete(hourglass_tick_timer_);
        hourglass_tick_timer_ = nullptr;
    }
    if (hourglass_anim_timer_) {
        lv_timer_delete(hourglass_anim_timer_);
        hourglass_anim_timer_ = nullptr;
    }
    const DesktopPage target = hourglass_return_page_ == DesktopPage::HOURGLASS
        ? DesktopPage::MAIN
        : hourglass_return_page_;
    ShowPage(target);
    if (hourglass_page_) {
        // Paint the destination page first, then reclaim this large object tree.
        lv_obj_delete_async(hourglass_page_);
        hourglass_page_ = nullptr;
        hourglass_portrait_ = nullptr;
        hourglass_time_label_ = nullptr;
        hourglass_status_label_ = nullptr;
        hourglass_top_sand_ = nullptr;
        hourglass_bottom_sand_ = nullptr;
        memset(hourglass_top_sand_strips_, 0, sizeof(hourglass_top_sand_strips_));
        memset(hourglass_bottom_sand_strips_, 0, sizeof(hourglass_bottom_sand_strips_));
        memset(hourglass_falling_dots_, 0, sizeof(hourglass_falling_dots_));
        memset(hourglass_preset_buttons_, 0, sizeof(hourglass_preset_buttons_));
        memset(hourglass_preset_labels_, 0, sizeof(hourglass_preset_labels_));
        ESP_LOGI(TAG, "Hourglass page released");
    }
}

void DesktopUI::SelectHourglassPreset(uint8_t index) {
    static constexpr uint32_t kDurationsSec[4] = {5 * 60, 10 * 60, 15 * 60, 20 * 60};
    if (index >= 4) {
        return;
    }
    if (index == hourglass_selected_index_ && !hourglass_done_ && hourglass_remaining_sec_ > 0 &&
        hourglass_remaining_sec_ < hourglass_total_sec_) {
        hourglass_running_ = !hourglass_running_;
    } else {
        hourglass_selected_index_ = index;
        hourglass_total_sec_ = kDurationsSec[index];
        hourglass_remaining_sec_ = hourglass_total_sec_;
        hourglass_done_ = false;
        hourglass_alarm_played_ = false;
        hourglass_running_ = true;
    }
    if (hourglass_tick_timer_) {
        if (hourglass_running_) {
            lv_timer_resume(hourglass_tick_timer_);
        } else {
            lv_timer_pause(hourglass_tick_timer_);
        }
    }
    if (hourglass_anim_timer_) {
        if (hourglass_running_) {
            lv_timer_resume(hourglass_anim_timer_);
        } else {
            lv_timer_pause(hourglass_anim_timer_);
        }
    }
    ESP_LOGI(TAG, "Hourglass preset=%u running=%d remaining=%lu",
             static_cast<unsigned>(index), hourglass_running_,
             static_cast<unsigned long>(hourglass_remaining_sec_));
    UpdateHourglassUI();
}

bool DesktopUI::HandleHourglassTap(uint16_t x, uint16_t y) {
    const int16_t portrait_x = static_cast<int16_t>(y);
    const int16_t portrait_y = static_cast<int16_t>(DISPLAY_WIDTH) - static_cast<int16_t>(x);
    ESP_LOGI(TAG, "Hourglass tap raw=(%u,%u) portrait=(%d,%d)",
             static_cast<unsigned>(x), static_cast<unsigned>(y), portrait_x, portrait_y);
    if (portrait_y < 438 || portrait_y > 476) {
        return false;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t left = 12 + i * 78;
        if (portrait_x >= left - 6 && portrait_x < left + 70) {
            SelectHourglassPreset(i);
            return true;
        }
    }
    return false;
}

void DesktopUI::UpdateHourglassButtons() {
    const lv_color_t brown = LV_COLOR_MAKE(0x4b, 0x2d, 0x16);
    const lv_color_t pink = LV_COLOR_MAKE(0xff, 0x9c, 0xa0);
    const lv_color_t cream = LV_COLOR_MAKE(0xff, 0xf7, 0xe8);
    const lv_color_t white = LV_COLOR_MAKE(0xff, 0xff, 0xff);
    for (uint8_t i = 0; i < 4; ++i) {
        if (!hourglass_preset_buttons_[i]) {
            continue;
        }
        const bool selected = i == hourglass_selected_index_;
        lv_obj_set_style_bg_color(hourglass_preset_buttons_[i],
                                  selected ? pink : cream, 0);
        lv_obj_set_style_border_width(hourglass_preset_buttons_[i], selected ? 3 : 2, 0);
        lv_obj_set_style_border_color(hourglass_preset_buttons_[i], brown, 0);
        if (hourglass_preset_labels_[i]) {
            lv_obj_set_style_text_color(hourglass_preset_labels_[i],
                                        selected ? white : brown, 0);
        }
    }
}

void DesktopUI::UpdateHourglassUI() {
    if (!hourglass_time_label_) {
        return;
    }
    char time_text[16];
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu",
             static_cast<unsigned long>(hourglass_remaining_sec_ / 60),
             static_cast<unsigned long>(hourglass_remaining_sec_ % 60));
    lv_label_set_text(hourglass_time_label_, time_text);
    if (hourglass_status_label_) {
        const bool paused_midway = !hourglass_running_ && hourglass_remaining_sec_ < hourglass_total_sec_;
        lv_label_set_text(hourglass_status_label_,
                          hourglass_done_ ? "时间到" : (paused_midway ? "暂停中" : "倒计时中"));
    }

    const uint32_t elapsed = hourglass_total_sec_ > hourglass_remaining_sec_
        ? hourglass_total_sec_ - hourglass_remaining_sec_
        : 0;
    const uint32_t progress = hourglass_total_sec_ == 0 ? 0 : (elapsed * 1000) / hourglass_total_sec_;
    const int top_visible = hourglass_done_ ? 0 : std::max(1, 16 - static_cast<int>(progress * 15 / 1000));
    const int bottom_visible = std::min(22, 4 + static_cast<int>(progress * 18 / 1000));
    for (int i = 0; i < 16; ++i) {
        lv_obj_t* strip = hourglass_top_sand_strips_[i];
        if (!strip) {
            continue;
        }
        const bool visible = i >= (16 - top_visible);
        const lv_opa_t opa = visible ? static_cast<lv_opa_t>(LV_OPA_70 + (i % 3) * 10)
                                     : static_cast<lv_opa_t>(LV_OPA_TRANSP);
        lv_obj_set_style_bg_opa(strip, opa, 0);
    }
    for (int i = 0; i < 22; ++i) {
        lv_obj_t* strip = hourglass_bottom_sand_strips_[i];
        if (!strip) {
            continue;
        }
        const bool visible = i < bottom_visible;
        const lv_opa_t opa = visible ? static_cast<lv_opa_t>(LV_OPA_70 + (i % 4) * 6)
                                     : static_cast<lv_opa_t>(LV_OPA_TRANSP);
        lv_obj_set_style_bg_opa(strip, opa, 0);
    }
    for (auto* dot : hourglass_falling_dots_) {
        if (dot) {
            lv_obj_set_style_bg_opa(dot, hourglass_running_ ? LV_OPA_70 : LV_OPA_TRANSP, 0);
            lv_obj_set_style_shadow_opa(dot, hourglass_running_ ? LV_OPA_20 : LV_OPA_TRANSP, 0);
        }
    }
    UpdateHourglassButtons();
}

void DesktopUI::StartFocusTimer(bool rotate_180) {
    if (rotate_180) {
        if (focus_rotation_callback_) {
            focus_rotation_callback_(true);
        }
        focus_auto_rotated_ = true;
        ESP_LOGI(TAG, "Focus display rotated 180 by motion");
    }
    if (focus_remaining_sec_ == 0) {
        focus_is_work_ = true;
        focus_total_sec_ = 25 * 60;
        focus_remaining_sec_ = focus_total_sec_;
    }
    if (focus_is_work_ && !focus_running_) {
        focus_running_ = true;
        if (focus_timer_) {
            lv_timer_resume(focus_timer_);
        }
        ESP_LOGI(TAG, "Focus timer start by motion remaining=%lu",
                 static_cast<unsigned long>(focus_remaining_sec_));
    }
    ShowPage(DesktopPage::FOCUS);
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

uint32_t DesktopUI::CurrentFocusDateKey() const {
    if (current_year_ <= 0 || current_month_ <= 0 || current_day_ <= 0) {
        return 0;
    }
    return static_cast<uint32_t>(current_year_ * 10000 + current_month_ * 100 + current_day_);
}

void DesktopUI::ReconcileFocusDate(bool persist) {
    const uint32_t today = CurrentFocusDateKey();
    if (today == 0) {
        return;
    }
    if (focus_count_date_ == today) {
        return;
    }
    if (focus_count_date_ == 0) {
        focus_count_date_ = today;
        if (persist) {
            SaveFocusStats(focus_completed_count_, focus_count_date_);
        }
        ESP_LOGI(TAG, "Focus count date initialized date=%lu count=%u",
                 static_cast<unsigned long>(focus_count_date_), focus_completed_count_);
        return;
    }
    focus_count_date_ = today;
    focus_completed_count_ = 0;
    if (persist) {
        SaveFocusStats(focus_completed_count_, focus_count_date_);
    }
    ESP_LOGI(TAG, "Focus count reset for new day date=%lu",
             static_cast<unsigned long>(focus_count_date_));
    UpdateFocusUI();
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
        const char* start_text = focus_running_
            ? "暂停"
            : (focus_remaining_sec_ == 0 ? (focus_is_work_ ? "休息" : "专注") : "开始");
        lv_label_set_text(focus_start_label_, start_text);
    }
    if (focus_completed_label_) {
        char done_text[32];
        snprintf(done_text, sizeof(done_text), "%u 个番茄", focus_completed_count_);
        lv_label_set_text(focus_completed_label_, done_text);
    }

    char app_status[16];
    lv_color_t app_color = focus_is_work_ ? COLOR_GOLD : COLOR_BLUE;
    if (focus_remaining_sec_ == 0) {
        snprintf(app_status, sizeof(app_status), "Done");
        app_color = COLOR_GREEN;
    } else if (focus_running_) {
        snprintf(app_status, sizeof(app_status), "%lum",
                 static_cast<unsigned long>((focus_remaining_sec_ + 59) / 60));
    } else if (focus_remaining_sec_ != focus_total_sec_) {
        snprintf(app_status, sizeof(app_status), "Paused");
        app_color = COLOR_MUTED;
    } else {
        snprintf(app_status, sizeof(app_status), "%lu min",
                 static_cast<unsigned long>(focus_total_sec_ / 60));
    }
    SetAppTileStatus(5, app_status, app_color);
}

void DesktopUI::SetAppTileStatus(uint8_t index, const char* status, lv_color_t color) {
    if (index >= sizeof(app_status_labels_) / sizeof(app_status_labels_[0])) {
        return;
    }
    if (app_status_labels_[index] && status) {
        lv_label_set_text(app_status_labels_[index], status);
    }
    if (app_status_dots_[index]) {
        lv_obj_set_style_bg_color(app_status_dots_[index], color, 0);
    }
}

void DesktopUI::RefreshAppTileStatuses() {
    SetAppTileStatus(0, radio_playing_ ? "Playing" : "Music FM",
                     radio_playing_ ? COLOR_GREEN : COLOR_GOLD);
    if (current_year_ > 0 && current_month_ > 0 && current_day_ > 0) {
        char calendar_status[40];
        snprintf(calendar_status, sizeof(calendar_status), "%04d/%02d/%02d",
                 current_year_, current_month_, current_day_);
        SetAppTileStatus(4, calendar_status, is_tupi_warm_theme() ? COLOR_GOLD : COLOR_PURPLE);
    } else {
        SetAppTileStatus(4, "Today", is_tupi_warm_theme() ? COLOR_GOLD : COLOR_PURPLE);
    }
    SetAppTileStatus(1, photo_app_status_.c_str(),
                     photo_app_status_ == "SD Slideshow" ? COLOR_GREEN : photo_app_color_);
    SetAppTileStatus(3, fc_app_status_.c_str(),
                     fc_app_status_ == "SD ROMs" ? COLOR_GREEN : fc_app_color_);
    UpdateFocusUI();
    SetAppTileStatus(6, network_app_status_.c_str(),
                     network_app_status_ == "WiFi Hub"
                         ? (is_tupi_warm_theme() ? COLOR_GREEN : COLOR_BLUE)
                         : network_app_color_);
    if (firmware_update_busy_) {
        char progress_text[12];
        if (firmware_update_progress_ >= 0) {
            snprintf(progress_text, sizeof(progress_text), "%d%%",
                     std::max(0, std::min(100, firmware_update_progress_)));
            SetAppTileStatus(7, progress_text, COLOR_GOLD);
        } else {
            SetAppTileStatus(7, "Wait", COLOR_GOLD);
        }
    } else if (firmware_update_available_) {
        SetAppTileStatus(7, "Update", COLOR_GOLD);
    } else {
        SetAppTileStatus(7, "System", COLOR_GREEN);
    }
    SetAppTileStatus(8, music_title_ == "No song yet" ? "Ask song" : music_title_.c_str(),
                     music_title_ == "No song yet" ? (is_tupi_warm_theme() ? COLOR_GREEN : COLOR_PURPLE) : COLOR_GREEN);
    SetAppTileStatus(9, podcast_app_status_.c_str(),
                     podcast_app_status_ == "Episodes" ? COLOR_GOLD : podcast_app_color_);
}

void DesktopUI::SetNetworkStatus(const char* status) {
    if (!status) return;
    if (network_status_label_) {
        lv_label_set_text(network_status_label_, status);
    }
    if (network_detail_label_) {
        lv_label_set_text(network_detail_label_, status);
    }

    const bool offline = strstr(status, "disconnect") || strstr(status, "Disconnect") ||
                         strstr(status, "Offline") || strstr(status, "offline") ||
                         strstr(status, "failed") || strstr(status, "Failed");
    const bool online = strstr(status, "Ready") || strstr(status, "ready") ||
                        strstr(status, "Connected") || strstr(status, "connected") ||
                        strstr(status, "IP") || strstr(status, "Online") ||
                        strstr(status, "online");
    auto& wifi = WifiStation::GetInstance();
    const std::string ip = wifi.IsConnected() ? wifi.GetIpAddress() : "";
    if (online && !offline && !ip.empty()) {
        network_app_status_ = ip;
        network_app_color_ = COLOR_GREEN;
    } else if (online && !offline) {
        network_app_status_ = "Online";
        network_app_color_ = COLOR_GREEN;
    } else if (offline) {
        network_app_status_ = "Offline";
        network_app_color_ = COLOR_MUTED;
    } else {
        network_app_status_ = "WiFi";
        network_app_color_ = COLOR_GOLD;
    }
    SetAppTileStatus(6, network_app_status_.c_str(), network_app_color_);
}

void DesktopUI::SetBatteryStatus(int level, bool charging, bool valid) {
    const int next_level = valid ? std::max(0, std::min(100, level)) : -1;
    if (battery_level_ == next_level && battery_charging_ == charging) {
        return;
    }
    battery_level_ = next_level;
    battery_charging_ = charging;

    char text[16];
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
void DesktopUI::SetFirmwareUpdateStatus(const char* status, bool update_available, bool busy, int progress,
                                        size_t asset_size, size_t partition_size) {
    const bool usb_required = status && strstr(status, "USB");
    if (status) {
        firmware_update_status_ = status;
    }
    firmware_update_available_ = update_available;
    firmware_update_busy_ = busy;
    firmware_update_progress_ = busy ? progress : -1;
    firmware_update_asset_size_ = asset_size;
    firmware_update_partition_size_ = partition_size;

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
                                      busy ? COLOR_MUTED : (update_available || usb_required ? COLOR_GOLD : COLOR_GREEN),
                                      0);
    }

    if (settings_firmware_button_label_) {
        char progress_text[12];
        const char* text = nullptr;
        if (busy && progress >= 0) {
            snprintf(progress_text, sizeof(progress_text), "%d%%", std::max(0, std::min(100, progress)));
            text = progress_text;
        } else {
            text = busy ? "Wait" : (update_available ? "Update" : (usb_required ? "USB" : "Check"));
        }
        lv_label_set_text(settings_firmware_button_label_, text);
        lv_obj_set_style_text_color(settings_firmware_button_label_,
                                    (update_available || usb_required) && !busy ? COLOR_GOLD : COLOR_TEXT, 0);
        lv_obj_center(settings_firmware_button_label_);
    }

    if (busy) {
        char progress_text[12];
        if (progress >= 0) {
            snprintf(progress_text, sizeof(progress_text), "%d%%", std::max(0, std::min(100, progress)));
            SetAppTileStatus(7, progress_text, COLOR_GOLD);
        } else {
            SetAppTileStatus(7, "Wait", COLOR_GOLD);
        }
    } else if (update_available) {
        SetAppTileStatus(7, "Update", COLOR_GOLD);
    } else if (usb_required) {
        SetAppTileStatus(7, "USB", COLOR_GOLD);
    } else {
        SetAppTileStatus(7, status && strstr(status, "Latest") ? "Latest" : "Check", COLOR_GREEN);
    }
}

void DesktopUI::SetRadioActions(std::function<void()> play_pause, std::function<void()> stop,
                                std::function<void()> next, std::function<void()> prev) {
    radio_play_pause_ = std::move(play_pause);
    radio_stop_ = std::move(stop);
    radio_next_ = std::move(next);
    radio_prev_ = std::move(prev);
}

void DesktopUI::SetMusicActions(std::function<void()> play, std::function<void()> pause,
                                std::function<void()> next) {
    music_play_ = std::move(play);
    music_pause_ = std::move(pause);
    music_next_ = std::move(next);
}

void DesktopUI::SetMusicReplayCallback(std::function<void(const std::string& title,
                                                          const std::string& artist,
                                                          const std::string& url,
                                                           const std::string& lyrics_json)> callback) {
    music_replay_cb_ = std::move(callback);
}

bool DesktopUI::TryAcceptMusicControlTap() {
    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - music_control_last_ms_ < kMusicControlDebounceMs) {
        return false;
    }
    music_control_last_ms_ = now_ms;
    return true;
}

void DesktopUI::StartMusicAsk() {
    music_line_ = "Listening... say a song name.";
    if (music_line_label_) {
        lv_label_set_text(music_line_label_, "Listening");
    }
    if (music_side_lyric_label_) {
        lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
    }
    SetXiaozhiState("Music", "Tell me a song name.", "thinking");

    auto& app = Application::GetInstance();
    app.PrepareVoiceInteraction();
    if (app.GetDeviceState() == kDeviceStateIdle) {
        app.ToggleChatState();
    } else if (app.GetDeviceState() == kDeviceStateSpeaking) {
        app.ToggleChatState();
    }
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
    if (state) {
        if (music_recent_pending_index_ < kMusicRecentCount) {
            const bool same_station = station && music_recent_pending_title_.size() > 0 &&
                                      std::string(station).find(music_recent_pending_title_) != std::string::npos;
            if (same_station && (strcmp(state, "Playing") == 0 || strcmp(state, "Buffering") == 0)) {
                music_recent_pending_index_ = kMusicRecentCount;
                music_recent_pending_title_.clear();
                music_recent_failed_index_ = kMusicRecentCount;
                music_recent_failed_reason_.clear();
                RefreshMusicRecent();
            } else if (same_station && strcmp(state, "Error") == 0) {
                const std::string failure = clean_subtitle_text(meta && meta[0] ? meta : "Replay failed", 24);
                music_recent_failed_index_ = music_recent_pending_index_;
                music_recent_failed_reason_ = failure.empty() ? "Replay failed" : failure;
                music_recent_pending_index_ = kMusicRecentCount;
                music_recent_pending_title_.clear();
                RefreshMusicRecent();
                music_line_ = meta && meta[0] ? meta : "Replay failed. Ask XiaoZhi for a fresh URL.";
                if (music_line_label_) {
                    lv_label_set_text(music_line_label_, "Error");
                }
                SetAppTileStatus(8, "Failed", lv_color_make(0xff, 0x88, 0x68));
            }
        }

        lv_color_t color = COLOR_MUTED;
        const char* app_status = "Stopped";
        if (strcmp(state, "Playing") == 0) {
            color = COLOR_GREEN;
            app_status = "Playing";
        } else if (strcmp(state, "Buffering") == 0 || strcmp(state, "Connecting") == 0) {
            color = COLOR_GOLD;
            app_status = strcmp(state, "Buffering") == 0 ? "Buffer" : "Connect";
        }
        SetAppTileStatus(0, app_status, color);
    }
}

void DesktopUI::SetPodcastActions(std::function<void()> play_pause, std::function<void()> stop,
                                  std::function<void()> next, std::function<void()> prev,
                                  std::function<void()> up, std::function<void()> down,
                                  std::function<void(int)> seek) {
    podcast_play_pause_ = std::move(play_pause);
    podcast_stop_ = std::move(stop);
    podcast_next_ = std::move(next);
    podcast_prev_ = std::move(prev);
    podcast_up_ = std::move(up);
    podcast_down_ = std::move(down);
    podcast_seek_ = std::move(seek);
}

void DesktopUI::SetPodcastState(const char* title, const char* state, const char* meta,
                                const char* summary, const char* list) {
    if (podcast_title_label_) {
        lv_label_set_text(podcast_title_label_, title ? title : "Nothing Impossible");
    }
    if (podcast_state_label_) {
        lv_label_set_text(podcast_state_label_, state ? state : "Ready");
    }
    if (podcast_meta_label_) {
        lv_label_set_text(podcast_meta_label_, meta ? meta : "");
    }
    if (podcast_summary_label_) {
        lv_label_set_text(podcast_summary_label_, summary ? summary : "");
    }
    if (podcast_list_label_) {
        lv_label_set_text(podcast_list_label_, list ? list : "");
    }
    if (state) {
        lv_color_t color = COLOR_GOLD;
        const char* app_status = state;
        if (strcmp(state, "Playing") == 0 || strcmp(state, "Buffering") == 0) {
            color = COLOR_GREEN;
        } else if (strcmp(state, "Ready") == 0 || strcmp(state, "Stopped") == 0) {
            color = COLOR_MUTED;
        }
        podcast_app_status_ = app_status;
        podcast_app_color_ = color;
        SetAppTileStatus(9, podcast_app_status_.c_str(), podcast_app_color_);
    }
}

void DesktopUI::ShowPodcastDetail(bool detail) {
    podcast_detail_view_ = detail;
    if (podcast_list_group_) {
        if (detail) {
            lv_obj_add_flag(podcast_list_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(podcast_list_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (podcast_detail_group_) {
        if (detail) {
            lv_obj_clear_flag(podcast_detail_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(podcast_detail_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void DesktopUI::SetPodcastCover(const lv_img_dsc_t* image) {
    if (!podcast_cover_image_ || !image) {
        return;
    }
    lv_image_set_src(podcast_cover_image_, image);
    int32_t scale_x = image->header.w > 0 ? (150 * 256) / image->header.w : 256;
    int32_t scale_y = image->header.h > 0 ? (150 * 256) / image->header.h : 256;
    int32_t scale = LV_MAX(scale_x, scale_y);
    if (scale <= 0) {
        scale = 256;
    }
    lv_image_set_scale(podcast_cover_image_, scale);
}

void DesktopUI::SetPodcastProgress(int percent) {
    percent = std::clamp(percent, 0, 100);
    if (podcast_progress_slider_ && !podcast_progress_dragging_) {
        lv_slider_set_value(podcast_progress_slider_, percent, LV_ANIM_OFF);
    }
    if (podcast_progress_label_ && !podcast_progress_dragging_) {
        char text[16];
        snprintf(text, sizeof(text), "%d%%", percent);
        lv_label_set_text(podcast_progress_label_, text);
    }
}

void DesktopUI::HandlePodcastSeekEvent(lv_event_t* event) {
    if (!event) {
        return;
    }
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        podcast_progress_dragging_ = true;
        return;
    }
    if (code == LV_EVENT_VALUE_CHANGED && podcast_progress_label_) {
        char text[16];
        snprintf(text, sizeof(text), "%ld%%", static_cast<long>(lv_slider_get_value(slider)));
        lv_label_set_text(podcast_progress_label_, text);
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        const int value = static_cast<int>(lv_slider_get_value(slider));
        podcast_progress_dragging_ = false;
        if (podcast_seek_) {
            podcast_seek_(value);
        }
    }
}

void DesktopUI::SetXiaozhiState(const char* state, const char* message, const char* emotion) {
    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (xiaozhi_state_label_) {
        if (is_themed_face_gif_theme()) {
            lv_label_set_text(xiaozhi_state_label_,
                              themed_face_state_text(Application::GetInstance().GetDeviceState()));
        } else {
            lv_label_set_text(xiaozhi_state_label_, state ? state : "Standby");
        }
    }
    if (xiaozhi_message_label_) {
        std::string clean_message = clean_subtitle_text(message);
        if (IsMusicLyricActive(now_ms)) {
            ESP_LOGI(TAG, "SetXiaozhiState skipped during music lyric hold state=%s message_len=%u",
                     state ? state : "", static_cast<unsigned>(clean_message.size()));
        } else {
            if (is_themed_face_gif_theme() && clean_message.empty()) {
                clean_message = clean_subtitle_text(state);
            }
            lv_label_set_text(xiaozhi_message_label_, clean_message.c_str());
        }
    }
    if (xiaozhi_hint_label_) {
        lv_label_set_text(xiaozhi_hint_label_, state ? state : "");
    }
    if (emotion) {
        emotion_ = emotion;
    }

}

void DesktopUI::LoadMusicRecent() {
    for (auto& item : music_recent_) {
        item = {};
    }

    nvs_handle_t handle;
    if (nvs_open("music_ui", NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    for (size_t i = 0; i < kMusicRecentCount; ++i) {
        char title_key[12];
        char artist_key[12];
        char url_key[12];
        snprintf(title_key, sizeof(title_key), "mt%u_t", static_cast<unsigned>(i));
        snprintf(artist_key, sizeof(artist_key), "mt%u_a", static_cast<unsigned>(i));
        snprintf(url_key, sizeof(url_key), "mt%u_u", static_cast<unsigned>(i));

        char title[72] = {};
        char artist[72] = {};
        char url[384] = {};
        size_t title_len = sizeof(title);
        size_t artist_len = sizeof(artist);
        size_t url_len = sizeof(url);
        nvs_get_str(handle, title_key, title, &title_len);
        nvs_get_str(handle, artist_key, artist, &artist_len);
        nvs_get_str(handle, url_key, url, &url_len);
        music_recent_[i].title = title;
        music_recent_[i].artist = artist;
        music_recent_[i].url = url;
    }
    nvs_close(handle);
}

void DesktopUI::SaveMusicRecent() {
    nvs_handle_t handle;
    if (nvs_open("music_ui", NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    for (size_t i = 0; i < kMusicRecentCount; ++i) {
        char title_key[12];
        char artist_key[12];
        char url_key[12];
        snprintf(title_key, sizeof(title_key), "mt%u_t", static_cast<unsigned>(i));
        snprintf(artist_key, sizeof(artist_key), "mt%u_a", static_cast<unsigned>(i));
        snprintf(url_key, sizeof(url_key), "mt%u_u", static_cast<unsigned>(i));
        nvs_set_str(handle, title_key, music_recent_[i].title.c_str());
        nvs_set_str(handle, artist_key, music_recent_[i].artist.c_str());
        nvs_set_str(handle, url_key, music_recent_[i].url.c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

void DesktopUI::RefreshMusicRecent() {
    for (size_t i = 0; i < kMusicRecentCount; ++i) {
        const bool has_track = !music_recent_[i].title.empty() && !music_recent_[i].url.empty();
        if (music_recent_labels_[i]) {
            const bool pending = has_track && i == music_recent_pending_index_;
            const bool failed = has_track && i == music_recent_failed_index_;
            std::string text = pending ? "Replaying..." :
                               (failed ? music_recent_failed_reason_ :
                                (has_track ? music_recent_[i].title : "No recent song"));
            if (has_track && !pending && !failed && !music_recent_[i].artist.empty()) {
                text += " - ";
                text += music_recent_[i].artist;
            }
            lv_label_set_text(music_recent_labels_[i], text.c_str());
            lv_obj_set_style_text_color(music_recent_labels_[i],
                                        failed ? lv_color_make(0xff, 0x88, 0x68) :
                                        (pending ? COLOR_GOLD : (has_track ? COLOR_TEXT : COLOR_MUTED)), 0);
        }
        if (music_recent_buttons_[i]) {
            if (has_track) {
                lv_obj_remove_state(music_recent_buttons_[i], LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(music_recent_buttons_[i], LV_STATE_DISABLED);
            }
            lv_obj_set_style_border_color(music_recent_buttons_[i],
                                          i == music_recent_failed_index_ ? lv_color_make(0xff, 0x88, 0x68) :
                                          i == music_recent_pending_index_ ? COLOR_GOLD :
                                          (i == 0 && has_track ? COLOR_GOLD : COLOR_LINE), 0);
        }
    }
}

void DesktopUI::ClearMusicRecent() {
    bool had_recent = false;
    for (auto& item : music_recent_) {
        had_recent = had_recent || !item.title.empty() || !item.artist.empty() || !item.url.empty();
        item = {};
    }
    music_recent_pending_index_ = kMusicRecentCount;
    music_recent_pending_title_.clear();
    music_recent_failed_index_ = kMusicRecentCount;
    music_recent_failed_reason_.clear();
    SaveMusicRecent();
    RefreshMusicRecent();
    music_line_ = had_recent ? "Recent songs cleared." : "No recent songs yet.";
    if (music_line_label_) {
        lv_label_set_text(music_line_label_, "Ready");
    }
    if (music_side_lyric_label_) {
        lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
    }
}

void DesktopUI::RemoveMusicRecent(size_t index) {
    if (index >= kMusicRecentCount || music_recent_[index].url.empty()) {
        return;
    }
    const std::string removed_title = music_recent_[index].title;
    for (size_t i = index; i + 1 < kMusicRecentCount; ++i) {
        music_recent_[i] = music_recent_[i + 1];
    }
    music_recent_[kMusicRecentCount - 1] = {};
    music_recent_pending_index_ = kMusicRecentCount;
    music_recent_pending_title_.clear();
    music_recent_failed_index_ = kMusicRecentCount;
    music_recent_failed_reason_.clear();
    SaveMusicRecent();
    RefreshMusicRecent();
    music_line_ = removed_title.empty() ? "Recent song removed." : "Removed from recent.";
    if (music_line_label_) {
        lv_label_set_text(music_line_label_, "Ready");
    }
    if (music_side_lyric_label_) {
        lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
    }
}

void DesktopUI::RememberMusicTrack(const char* title, const char* artist, const char* url, const char* lyrics_json) {
    if (!url || !url[0]) {
        return;
    }
    MusicRecentTrack track;
    track.title = clean_subtitle_text(title && title[0] ? title : "Music", 28);
    track.artist = clean_subtitle_text(artist, 18);
    track.url = std::string(url).substr(0, 360);
    if (lyrics_json && lyrics_json[0]) {
        track.lyrics_json = lyrics_json;
    }
    if (track.title.empty()) {
        track.title = "Music";
    }

    size_t existing = kMusicRecentCount;
    for (size_t i = 0; i < kMusicRecentCount; ++i) {
        if ((!music_recent_[i].url.empty() && music_recent_[i].url == track.url) ||
            (!music_recent_[i].title.empty() && music_recent_[i].title == track.title &&
             music_recent_[i].artist == track.artist)) {
            existing = i;
            break;
        }
    }
    const size_t stop = existing < kMusicRecentCount ? existing : kMusicRecentCount - 1;
    for (size_t i = stop; i > 0; --i) {
        music_recent_[i] = music_recent_[i - 1];
    }
    music_recent_[0] = std::move(track);
    music_recent_pending_index_ = kMusicRecentCount;
    music_recent_pending_title_.clear();
    music_recent_failed_index_ = kMusicRecentCount;
    music_recent_failed_reason_.clear();
    SaveMusicRecent();
    RefreshMusicRecent();
}

void DesktopUI::ReplayNextMusicRecent() {
    if (kMusicRecentCount < 2 || music_recent_[1].url.empty()) {
        music_line_ = "No next song cached. Ask XiaoZhi for a fresh song.";
        if (music_line_label_) {
            lv_label_set_text(music_line_label_, "Ready");
        }
        if (music_side_lyric_label_) {
            lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
        }
        SetAppTileStatus(8, "Ask XiaoZhi", COLOR_GOLD);
        return;
    }
    ReplayMusicRecent(1);
}

void DesktopUI::ReplayMusicRecent(size_t index) {
    if (index >= kMusicRecentCount || music_recent_[index].url.empty()) {
        music_line_ = "No recent songs yet.";
        if (music_line_label_) {
            lv_label_set_text(music_line_label_, "Ready");
        }
        if (music_side_lyric_label_) {
            lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
        }
        return;
    }
    const auto track = music_recent_[index];
    RememberMusicTrack(track.title.c_str(), track.artist.c_str(), track.url.c_str(), track.lyrics_json.c_str());
    music_recent_pending_index_ = 0;
    music_recent_pending_title_ = track.title;
    music_recent_failed_index_ = kMusicRecentCount;
    music_recent_failed_reason_.clear();
    music_title_ = track.title;
    music_artist_ = track.artist.empty() ? "Recent song" : track.artist;
    music_line_ = "Replaying recent song";
    if (music_title_label_) {
        lv_label_set_text(music_title_label_, music_title_.c_str());
    }
    if (music_artist_label_) {
        lv_label_set_text(music_artist_label_, music_artist_.c_str());
    }
    if (music_line_label_) {
        lv_label_set_text(music_line_label_, "Replaying");
    }
    if (music_side_lyric_label_) {
        lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
    }
    RefreshMusicRecent();
    SetAppTileStatus(8, music_title_.c_str(), COLOR_GREEN);
    if (music_replay_cb_) {
        music_replay_cb_(track.title, track.artist, track.url, track.lyrics_json);
    }
}

void DesktopUI::SetMusicLyric(const char* title, const char* artist, const char* line) {
    const int64_t now_ms = esp_timer_get_time() / 1000;
    music_lyric_hold_until_ms_ = now_ms + kMusicLyricHoldMs;

    std::string state = clean_subtitle_text(title && title[0] ? title : "Music", 20);
    std::string clean_artist = clean_subtitle_text(artist, 16);
    if (!clean_artist.empty()) {
        state += " - ";
        state += clean_artist;
    }
    std::string clean_line = clean_subtitle_text(line, 48);
    if (clean_line.empty()) {
        clean_line = state;
    }

    const std::string next_title = clean_subtitle_text(title && title[0] ? title : "Music", 32);
    const std::string next_artist = clean_artist.empty() ? "Music URL playback" : clean_artist;
    if (music_title_ == next_title && music_artist_ == next_artist && music_line_ == clean_line) {
        return;
    }

    music_title_ = next_title;
    music_artist_ = next_artist;
    music_line_ = clean_line;
    if (music_title_label_) {
        lv_label_set_text(music_title_label_, music_title_.c_str());
    }
    if (music_artist_label_) {
        lv_label_set_text(music_artist_label_, music_artist_.c_str());
    }
    if (music_line_label_) {
        lv_label_set_text(music_line_label_, "Playing");
    }
    if (music_side_lyric_label_) {
        lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
    }
    SetAppTileStatus(8, music_title_.c_str(), COLOR_GREEN);

    ESP_LOGI(TAG, "SetMusicLyric line=%s title=%s artist=%s",
             clean_line.c_str(), title ? title : "", artist ? artist : "");

    if (xiaozhi_state_label_ && !is_themed_face_gif_theme()) {
        lv_label_set_text(xiaozhi_state_label_, state.c_str());
        lv_obj_invalidate(xiaozhi_state_label_);
    }
    if (xiaozhi_message_label_) {
        lv_label_set_text(xiaozhi_message_label_, clean_line.c_str());
        lv_obj_invalidate(xiaozhi_message_label_);
    }
    if (xiaozhi_hint_label_) {
        lv_label_set_text(xiaozhi_hint_label_, state.c_str());
        lv_obj_invalidate(xiaozhi_hint_label_);
    }
    if (music_lyric_panel_ && music_lyric_label_) {
        lv_obj_clear_flag(music_lyric_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(music_lyric_label_, clean_line.c_str());
        lv_obj_move_foreground(music_lyric_panel_);
        lv_obj_invalidate(music_lyric_panel_);
        ESP_LOGI(TAG, "SetMusicLyric overlay line=%s", clean_line.c_str());
    }
    emotion_ = "happy";

}

void DesktopUI::ClearMusicLyric() {
    music_lyric_hold_until_ms_ = 0;
    music_title_ = "No song yet";
    music_artist_ = "Ask XiaoZhi to play NetEase music";
    music_line_ = "Tap Ask and say a song name.";
    if (music_title_label_) {
        lv_label_set_text(music_title_label_, music_title_.c_str());
    }
    if (music_artist_label_) {
        lv_label_set_text(music_artist_label_, music_artist_.c_str());
    }
    if (music_line_label_) {
        lv_label_set_text(music_line_label_, "Ready");
    }
    if (music_side_lyric_label_) {
        lv_label_set_text(music_side_lyric_label_, music_line_.c_str());
    }
    if (music_lyric_label_) {
        lv_label_set_text(music_lyric_label_, "");
    }
    if (music_lyric_panel_) {
        lv_obj_add_flag(music_lyric_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(music_lyric_panel_);
    }
    SetAppTileStatus(8, "Ask song", is_tupi_warm_theme() ? COLOR_GREEN : COLOR_PURPLE);
    ESP_LOGI(TAG, "ClearMusicLyric");
}

bool DesktopUI::IsMusicLyricActive(int64_t now_ms) const {
    return music_lyric_hold_until_ms_ > now_ms;
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

    lv_obj_t* parent = lv_obj_get_parent(self->weather_sun_);
    if (!parent) return;

    constexpr uint32_t kMaxParticles = 12;
    const uint32_t child_count = lv_obj_get_child_count(parent);
    if (child_count >= kMaxParticles + 10) {
        return;
    }

    const int code = self->current_weather_code_;
    const bool is_rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
    const bool is_snow = code >= 71 && code <= 77;
    const bool is_storm = code >= 95;
    const bool is_clear = code == 0;
    const bool storm_flash = is_storm && (esp_random() % 4 == 0);

    lv_obj_t* particle = lv_obj_create(parent);
    lv_obj_remove_style_all(particle);
    const int16_t size = is_rain || is_storm ? 3 : (is_snow ? 6 : 4);
    lv_obj_set_size(particle,
                    storm_flash ? static_cast<int16_t>(18 + (esp_random() % 12)) : size,
                    storm_flash ? 3 : (is_rain || is_storm ? 13 : size));
    lv_obj_set_style_radius(particle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(particle,
                              storm_flash ? COLOR_GOLD :
                              (is_rain || is_storm) ? COLOR_BLUE :
                              (is_snow ? COLOR_CREAM : COLOR_GOLD), 0);
    lv_obj_set_style_bg_opa(particle, storm_flash ? LV_OPA_80 : LV_OPA_60, 0);

    int16_t x = 48 + (esp_random() % 70);
    int16_t y = 54 + (esp_random() % 44);
    int16_t end_y = y - 34;
    int16_t end_x = x;
    if (storm_flash) {
        x = 56 + (esp_random() % 54);
        y = 56 + (esp_random() % 30);
        end_x = x + static_cast<int16_t>(4 + (esp_random() % 7));
        end_y = y + static_cast<int16_t>((esp_random() % 9) - 4);
        lv_obj_set_style_transform_rotation(particle, (esp_random() % 2) ? 270 : 620, 0);
    } else if (is_storm) {
        x = 42 + (esp_random() % 78);
        y = 70 + (esp_random() % 20);
        end_x = x + static_cast<int16_t>(4 + (esp_random() % 7));
        end_y = y + static_cast<int16_t>(16 + (esp_random() % 10));
        if (end_y > 104) end_y = 104;
        lv_obj_set_style_transform_rotation(particle, 180, 0);
    } else if (is_rain) {
        x = 44 + (esp_random() % 74);
        y = 74 + (esp_random() % 22);
        end_x = x + 8;
        end_y = y + 28;
        if (end_y > 104) end_y = 104;
        lv_obj_set_style_transform_rotation(particle, 180, 0);
    } else if (is_snow) {
        x = 42 + (esp_random() % 76);
        y = 78 + (esp_random() % 20);
        end_x = x + static_cast<int16_t>((esp_random() % 17) - 8);
        end_y = y + 24;
        if (end_y > 104) end_y = 104;
    } else if (is_clear) {
        x = 52 + (esp_random() % 58);
        y = 34 + (esp_random() % 42);
        end_y = y - 36;
    }
    lv_obj_align(particle, LV_ALIGN_TOP_LEFT, x, y);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, particle);
    lv_anim_set_values(&anim, y, end_y);
    lv_anim_set_time(&anim, storm_flash ? 360 : ((is_rain || is_storm) ? 760 : 1500));
    lv_anim_set_exec_cb(&anim, ObjYCb);
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
        lv_obj_del(static_cast<lv_obj_t*>(a->var));
    });
    lv_anim_start(&anim);

    if (end_x != x) {
        lv_anim_t drift;
        lv_anim_init(&drift);
        lv_anim_set_var(&drift, particle);
        lv_anim_set_values(&drift, x, end_x);
        lv_anim_set_time(&drift, storm_flash ? 360 : ((is_rain || is_storm) ? 760 : 1500));
        lv_anim_set_exec_cb(&drift, ObjXCb);
        lv_anim_start(&drift);
    }

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, particle);
    lv_anim_set_values(&fade, LV_OPA_60, LV_OPA_TRANSP);
    lv_anim_set_time(&fade, storm_flash ? 360 : ((is_rain || is_storm) ? 760 : 1500));
    lv_anim_set_exec_cb(&fade, ObjOpaCb);
    lv_anim_start(&fade);
}

void DesktopUI::MusicCoverTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->music_page_) {
        return;
    }

    self->music_cover_phase_ = static_cast<uint8_t>(self->music_cover_phase_ + 1);
    const bool active = self->music_title_ != "No song yet" ||
                        self->music_line_.find("Listening") != std::string::npos ||
                        self->music_line_.find("Playing") != std::string::npos ||
                        self->music_line_.find("Replaying") != std::string::npos;
    const uint8_t phase = self->music_cover_phase_;
    if (self->music_cover_disc_) {
        lv_obj_align(self->music_cover_disc_, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_set_style_opa(self->music_cover_disc_, active ? LV_OPA_COVER : LV_OPA_70, 0);
    }

    static constexpr int16_t kBarX[] = {20, 40, 60, 80};
    static constexpr uint8_t kWave[] = {18, 32, 44, 26, 38, 22, 40, 28};
    for (size_t i = 0; i < sizeof(self->music_cover_bars_) / sizeof(self->music_cover_bars_[0]); ++i) {
        lv_obj_t* bar = self->music_cover_bars_[i];
        if (!bar) {
            continue;
        }
        const int16_t height = active ? kWave[(phase + i * 2) % (sizeof(kWave) / sizeof(kWave[0]))] : 18;
        lv_obj_set_size(bar, 9, height);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, kBarX[i], -10);
        lv_obj_set_style_bg_opa(bar, active ? LV_OPA_80 : LV_OPA_30, 0);
    }
}

void DesktopUI::SetAppsMoreVisible(bool visible) {
    apps_showing_more_ = visible;
    if (apps_primary_group_) {
        if (visible) {
            lv_obj_add_flag(apps_primary_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(apps_primary_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (apps_more_group_) {
        if (visible) {
            lv_obj_clear_flag(apps_more_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(apps_more_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (apps_more_button_) {
        lv_obj_t* label = lv_obj_get_child(apps_more_button_, 0);
        if (label) {
            lv_label_set_text(label, visible ? "All Apps" : "More");
        }
    }
}

void DesktopUI::SetShakeLabSamplingCallback(std::function<void(bool)> callback) {
    shake_lab_sampling_callback_ = std::move(callback);
}

void DesktopUI::CreateShakeLabPage(lv_obj_t* root) {
    shake_lab_page_ = lv_obj_create(root);
    lv_obj_add_style(shake_lab_page_, &style_screen, 0);
    lv_obj_set_size(shake_lab_page_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(shake_lab_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shake_lab_page_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(shake_lab_page_, media_gesture_cb, LV_EVENT_GESTURE, nullptr);
    lv_obj_set_style_bg_color(shake_lab_page_, COLOR_BG, 0);

    lv_obj_t* title = label_en(shake_lab_page_, "Shake Lab", &style_en);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 32);
    lv_obj_t* subtitle = label_en(shake_lab_page_, "摇一摇实验室", &style_gold);
    lv_obj_set_style_text_font(subtitle, qd_cn_font_16(), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 24, 58);
    lv_obj_t* back = CreateButton(shake_lab_page_, "Back", nullptr);
    lv_obj_set_size(back, 76, 28);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -18, 34);

    shake_lab_home_group_ = lv_obj_create(shake_lab_page_);
    lv_obj_remove_style_all(shake_lab_home_group_);
    lv_obj_set_size(shake_lab_home_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(shake_lab_home_group_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ask_card = CreatePanel(shake_lab_home_group_, 138, 156, 18, 104);
    lv_obj_set_style_border_color(ask_card, COLOR_PURPLE, 0);
    lv_obj_set_style_border_width(ask_card, 2, 0);
    lv_obj_t* ask_orb = circle(ask_card, 60, COLOR_PURPLE, LV_OPA_70);
    lv_obj_align(ask_orb, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_t* ask_core = circle(ask_card, 26, COLOR_GOLD, LV_OPA_COVER);
    lv_obj_align_to(ask_core, ask_orb, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* ask_title = label_en(ask_card, "Ask Ball", &style_en);
    lv_obj_set_style_text_font(ask_title, &lv_font_montserrat_16, 0);
    lv_obj_align(ask_title, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_t* ask_cn = label_en(ask_card, "答案球", &style_gold);
    lv_obj_set_style_text_font(ask_cn, qd_cn_font_16(), 0);
    lv_obj_align(ask_cn, LV_ALIGN_TOP_MID, 0, 116);

    lv_obj_t* dice_card = CreatePanel(shake_lab_home_group_, 138, 156, 171, 104);
    lv_obj_set_style_border_color(dice_card, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(dice_card, 2, 0);
    lv_obj_t* dice_demo = lv_obj_create(dice_card);
    lv_obj_remove_style_all(dice_demo);
    lv_obj_set_size(dice_demo, 66, 66);
    lv_obj_set_style_radius(dice_demo, 10, 0);
    lv_obj_set_style_bg_color(dice_demo, COLOR_CREAM, 0);
    lv_obj_set_style_bg_opa(dice_demo, LV_OPA_COVER, 0);
    lv_obj_align(dice_demo, LV_ALIGN_TOP_MID, 0, 18);
    for (int i = 0; i < 5; ++i) {
        lv_obj_t* dot = circle(dice_demo, 9, COLOR_GREEN, LV_OPA_COVER);
        const int16_t px = (i == 1 || i == 3) ? 46 : (i == 2 ? 28 : 10);
        const int16_t py = (i < 2) ? 10 : (i < 4 ? 28 : 46);
        lv_obj_set_pos(dot, px, py);
    }
    lv_obj_t* dice_title = label_en(dice_card, "Dice", &style_en);
    lv_obj_set_style_text_font(dice_title, &lv_font_montserrat_16, 0);
    lv_obj_align(dice_title, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_t* dice_cn = label_en(dice_card, "骰子", &style_green);
    lv_obj_set_style_text_font(dice_cn, qd_cn_font_16(), 0);
    lv_obj_align(dice_cn, LV_ALIGN_TOP_MID, 0, 116);

    lv_obj_t* fortune_card = CreatePanel(shake_lab_home_group_, 138, 156, 324, 104);
    lv_obj_set_style_border_color(fortune_card, COLOR_GOLD, 0);
    lv_obj_set_style_border_width(fortune_card, 2, 0);
    lv_obj_t* stick = lv_obj_create(fortune_card);
    lv_obj_remove_style_all(stick);
    lv_obj_set_size(stick, 34, 72);
    lv_obj_set_style_radius(stick, 8, 0);
    lv_obj_set_style_bg_color(stick, COLOR_GOLD, 0);
    lv_obj_set_style_bg_opa(stick, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_rotation(stick, 80, 0);
    lv_obj_align(stick, LV_ALIGN_TOP_MID, 0, 13);
    lv_obj_t* fortune_mark = label_en(stick, "签", &style_en);
    lv_obj_set_style_text_font(fortune_mark, &qd_font_lxgw_20, 0);
    lv_obj_set_style_text_color(fortune_mark, COLOR_BG, 0);
    lv_obj_align(fortune_mark, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* fortune_title = label_en(fortune_card, "Fortune", &style_en);
    lv_obj_set_style_text_font(fortune_title, &lv_font_montserrat_16, 0);
    lv_obj_align(fortune_title, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_t* fortune_cn = label_en(fortune_card, "趣味抽签", &style_gold);
    lv_obj_set_style_text_font(fortune_cn, &qd_font_lxgw_16, 0);
    lv_obj_align(fortune_cn, LV_ALIGN_TOP_MID, 0, 116);

    shake_lab_mode_group_ = lv_obj_create(shake_lab_page_);
    lv_obj_remove_style_all(shake_lab_mode_group_);
    lv_obj_set_size(shake_lab_mode_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(shake_lab_mode_group_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shake_lab_mode_group_, LV_OBJ_FLAG_HIDDEN);
    shake_lab_mode_title_ = label_en(shake_lab_mode_group_, "Ask Ball", &style_en);
    lv_obj_set_style_text_font(shake_lab_mode_title_, &lv_font_montserrat_16, 0);
    lv_obj_align(shake_lab_mode_title_, LV_ALIGN_TOP_LEFT, 190, 42);
    lv_obj_t* home = CreateButton(shake_lab_mode_group_, "Home", nullptr);
    lv_obj_set_size(home, 70, 24);
    lv_obj_align(home, LV_ALIGN_TOP_RIGHT, -18, 74);

    shake_lab_ask_group_ = lv_obj_create(shake_lab_mode_group_);
    lv_obj_remove_style_all(shake_lab_ask_group_);
    lv_obj_set_size(shake_lab_ask_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(shake_lab_ask_group_, LV_OBJ_FLAG_SCROLLABLE);
    shake_lab_glow_[0] = circle(shake_lab_ask_group_, 210, COLOR_PURPLE, LV_OPA_20);
    lv_obj_align(shake_lab_glow_[0], LV_ALIGN_TOP_MID, 0, 80);
    shake_lab_glow_[1] = circle(shake_lab_ask_group_, 174, COLOR_BLUE, LV_OPA_30);
    lv_obj_align(shake_lab_glow_[1], LV_ALIGN_TOP_MID, 0, 98);
    shake_lab_ball_ = circle(shake_lab_ask_group_, 148, COLOR_PURPLE, LV_OPA_COVER);
    lv_obj_set_style_border_width(shake_lab_ball_, 3, 0);
    lv_obj_set_style_border_color(shake_lab_ball_, COLOR_GOLD, 0);
    lv_obj_align(shake_lab_ball_, LV_ALIGN_TOP_MID, 0, 111);
    for (size_t i = 0; i < sizeof(shake_lab_particles_) / sizeof(shake_lab_particles_[0]); ++i) {
        shake_lab_particles_[i] = circle(shake_lab_ask_group_, 7, i % 2 ? COLOR_GOLD : COLOR_GREEN, LV_OPA_70);
    }
    shake_lab_answer_label_ = label_en(shake_lab_ask_group_, "在心里想一个问题，\n然后摇一摇。", &style_en);
    lv_obj_set_style_text_font(shake_lab_answer_label_, qd_cn_font_20(), 0);
    lv_obj_set_width(shake_lab_answer_label_, 138);
    lv_label_set_long_mode(shake_lab_answer_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(shake_lab_answer_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(shake_lab_answer_label_, LV_ALIGN_TOP_MID, 0, 156);
    shake_lab_hint_label_ = label_en(shake_lab_ask_group_, "Shake steadily to reveal", &style_muted);
    lv_obj_set_style_text_font(shake_lab_hint_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(shake_lab_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -14);

    shake_lab_dice_group_ = lv_obj_create(shake_lab_mode_group_);
    lv_obj_remove_style_all(shake_lab_dice_group_);
    lv_obj_set_size(shake_lab_dice_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(shake_lab_dice_group_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shake_lab_dice_group_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* dice_minus = CreateButton(shake_lab_dice_group_, "-", nullptr);
    lv_obj_set_size(dice_minus, 52, 28);
    lv_obj_align(dice_minus, LV_ALIGN_TOP_LEFT, 142, 76);
    shake_lab_dice_count_label_ = label_en(shake_lab_dice_group_, "1 Die", &style_green);
    lv_obj_set_width(shake_lab_dice_count_label_, 90);
    lv_obj_set_style_text_align(shake_lab_dice_count_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(shake_lab_dice_count_label_, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_t* dice_plus = CreateButton(shake_lab_dice_group_, "+", nullptr);
    lv_obj_set_size(dice_plus, 52, 28);
    lv_obj_align(dice_plus, LV_ALIGN_TOP_LEFT, 286, 76);
    for (int die = 0; die < 6; ++die) {
        shake_lab_dice_boxes_[die] = lv_obj_create(shake_lab_dice_group_);
        lv_obj_remove_style_all(shake_lab_dice_boxes_[die]);
        lv_obj_set_size(shake_lab_dice_boxes_[die], 58, 58);
        lv_obj_set_style_radius(shake_lab_dice_boxes_[die], 9, 0);
        lv_obj_set_style_bg_color(shake_lab_dice_boxes_[die], COLOR_CREAM, 0);
        lv_obj_set_style_bg_opa(shake_lab_dice_boxes_[die], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(shake_lab_dice_boxes_[die], COLOR_GREEN, 0);
        lv_obj_set_style_border_width(shake_lab_dice_boxes_[die], 2, 0);
        const int col = die % 3;
        const int row = die / 3;
        lv_obj_align(shake_lab_dice_boxes_[die], LV_ALIGN_TOP_LEFT, 133 + col * 76, 116 + row * 66);
        shake_lab_dice_values_[die] = label_en(shake_lab_dice_boxes_[die], "", &style_en);
        lv_obj_add_flag(shake_lab_dice_values_[die], LV_OBJ_FLAG_HIDDEN);
        for (int pip = 0; pip < 7; ++pip) {
            shake_lab_dice_dots_[die][pip] = circle(shake_lab_dice_boxes_[die], 7, COLOR_GREEN, LV_OPA_COVER);
        }
    }
    shake_lab_dice_total_label_ = label_en(shake_lab_dice_group_, "Choose 1-6 dice, then shake", &style_muted);
    lv_obj_set_style_text_font(shake_lab_dice_total_label_, &lv_font_montserrat_12, 0);
    lv_obj_align(shake_lab_dice_total_label_, LV_ALIGN_BOTTOM_MID, 0, -25);
    shake_lab_dice_lucky_label_ = label_en(shake_lab_dice_group_, "LUCKY", &style_gold);
    lv_obj_set_style_text_font(shake_lab_dice_lucky_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(shake_lab_dice_lucky_label_, LV_ALIGN_BOTTOM_RIGHT, -18, -8);
    lv_obj_add_flag(shake_lab_dice_lucky_label_, LV_OBJ_FLAG_HIDDEN);

    shake_lab_fortune_group_ = lv_obj_create(shake_lab_mode_group_);
    lv_obj_remove_style_all(shake_lab_fortune_group_);
    lv_obj_set_size(shake_lab_fortune_group_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(shake_lab_fortune_group_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shake_lab_fortune_group_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* fortune_panel = CreatePanel(shake_lab_fortune_group_, 410, 198, 35, 104);
    lv_obj_set_style_border_color(fortune_panel, COLOR_GOLD, 0);
    lv_obj_set_style_border_width(fortune_panel, 2, 0);
    shake_lab_fortune_number_label_ = label_en(fortune_panel, "诚心摇签·静待签来", &style_gold);
    lv_obj_set_style_text_font(shake_lab_fortune_number_label_, &qd_font_lxgw_20, 0);
    lv_obj_align(shake_lab_fortune_number_label_, LV_ALIGN_TOP_MID, 0, 14);
    shake_lab_fortune_poem_label_ = label_en(fortune_panel, "手持签筒轻轻摇，\n一枝缘分自会来。", &style_en);
    lv_obj_set_style_text_font(shake_lab_fortune_poem_label_, &qd_font_lxgw_20, 0);
    lv_obj_set_width(shake_lab_fortune_poem_label_, 360);
    lv_label_set_long_mode(shake_lab_fortune_poem_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(shake_lab_fortune_poem_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(shake_lab_fortune_poem_label_, LV_ALIGN_TOP_MID, 0, 52);
    shake_lab_fortune_explain_label_ = label_en(fortune_panel, "摇动设备，抽取今日之签。", &style_muted);
    lv_obj_set_style_text_font(shake_lab_fortune_explain_label_, &qd_font_lxgw_16, 0);
    lv_obj_set_width(shake_lab_fortune_explain_label_, 370);
    lv_label_set_long_mode(shake_lab_fortune_explain_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(shake_lab_fortune_explain_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(shake_lab_fortune_explain_label_, LV_ALIGN_TOP_MID, 0, 118);
    shake_lab_fortune_hint_label_ = label_en(fortune_panel, "Shake steadily to draw", &style_green);
    // This status switches between English and mixed Chinese/English strings
    // (for example "可再次摇签 · Shake again"), so it must not use the
    // Latin-only Montserrat font.
    lv_obj_set_style_text_font(shake_lab_fortune_hint_label_, &qd_font_lxgw_16, 0);
    lv_obj_align(shake_lab_fortune_hint_label_, LV_ALIGN_BOTTOM_MID, 0, -10);

    shake_lab_anim_timer_ = lv_timer_create(ShakeLabAnimCb, 80, this);
    lv_timer_pause(shake_lab_anim_timer_);
    UpdateShakeLabDice();
}

bool DesktopUI::HandleShakeLabTap(uint16_t x, uint16_t y) {
    auto hit = [x, y](uint16_t left, uint16_t top, uint16_t width, uint16_t height) {
        return x >= left && x < left + width && y >= top && y < top + height;
    };
    if (hit(388, 30, 84, 42)) {
        if (shake_lab_mode_ == ShakeLabMode::HOME) {
            NavigateBack();
        } else {
            LeaveShakeLabMode();
        }
        return true;
    }
    if (shake_lab_mode_ == ShakeLabMode::HOME) {
        if (hit(18, 104, 138, 156)) {
            EnterShakeLabMode(ShakeLabMode::ASK_BALL);
            return true;
        }
        if (hit(171, 104, 138, 156)) {
            EnterShakeLabMode(ShakeLabMode::DICE);
            return true;
        }
        if (hit(324, 104, 138, 156)) {
            EnterShakeLabMode(ShakeLabMode::FORTUNE);
            return true;
        }
        return false;
    }
    if (hit(392, 72, 82, 40)) {
        LeaveShakeLabMode();
        return true;
    }
    if (shake_lab_mode_ == ShakeLabMode::DICE) {
        if (hit(132, 70, 72, 42)) {
            shake_lab_dice_count_ = std::max<uint8_t>(1, shake_lab_dice_count_ - 1);
            UpdateShakeLabDice();
            return true;
        }
        if (hit(276, 70, 72, 42)) {
            shake_lab_dice_count_ = std::min<uint8_t>(6, shake_lab_dice_count_ + 1);
            UpdateShakeLabDice();
            return true;
        }
    }
    return false;
}

void DesktopUI::EnterShakeLabMode(ShakeLabMode mode) {
    if (!shake_lab_page_ || mode == ShakeLabMode::HOME) {
        return;
    }
    shake_lab_mode_ = mode;
    shake_lab_detector_state_ = ShakeDetector::State::ARMED;
    shake_lab_intensity_ = 0;
    shake_lab_anim_tick_ = 0;
    if (shake_lab_home_group_) {
        lv_obj_add_flag(shake_lab_home_group_, LV_OBJ_FLAG_HIDDEN);
    }
    if (shake_lab_mode_group_) {
        lv_obj_clear_flag(shake_lab_mode_group_, LV_OBJ_FLAG_HIDDEN);
    }
    if (shake_lab_ask_group_) {
        if (mode == ShakeLabMode::ASK_BALL) {
            lv_obj_clear_flag(shake_lab_ask_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shake_lab_ask_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (shake_lab_dice_group_) {
        if (mode == ShakeLabMode::DICE) {
            lv_obj_clear_flag(shake_lab_dice_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shake_lab_dice_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (shake_lab_fortune_group_) {
        if (mode == ShakeLabMode::FORTUNE) {
            lv_obj_clear_flag(shake_lab_fortune_group_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shake_lab_fortune_group_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (shake_lab_mode_title_) {
        const char* title = mode == ShakeLabMode::ASK_BALL ? "Ask Ball" :
                            (mode == ShakeLabMode::DICE ? "Dice" : "Fortune Stick");
        lv_label_set_text(shake_lab_mode_title_, title);
    }
    if (mode == ShakeLabMode::ASK_BALL && shake_lab_answer_label_) {
        lv_label_set_text(shake_lab_answer_label_, "在心里想一个问题，\n然后摇一摇。");
        lv_label_set_text(shake_lab_hint_label_, "Shake steadily to reveal");
        UpdateShakeLabVisuals();
    }
    if (mode == ShakeLabMode::DICE) {
        if (shake_lab_dice_total_label_) {
            lv_label_set_text(shake_lab_dice_total_label_, "Choose 1-6 dice, then shake");
        }
        if (shake_lab_dice_lucky_label_) {
            lv_obj_add_flag(shake_lab_dice_lucky_label_, LV_OBJ_FLAG_HIDDEN);
        }
        UpdateShakeLabDice();
    }
    if (mode == ShakeLabMode::FORTUNE) {
        lv_label_set_text(shake_lab_fortune_number_label_, "诚心摇签·静待签来");
        lv_label_set_text(shake_lab_fortune_poem_label_, "手持签筒轻轻摇，\n一枝缘分自会来。");
        lv_label_set_text(shake_lab_fortune_explain_label_, "摇动设备，抽取今日之签。");
        lv_label_set_text(shake_lab_fortune_hint_label_, "Shake steadily to draw");
    }
    shake_lab_sampling_active_ = true;
    if (shake_lab_sampling_callback_) {
        shake_lab_sampling_callback_(true);
    }
    const char* mode_name = mode == ShakeLabMode::ASK_BALL ? "Ask Ball" :
                            (mode == ShakeLabMode::DICE ? "Dice" : "Fortune");
    ESP_LOGI(TAG, "Shake Lab mode=%s armed", mode_name);
}

void DesktopUI::LeaveShakeLabMode() {
    if (shake_lab_anim_timer_) {
        lv_timer_pause(shake_lab_anim_timer_);
    }
    if (shake_lab_sampling_active_ && shake_lab_sampling_callback_) {
        shake_lab_sampling_callback_(false);
    }
    shake_lab_sampling_active_ = false;
    shake_lab_detector_state_ = ShakeDetector::State::IDLE;
    shake_lab_intensity_ = 0;
    shake_lab_mode_ = ShakeLabMode::HOME;
    if (shake_lab_mode_group_) {
        lv_obj_add_flag(shake_lab_mode_group_, LV_OBJ_FLAG_HIDDEN);
    }
    if (shake_lab_home_group_) {
        lv_obj_clear_flag(shake_lab_home_group_, LV_OBJ_FLAG_HIDDEN);
    }
    ESP_LOGI(TAG, "Shake Lab returned home");
}

void DesktopUI::ReleaseShakeLabPage() {
    LeaveShakeLabMode();
    if (shake_lab_anim_timer_) {
        lv_timer_delete(shake_lab_anim_timer_);
        shake_lab_anim_timer_ = nullptr;
    }
    if (shake_lab_page_) {
        // Paint the destination page first, then reclaim this large object tree.
        lv_obj_delete_async(shake_lab_page_);
    }
    shake_lab_page_ = nullptr;
    shake_lab_home_group_ = nullptr;
    shake_lab_mode_group_ = nullptr;
    shake_lab_ask_group_ = nullptr;
    shake_lab_dice_group_ = nullptr;
    shake_lab_fortune_group_ = nullptr;
    shake_lab_ball_ = nullptr;
    shake_lab_answer_label_ = nullptr;
    shake_lab_hint_label_ = nullptr;
    shake_lab_mode_title_ = nullptr;
    shake_lab_dice_count_label_ = nullptr;
    shake_lab_dice_total_label_ = nullptr;
    shake_lab_dice_lucky_label_ = nullptr;
    shake_lab_fortune_number_label_ = nullptr;
    shake_lab_fortune_poem_label_ = nullptr;
    shake_lab_fortune_explain_label_ = nullptr;
    shake_lab_fortune_hint_label_ = nullptr;
    memset(shake_lab_glow_, 0, sizeof(shake_lab_glow_));
    memset(shake_lab_particles_, 0, sizeof(shake_lab_particles_));
    memset(shake_lab_dice_boxes_, 0, sizeof(shake_lab_dice_boxes_));
    memset(shake_lab_dice_values_, 0, sizeof(shake_lab_dice_values_));
    memset(shake_lab_dice_dots_, 0, sizeof(shake_lab_dice_dots_));
}

void DesktopUI::UpdateShakeLabDice() {
    if (!shake_lab_dice_boxes_[0]) {
        return;
    }
    static constexpr int16_t kPipPositions[7][2] = {
        {10, 10}, {40, 10}, {25, 25}, {10, 40}, {40, 40}, {10, 25}, {40, 25},
    };
    static constexpr bool kPipMap[7][7] = {
        {false, false, true, false, false, false, false},
        {true, false, false, false, true, false, false},
        {true, false, true, false, true, false, false},
        {true, true, false, true, true, false, false},
        {true, true, true, true, true, false, false},
        {true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false},
    };
    if (shake_lab_dice_count_label_) {
        char count_text[16];
        snprintf(count_text, sizeof(count_text), "%u %s", shake_lab_dice_count_,
                 shake_lab_dice_count_ == 1 ? "Die" : "Dice");
        lv_label_set_text(shake_lab_dice_count_label_, count_text);
    }
    for (int die = 0; die < 6; ++die) {
        const bool visible = die < shake_lab_dice_count_;
        if (visible) {
            lv_obj_clear_flag(shake_lab_dice_boxes_[die], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shake_lab_dice_boxes_[die], LV_OBJ_FLAG_HIDDEN);
        }
        const uint8_t value = std::max<uint8_t>(1, std::min<uint8_t>(6, shake_lab_dice_values_state_[die]));
        for (int pip = 0; pip < 7; ++pip) {
            lv_obj_t* dot = shake_lab_dice_dots_[die][pip];
            if (!dot) {
                continue;
            }
            lv_obj_set_pos(dot, kPipPositions[pip][0], kPipPositions[pip][1]);
            if (visible && kPipMap[value - 1][pip]) {
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void DesktopUI::UpdateShakeLabVisuals() {
    if (!shake_lab_page_ || shake_lab_mode_ != ShakeLabMode::ASK_BALL) {
        return;
    }
    const bool active = shake_lab_detector_state_ == ShakeDetector::State::SHAKING ||
        shake_lab_detector_state_ == ShakeDetector::State::SETTLING;
    const int16_t radius = static_cast<int16_t>(72 + (active ? shake_lab_intensity_ / 3 : 0));
    const lv_opa_t particle_opa = active ? static_cast<lv_opa_t>(LV_OPA_50 + shake_lab_intensity_ * 2)
                                        : static_cast<lv_opa_t>(LV_OPA_30);
    for (size_t i = 0; i < sizeof(shake_lab_particles_) / sizeof(shake_lab_particles_[0]); ++i) {
        if (!shake_lab_particles_[i]) {
            continue;
        }
        const float angle = static_cast<float>((shake_lab_anim_tick_ * (active ? 12 : 3) + i * 36) % 360) *
            3.1415926f / 180.0f;
        const int16_t x = static_cast<int16_t>(240 + std::cos(angle) * (radius - static_cast<int>(i % 3) * 8) - 4);
        const int16_t y = static_cast<int16_t>(184 + std::sin(angle) * (radius - static_cast<int>(i % 3) * 8) - 4);
        lv_obj_set_pos(shake_lab_particles_[i], x, y);
        lv_obj_set_style_bg_opa(shake_lab_particles_[i], particle_opa, 0);
    }
    if (shake_lab_ball_) {
        lv_obj_set_style_border_color(shake_lab_ball_, active ? COLOR_GOLD : COLOR_PURPLE, 0);
        lv_obj_set_style_shadow_width(shake_lab_ball_, active ? 18 : 8, 0);
        lv_obj_set_style_shadow_color(shake_lab_ball_, COLOR_PURPLE, 0);
        lv_obj_set_style_shadow_opa(shake_lab_ball_, active ? LV_OPA_70 : LV_OPA_30, 0);
    }
    if (shake_lab_glow_[0]) {
        lv_obj_set_style_bg_opa(shake_lab_glow_[0], active ? LV_OPA_40 : LV_OPA_20, 0);
    }
    if (shake_lab_glow_[1]) {
        lv_obj_set_style_bg_opa(shake_lab_glow_[1], active ? static_cast<lv_opa_t>(140) : static_cast<lv_opa_t>(LV_OPA_30), 0);
    }
}

void DesktopUI::ShakeLabAnimCb(lv_timer_t* timer) {
    auto* self = static_cast<DesktopUI*>(lv_timer_get_user_data(timer));
    if (!self || !self->shake_lab_page_ || self->shake_lab_mode_ == ShakeLabMode::HOME) {
        return;
    }
    ++self->shake_lab_anim_tick_;
    if (self->shake_lab_mode_ == ShakeLabMode::ASK_BALL) {
        self->UpdateShakeLabVisuals();
        return;
    }
    if (self->shake_lab_mode_ == ShakeLabMode::DICE &&
        (self->shake_lab_detector_state_ == ShakeDetector::State::SHAKING ||
         (self->shake_lab_detector_state_ == ShakeDetector::State::SETTLING && self->shake_lab_anim_tick_ % 3 == 0))) {
        for (uint8_t die = 0; die < self->shake_lab_dice_count_; ++die) {
            self->shake_lab_dice_values_state_[die] = static_cast<uint8_t>(esp_random() % 6 + 1);
        }
        self->UpdateShakeLabDice();
    }
}

void DesktopUI::RevealShakeLabResult() {
    if (shake_lab_mode_ == ShakeLabMode::ASK_BALL) {
        static constexpr const char* kAnswers[] = {
            "大胆去做。", "可以。", "值得一试。", "你已经准备好了。", "答案偏向肯定。",
            "可以，但慢一点。", "先做小范围尝试。", "时机正在靠近。", "保持耐心。", "准备好再出发。",
            "再观察一下。", "先收集更多信息。", "现在不必急着决定。", "把问题拆小。", "给自己一点时间。",
            "这一次，答案是否定的。", "暂时不要。", "别为了取悦别人决定。", "换一条路。", "先保护好自己。",
            "你需要的是勇气。", "先完成，再完美。", "换个角度想想。", "相信你的节奏。", "休息后再决定。",
            "先问问内心。", "保持好奇。", "把注意力放回当下。", "小步也算前进。", "答案会慢慢清楚。",
            "可以开始了。", "先别否定自己。", "顺着最重要的事做。", "别让恐惧替你决定。", "今天适合行动。",
            "现在先稳住。", "把边界说清楚。", "相信积累。", "这件事有转机。", "先听听真实感受。",
            "你比想象中更接近。", "留一点余地。", "可以请求帮助。", "不急，答案在路上。", "愿你做自己的选择。",
        };
        const size_t index = esp_random() % (sizeof(kAnswers) / sizeof(kAnswers[0]));
        if (shake_lab_answer_label_) {
            lv_label_set_text(shake_lab_answer_label_, kAnswers[index]);
        }
        if (shake_lab_hint_label_) {
            lv_label_set_text(shake_lab_hint_label_, "Answer revealed");
        }
        ESP_LOGI(TAG, "Shake Lab Ask Ball reveal index=%u", static_cast<unsigned>(index));
    } else if (shake_lab_mode_ == ShakeLabMode::DICE) {
        uint16_t sum = 0;
        bool lucky = false;
        for (uint8_t die = 0; die < shake_lab_dice_count_; ++die) {
            shake_lab_dice_values_state_[die] = static_cast<uint8_t>(esp_random() % 6 + 1);
            sum += shake_lab_dice_values_state_[die];
            lucky = lucky || shake_lab_dice_values_state_[die] == 6;
        }
        UpdateShakeLabDice();
        if (shake_lab_dice_total_label_) {
            char total[32];
            if (shake_lab_dice_count_ == 1) {
                snprintf(total, sizeof(total), "Result: %u", shake_lab_dice_values_state_[0]);
            } else {
                snprintf(total, sizeof(total), "%u dice  Total: %u", shake_lab_dice_count_, sum);
            }
            lv_label_set_text(shake_lab_dice_total_label_, total);
        }
        if (shake_lab_dice_lucky_label_) {
            if (lucky) {
                lv_obj_clear_flag(shake_lab_dice_lucky_label_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(shake_lab_dice_lucky_label_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        ESP_LOGI(TAG, "Shake Lab Dice reveal count=%u total=%u", shake_lab_dice_count_, sum);
    } else if (shake_lab_mode_ == ShakeLabMode::FORTUNE) {
        struct Fortune {
            const char* level;
            const char* poem;
            const char* explain;
        };
        static constexpr Fortune kFortunes[] = {
            {"上上签", "春风得意马蹄轻，\n一日看尽好风景。", "运势正盛，大胆前行；好消息正在路上。"},
            {"上上签", "云开月出正分明，\n不用迟疑问前程。", "困惑将散，目标渐清；选择你真正想走的路。"},
            {"上签", "桂香吹过小窗前，\n喜事轻轻到枕边。", "近日有小惊喜，人缘亦旺；记得接住别人的善意。"},
            {"上签", "长路虽遥步步安，\n贵人携手过重山。", "不必独自硬撑；合作、求助会让事情更顺。"},
            {"上签", "种得梧桐树影长，\n凤来只待日升东。", "积累正在发芽，暂时未见成果，但方向是对的。"},
            {"中上签", "小舟初过浪头平，\n稳把船舵向远行。", "进展可期，宜稳不宜急；先守好节奏。"},
            {"中上签", "花未全开香已至，\n且将耐心待佳期。", "机会已近，再准备一点；不必为短暂等待焦虑。"},
            {"中上签", "故人一笑春风里，\n旧愿重逢有转机。", "旧人、旧事或搁置的计划可能带来新突破。"},
            {"中签", "山中暂起一层雾，\n风过自然见归途。", "信息不足，暂缓决定；先观察，别被情绪催促。"},
            {"中签", "一杯清茶消百虑，\n心定方知下一步。", "你需要的不是更多用力，而是休息后的清醒。"},
            {"中签", "双路分岔莫心慌，\n先问初衷在哪方。", "两个选择各有得失；以长期价值而非眼前轻松作答。"},
            {"中签", "细雨湿衣人不觉，\n微功日积自成河。", "当下看似平常，每天的小步正在改变结果。"},
            {"中平签", "客来客往皆有时，\n门前洒扫守心知。", "先整理环境和边界；不必挽留不属于你的人与事。"},
            {"中平签", "溪水绕石不争先，\n转个弯儿天更宽。", "正面强攻不是唯一解法；换个方法反而更快。"},
            {"中平签", "灯火微明夜未央，\n留些余力待晨光。", "此时宜保守能量，别一次耗尽；重要事留到状态更好时。"},
            {"平签", "潮起潮落本平常，\n不因一浪失方向。", "短期波动不代表失败；看长一点，继续保持基本功。"},
            {"平签", "棋至中盘须细想，\n一着不急局自安。", "现在适合复盘而非冲刺；检查隐藏成本和后续影响。"},
            {"平签", "他人花开他人春，\n自家种子有良辰。", "别用别人的时间表评价自己；你有自己的生长节奏。"},
            {"中下签", "风大莫将高帆挂，\n收绳系船待云开。", "外部变数较多，暂不宜冒进；先留退路与缓冲。"},
            {"中下签", "明珠暂隐尘沙里，\n拂去浮灰价自知。", "价值未被看见不等于没有价值；先完善表达与作品。"},
            {"下签", "急雨敲窗宜闭户，\n莫向风头问远途。", "当下不宜强行推进；先处理风险，等情势稳定再决定。"},
            {"下签", "旧桥木朽莫强过，\n另寻新渡保平安。", "原方案隐患已现；及时换路不是退缩，而是判断力。"},
            {"下签", "言多易失心中尺，\n且把真情留三分。", "谨慎沟通，先听后说；未确定的事不必过早承诺。"},
            {"安心签", "今日无须问吉凶，\n好好吃饭好好眠。", "这一签只提醒你：先照顾好自己，答案不必在今天全部出现。"},
        };
        const size_t index = esp_random() % (sizeof(kFortunes) / sizeof(kFortunes[0]));
        char number[48];
        snprintf(number, sizeof(number), "第%02u签 · %s", static_cast<unsigned>(index + 1), kFortunes[index].level);
        lv_label_set_text(shake_lab_fortune_number_label_, number);
        lv_label_set_text(shake_lab_fortune_poem_label_, kFortunes[index].poem);
        lv_label_set_text(shake_lab_fortune_explain_label_, kFortunes[index].explain);
        lv_label_set_text(shake_lab_fortune_hint_label_, "解签已显示 · Ready again soon");
        ESP_LOGI(TAG, "Shake Lab Fortune reveal index=%u level=%s", static_cast<unsigned>(index),
                 kFortunes[index].level);
    }
    if (!Application::GetInstance().IsExternalAudioActive()) {
        Application::GetInstance().Schedule([]() {
            Application::GetInstance().PlayNotificationSound(Lang::Sounds::P3_SUCCESS);
        });
    } else {
        ESP_LOGI(TAG, "Shake Lab result sound suppressed while external audio is active");
    }
    if (shake_lab_anim_timer_) {
        lv_timer_pause(shake_lab_anim_timer_);
    }
}

void DesktopUI::UpdateShakeLabDetector(const ShakeDetector::Result& result) {
    if (!shake_lab_page_ || !shake_lab_sampling_active_) {
        return;
    }
    shake_lab_intensity_ = result.intensity;
    shake_lab_detector_state_ = result.state;
    switch (result.transition) {
        case ShakeDetector::Transition::ARMED_TO_SHAKING:
            if (shake_lab_mode_ == ShakeLabMode::ASK_BALL && shake_lab_answer_label_) {
                lv_label_set_text(shake_lab_answer_label_, "摇晃已感应");
            } else if (shake_lab_mode_ == ShakeLabMode::DICE && shake_lab_dice_total_label_) {
                lv_label_set_text(shake_lab_dice_total_label_, "Rolling...");
            } else if (shake_lab_mode_ == ShakeLabMode::FORTUNE && shake_lab_fortune_hint_label_) {
                lv_label_set_text(shake_lab_fortune_hint_label_, "签筒已动·请继续摇");
            }
            if (shake_lab_anim_timer_) {
                lv_timer_resume(shake_lab_anim_timer_);
            }
            break;
        case ShakeDetector::Transition::SHAKING_TO_SETTLING:
            if (shake_lab_mode_ == ShakeLabMode::ASK_BALL && shake_lab_answer_label_) {
                lv_label_set_text(shake_lab_answer_label_, "正在感应……");
            } else if (shake_lab_mode_ == ShakeLabMode::DICE && shake_lab_dice_total_label_) {
                lv_label_set_text(shake_lab_dice_total_label_, "Settling...");
            } else if (shake_lab_mode_ == ShakeLabMode::FORTUNE && shake_lab_fortune_hint_label_) {
                lv_label_set_text(shake_lab_fortune_hint_label_, "一枝签正在落下……");
            }
            break;
        case ShakeDetector::Transition::SETTLING_TO_REVEAL:
            RevealShakeLabResult();
            break;
        case ShakeDetector::Transition::COOLDOWN_COMPLETE:
            if (shake_lab_mode_ == ShakeLabMode::ASK_BALL && shake_lab_hint_label_) {
                lv_label_set_text(shake_lab_hint_label_, "Ready to shake again");
            } else if (shake_lab_mode_ == ShakeLabMode::DICE && shake_lab_dice_total_label_) {
                lv_label_set_text(shake_lab_dice_total_label_, "Ready to roll again");
            } else if (shake_lab_mode_ == ShakeLabMode::FORTUNE && shake_lab_fortune_hint_label_) {
                lv_label_set_text(shake_lab_fortune_hint_label_, "可再次摇签 · Shake again");
            }
            ESP_LOGI(TAG, "Shake Lab cooldown complete");
            break;
        case ShakeDetector::Transition::NONE:
            break;
    }
    if (shake_lab_mode_ == ShakeLabMode::ASK_BALL &&
        (result.state == ShakeDetector::State::SHAKING || result.state == ShakeDetector::State::SETTLING)) {
        UpdateShakeLabVisuals();
    }
}
