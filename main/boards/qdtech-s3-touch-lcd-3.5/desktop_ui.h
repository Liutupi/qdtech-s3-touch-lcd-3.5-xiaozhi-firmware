#pragma once

#include "lvgl.h"
#include <cstdint>
#include <functional>

enum class DesktopPage {
    MAIN,
    APPS,
    PHOTO,
    CALENDAR,
    RADIO,
    FOCUS,
    XIAOZHI,
    SETTINGS,
};

class DesktopUI {
public:
    void Create();
    void ShowPage(DesktopPage page);
    void NavigateBack();
    void HandleTouchRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y,
                            int64_t duration_ms);
    void HandleSwipe(int16_t dx, int16_t dy);
    void HandleTap(uint16_t x, uint16_t y);

    // API for updating content
    void SetTime(int hour, int minute, int year, int month, int day, const char* weekday);
    void SetWeather(const char* temperature, const char* summary, int weather_code);
    void SetDailyQuote(const char* quote);
    void SetDailyCard(const char* date, const char* title, const char* body);
    void SetNetworkStatus(const char* status);
    void SetRadioActions(std::function<void()> play_pause, std::function<void()> stop,
                         std::function<void()> next, std::function<void()> prev);
    void SetRadioState(const char* station, const char* state, const char* meta);
    void SetXiaozhiState(const char* state, const char* message, const char* emotion);
    void SetXiaozhiEmotion(const char* emotion);
    void AdjustCalendarMonth(int delta);
    void ShowTodayCalendar();
    void ToggleFocusTimer();
    void ResetFocusTimer();
    void SetFocusMode(bool work_mode);
    void SetSystemBrightness(int value);
    void SetSystemVolume(int value);
    void SetPhotoActiveCallback(std::function<void(bool)> callback);
    void SetPhotoRefreshCallback(std::function<void()> callback);
    void SetPhotoState(const char* title, const char* detail);
    void SetPhotoFrame(const lv_img_dsc_t* image, const char* title, const char* detail);
    void RequestPhotoRefresh();

    // Radio actions (public for LVGL callbacks)
    std::function<void()> radio_play_pause_;
    std::function<void()> radio_stop_;
    std::function<void()> radio_next_;
    std::function<void()> radio_prev_;

    // Radio animation (public for timer callback)
    lv_obj_t* radio_bars_[16] = {};  // 音量动态柱
    bool radio_playing_ = false;

    // Face animation
    void UpdateFaceAnimation();

