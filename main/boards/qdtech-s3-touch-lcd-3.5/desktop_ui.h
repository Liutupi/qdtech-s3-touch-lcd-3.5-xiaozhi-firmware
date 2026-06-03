#pragma once

#include "lvgl.h"
#include <cstdint>

enum class DesktopPage {
    MAIN,
    APPS,
    XIAOZHI,
};

class DesktopUI {
public:
    void Create();
    void ShowPage(DesktopPage page);
    void HandleSwipe(int16_t dx, int16_t dy);
    void HandleTap(uint16_t x, uint16_t y);

    // API for updating content
    void SetTime(int hour, int minute, int month, int day, const char* weekday);
    void SetWeather(const char* temperature, const char* summary, int weather_code);
    void SetDailyQuote(const char* quote);
    void SetNetworkStatus(const char* status);
    void SetXiaozhiState(const char* state, const char* message, const char* emotion);
    void SetXiaozhiEmotion(const char* emotion);

    // Face animation
    void UpdateFaceAnimation();

private:
    // Pages
    lv_obj_t* main_page_ = nullptr;
    lv_obj_t* apps_page_ = nullptr;
    lv_obj_t* xiaozhi_page_ = nullptr;

    // Main page elements
    lv_obj_t* clock_labels_[4] = {};
    lv_obj_t* colon_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* week_label_ = nullptr;
    lv_obj_t* weather_temp_label_ = nullptr;
    lv_obj_t* weather_meta_label_ = nullptr;
    lv_obj_t* quote_label_ = nullptr;
    lv_obj_t* network_status_label_ = nullptr;
    lv_obj_t* status_bar_time_labels_[2] = {};

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
    void CreateXiaozhiPage(lv_obj_t* root);
    void CreateStatusBar(lv_obj_t* parent);
    void CreateBigTime(lv_obj_t* parent);
    void CreateWeatherPanel(lv_obj_t* parent);
    void CreateQuotePanel(lv_obj_t* parent);
    lv_obj_t* CreateAppTile(lv_obj_t* parent, uint8_t index, const char* cn, const char* en, const char* status, lv_color_t color);
    void CreateFaceUI(lv_obj_t* parent);
    lv_obj_t* CreateButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb);
    lv_obj_t* CreatePanel(lv_obj_t* parent, int16_t w, int16_t h, int16_t x, int16_t y);

    void RenderBigTime(int hour, int minute, bool animate);
    void FlipDigit(uint8_t index, uint8_t digit, bool animate);
    void SetFacePart(lv_obj_t* obj, int x, int y, int w, int h, int radius);

    static void ObjOpaCb(void* obj, int32_t value);
    static void ObjYCb(void* obj, int32_t value);
    static void ColonTimerCb(lv_timer_t* timer);
    static void FaceTimerCb(lv_timer_t* timer);
};