private:
    // Pages
    lv_obj_t* main_page_ = nullptr;
    lv_obj_t* apps_page_ = nullptr;
    lv_obj_t* photo_page_ = nullptr;
    lv_obj_t* calendar_page_ = nullptr;
    lv_obj_t* radio_page_ = nullptr;
    lv_obj_t* xiaozhi_page_ = nullptr;
    lv_obj_t* settings_page_ = nullptr;
    DesktopPage current_page_ = DesktopPage::MAIN;

    // Main page elements
    lv_obj_t* clock_labels_[4] = {};
    lv_obj_t* colon_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* week_label_ = nullptr;
    lv_obj_t* weather_sun_ = nullptr;
    lv_obj_t* weather_cloud_[3] = {};
    lv_obj_t* weather_rain_[3] = {};
    lv_obj_t* weather_snow_[3] = {};
    lv_obj_t* weather_storm_[2] = {};
    lv_obj_t* weather_temp_label_ = nullptr;
    lv_obj_t* weather_meta_label_ = nullptr;
    lv_obj_t* daily_card_date_label_ = nullptr;
    lv_obj_t* daily_card_title_label_ = nullptr;
    lv_obj_t* quote_label_ = nullptr;
    lv_obj_t* network_status_label_ = nullptr;
    lv_obj_t* status_bar_time_labels_[4] = {};
    lv_obj_t* calendar_app_status_label_ = nullptr;
    
    // Animation elements
    lv_obj_t* daily_card_panel_ = nullptr;
    lv_obj_t* clock_shadow_[4] = {};
    lv_timer_t* weather_particle_timer_ = nullptr;

    // Photo page elements
    lv_obj_t* photo_image_a_ = nullptr;
    lv_obj_t* photo_image_b_ = nullptr;
    lv_obj_t* photo_title_label_ = nullptr;
    lv_obj_t* photo_detail_label_ = nullptr;
    lv_obj_t* photo_refresh_label_ = nullptr;
    bool photo_show_a_ = true;
    std::function<void(bool)> photo_active_callback_;
    std::function<void()> photo_refresh_callback_;

    // Calendar page elements
    lv_obj_t* calendar_title_label_ = nullptr;
    lv_obj_t* calendar_today_label_ = nullptr;
    lv_obj_t* calendar_day_labels_[42] = {};
    lv_obj_t* calendar_day_cells_[42] = {};
    int current_year_ = 0;
    int current_month_ = 0;
    int current_day_ = 0;
    int calendar_year_ = 0;
    int calendar_month_ = 0;
    bool calendar_follow_today_ = true;

    // Radio page elements
    lv_obj_t* radio_station_label_ = nullptr;
    lv_obj_t* radio_state_label_ = nullptr;
    lv_obj_t* radio_meta_label_ = nullptr;
    lv_timer_t* radio_anim_timer_ = nullptr;

    // Focus timer page elements
    lv_obj_t* focus_page_ = nullptr;
    lv_obj_t* focus_arc_ = nullptr;
    lv_obj_t* focus_time_label_ = nullptr;
    lv_obj_t* focus_state_label_ = nullptr;
    lv_obj_t* focus_mode_label_ = nullptr;
    lv_obj_t* focus_start_label_ = nullptr;
    lv_obj_t* focus_completed_label_ = nullptr;
    lv_timer_t* focus_timer_ = nullptr;
    bool focus_running_ = false;
    bool focus_is_work_ = true;
    uint32_t focus_remaining_sec_ = 25 * 60;
    uint32_t focus_total_sec_ = 25 * 60;
    uint16_t focus_completed_count_ = 0;

    // Settings page elements
    lv_obj_t* settings_brightness_slider_ = nullptr;
    lv_obj_t* settings_brightness_value_ = nullptr;
    lv_obj_t* settings_volume_slider_ = nullptr;
    lv_obj_t* settings_volume_value_ = nullptr;
    lv_obj_t* settings_content_ = nullptr;

    // Xiaozhi page elements
    lv_obj_t* face_container_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
    lv_obj_t* pupil_left_ = nullptr;
    lv_obj_t* pupil_right_ = nullptr;
    lv_obj_t* highlight_left_ = nullptr;
    lv_obj_t* highlight_right_ = nullptr;
    lv_obj_t* eyebrow_left_ = nullptr;
    lv_obj_t* eyebrow_right_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* blush_left_ = nullptr;
    lv_obj_t* blush_right_ = nullptr;
    lv_obj_t* xiaozhi_state_label_ = nullptr;
    lv_obj_t* xiaozhi_message_label_ = nullptr;
    lv_obj_t* xiaozhi_hint_label_ = nullptr;

    // Animation state
    const char* emotion_ = "neutral";
    uint32_t anim_tick_ = 0;
    float pupil_offset_x_ = 0;
    float pupil_offset_y_ = 0;
    float pupil_target_x_ = 0;
    float pupil_target_y_ = 0;
    uint8_t blush_alpha_ = 0;
    bool blush_increasing_ = true;
    uint8_t clock_digits_[4] = {0, 0, 0, 0};

    // Internal methods
    void CreateMainPage(lv_obj_t* root);
    void CreateAppsPage(lv_obj_t* root);
    void CreatePhotoPage(lv_obj_t* root);
    void CreateCalendarPage(lv_obj_t* root);
    void CreateRadioPage(lv_obj_t* root);
    void CreateFocusPage(lv_obj_t* root);
    void CreateXiaozhiPage(lv_obj_t* root);
    void CreateSettingsPage(lv_obj_t* root);
    void CreateStatusBar(lv_obj_t* parent);
    void CreateBigTime(lv_obj_t* parent);
    void CreateWeatherPanel(lv_obj_t* parent);
    void CreateQuotePanel(lv_obj_t* parent);
    lv_obj_t* CreateAppTile(lv_obj_t* parent, uint8_t index, const char* cn, const char* en, const char* status, lv_color_t color);
    void CreateFaceUI(lv_obj_t* parent);
    lv_obj_t* CreateButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb);
    lv_obj_t* CreatePanel(lv_obj_t* parent, int16_t w, int16_t h, int16_t x, int16_t y);
    void UpdateWifiList();
    void RefreshSettingsControls();
    bool HandleSettingsSliderRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x);
    void RenderCalendar();
    void ApplyWeatherVisual(int weather_code);
    void UpdateFocusUI();

    void RenderBigTime(int hour, int minute, bool animate);
    void FlipDigit(uint8_t index, uint8_t digit, bool animate);
    void SetFacePart(lv_obj_t* obj, int x, int y, int w, int h, int radius);

    static void ObjOpaCb(void* obj, int32_t value);
    static void ObjYCb(void* obj, int32_t value);
    static void ColonTimerCb(lv_timer_t* timer);
    static void FaceTimerCb(lv_timer_t* timer);
    static void FocusTimerCb(lv_timer_t* timer);
    static void DailyCardBreathCb(lv_timer_t* timer);
    static void ClockShadowCb(lv_timer_t* timer);
    static void WeatherParticleCb(lv_timer_t* timer);
};
