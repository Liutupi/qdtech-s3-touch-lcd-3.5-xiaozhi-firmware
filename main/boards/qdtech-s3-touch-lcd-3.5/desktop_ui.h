#pragma once

#include "lvgl.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

enum class DesktopPage {
    MAIN,
    APPS,
    PHOTO,
    FC,
    CALENDAR,
    RADIO,
    MUSIC,
    MEDIA,
    PODCAST,
    FOCUS,
    HOURGLASS,
    XIAOZHI,
    NETWORK,
    SETTINGS,
    DIAGNOSTICS,
};

class DesktopUI {
public:
    void Create();
    void ShowPage(DesktopPage page);
    void NavigateBack();
    void HandleTouchRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y,
                            int64_t duration_ms);
    void HandleTouchState(uint16_t x, uint16_t y, bool pressed);
    void HandleTouchPoints(const uint16_t* xs, const uint16_t* ys, size_t count);
    void HandleSwipe(int16_t dx, int16_t dy);
    void HandleTap(uint16_t x, uint16_t y);

    // API for updating content
    void SetTime(int hour, int minute, int year, int month, int day, const char* weekday);
    void SetWeather(const char* temperature, const char* summary, int weather_code);
    void SetDailyQuote(const char* quote);
    void SetDailyCard(const char* date, const char* title, const char* body);
    void SetNetworkStatus(const char* status);
    void SetBatteryStatus(int level, bool charging, bool valid);
    void SetDefaultNetwork(size_t index);
    void SetFirmwareUpdateStatus(const char* status, bool update_available, bool busy, int progress,
                                 size_t asset_size = 0, size_t partition_size = 0);
    void CycleTheme();
    void SetRadioActions(std::function<void()> play_pause, std::function<void()> stop,
                         std::function<void()> next, std::function<void()> prev);
    void SetMusicActions(std::function<void()> play, std::function<void()> pause,
                         std::function<void()> next);
    void SetRadioState(const char* station, const char* state, const char* meta);
    void SetPodcastActions(std::function<void()> play_pause, std::function<void()> stop,
                           std::function<void()> next, std::function<void()> prev,
                           std::function<void()> up, std::function<void()> down,
                           std::function<void(int)> seek);
    void ShowPodcastDetail(bool detail);
    void SetPodcastState(const char* title, const char* state, const char* meta,
                         const char* summary, const char* list);
    void SetPodcastCover(const lv_img_dsc_t* image);
    void SetPodcastProgress(int percent);
    void HandlePodcastSeekEvent(lv_event_t* event);
    void SetXiaozhiState(const char* state, const char* message, const char* emotion);
    bool ShowQrCode(const char* content, const char* title, const char* hint);
    void HideQrCode();
    void SetMusicLyric(const char* title, const char* artist, const char* line);
    void RememberMusicTrack(const char* title, const char* artist, const char* url,
                            const char* lyrics_json = nullptr);
    void ClearMusicLyric();
    void StartMusicAsk();
    void SetXiaozhiEmotion(const char* emotion);
    void AdjustCalendarMonth(int delta);
    void ShowTodayCalendar();
    void ToggleFocusTimer();
    void StartFocusTimer(bool rotate_180 = false);
    void ResetFocusTimer();
    void SetFocusMode(bool work_mode);
    void EnterHourglassMode();
    void ExitHourglassMode();
    bool IsHourglassPage() const { return current_page_ == DesktopPage::HOURGLASS; }
    void SetSystemBrightness(int value);
    void SetSystemVolume(int value);
    void SetWifiConfigStatus(const char* status);
    void SetBluetoothConfigStatus(const char* status);
    void ReloadUserProfile();
    void RefreshSettingsControls();
    void SetMainPageCallback(std::function<void()> callback);
    void SetFocusRotationCallback(std::function<void(bool)> callback);
    void SetPhotoActiveCallback(std::function<void(bool)> callback);
    void SetPhotoRefreshCallback(std::function<void()> callback);
    void SetPhotoState(const char* title, const char* detail);
    void SetPhotoFrame(const lv_img_dsc_t* image, const lv_img_dsc_t* background,
                       const char* title, const char* detail);
    void RequestPhotoRefresh();
    void SetFcActiveCallback(std::function<void(bool)> callback);
    void SetFcExitCallback(std::function<void()> callback);
    void SetFcActions(std::function<void()> play_pause, std::function<void()> stop,
                      std::function<void()> next, std::function<void()> prev);
    void SetFcState(const char* title, const char* detail, const char* rom_list);
    void SetFcMode(bool playing);
    void SetFcFrame(const lv_img_dsc_t* image);
    void SetFcControllerCallback(std::function<void(uint8_t)> callback);
    bool IsFcPlayingView() const { return fc_playing_view_; }
    bool IsPhotoPage() const { return current_page_ == DesktopPage::PHOTO; }
    void SetMusicReplayCallback(std::function<void(const std::string& title,
                                                   const std::string& artist,
                                                   const std::string& url,
                                                   const std::string& lyrics_json)> callback);
    bool TryAcceptMusicControlTap();
    void ReplayNextMusicRecent();
    void ReplayMusicRecent(size_t index);
    void RemoveMusicRecent(size_t index);
    void ClearMusicRecent();

    // Radio actions (public for LVGL callbacks)
    std::function<void()> radio_play_pause_;
    std::function<void()> radio_stop_;
    std::function<void()> radio_next_;
    std::function<void()> radio_prev_;
    std::function<void()> music_play_;
    std::function<void()> music_pause_;
    std::function<void()> music_next_;
    std::function<void()> podcast_play_pause_;
    std::function<void()> podcast_stop_;
    std::function<void()> podcast_next_;
    std::function<void()> podcast_prev_;
    std::function<void()> podcast_up_;
    std::function<void()> podcast_down_;
    std::function<void(int)> podcast_seek_;
    std::function<void()> podcast_activate_;
    std::function<void()> podcast_stop_other_media_;
    std::function<void()> fc_play_pause_;
    std::function<void()> fc_stop_;
    std::function<void()> fc_next_;
    std::function<void()> fc_prev_;
    std::function<void(uint8_t)> fc_controller_cb_;
    std::function<void()> fc_stop_other_media_;
    std::function<void(const std::string& title, const std::string& artist,
                       const std::string& url, const std::string& lyrics_json)> music_replay_cb_;

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
    lv_obj_t* fc_page_ = nullptr;
    lv_obj_t* calendar_page_ = nullptr;
    lv_obj_t* radio_page_ = nullptr;
    lv_obj_t* music_page_ = nullptr;
    lv_obj_t* media_page_ = nullptr;
    lv_obj_t* podcast_page_ = nullptr;
    lv_obj_t* xiaozhi_page_ = nullptr;
    lv_obj_t* network_page_ = nullptr;
    lv_obj_t* settings_page_ = nullptr;
    lv_obj_t* diagnostics_page_ = nullptr;
    DesktopPage current_page_ = DesktopPage::MAIN;

    // Main page elements
    lv_obj_t* clock_hour_label_ = nullptr;
    lv_obj_t* clock_minute_label_ = nullptr;
    lv_obj_t* clock_colon_dots_[2] = {};
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* week_label_ = nullptr;
    lv_obj_t* weather_glow_ = nullptr;
    lv_obj_t* weather_sun_ = nullptr;
    lv_obj_t* weather_rays_[6] = {};
    lv_obj_t* weather_cloud_shadow_ = nullptr;
    lv_obj_t* weather_cloud_[3] = {};
    lv_obj_t* weather_rain_[6] = {};
    lv_obj_t* weather_snow_[6] = {};
    lv_obj_t* weather_storm_[4] = {};
    lv_obj_t* weather_scene_gif_ = nullptr;
    lv_obj_t* weather_horizon_ = nullptr;
    lv_obj_t* weather_temp_label_ = nullptr;
    lv_obj_t* weather_meta_label_ = nullptr;
    lv_obj_t* brand_earth_gif_ = nullptr;
    lv_obj_t* brand_logo_labels_[8] = {};
    lv_obj_t* brand_owner_labels_[8] = {};
    size_t brand_label_count_ = 0;
    lv_obj_t* daily_card_date_label_ = nullptr;
    lv_obj_t* daily_card_title_label_ = nullptr;
    lv_obj_t* quote_label_ = nullptr;
    lv_obj_t* network_status_label_ = nullptr;
    lv_obj_t* status_bar_time_labels_[4] = {};
    lv_obj_t* status_bar_battery_labels_[6] = {};
    int battery_level_ = -1;
    bool battery_charging_ = false;
    lv_obj_t* app_status_labels_[10] = {};
    lv_obj_t* app_status_dots_[10] = {};
    lv_obj_t* calendar_app_status_label_ = nullptr;
    std::function<void()> main_page_callback_;
    int current_hour_ = -1;
    int current_minute_ = -1;
    void RegisterBrandLabels(lv_obj_t* logo, lv_obj_t* owner);
    void RefreshBrandLabels();
    
    // Animation elements
    lv_obj_t* daily_card_panel_ = nullptr;
    lv_timer_t* weather_particle_timer_ = nullptr;
    int current_weather_code_ = -1;
    int current_weather_scene_ = -1;

    // Photo page elements
    lv_obj_t* photo_bg_a_ = nullptr;
    lv_obj_t* photo_bg_b_ = nullptr;
    lv_obj_t* photo_image_a_ = nullptr;
    lv_obj_t* photo_image_b_ = nullptr;
    lv_obj_t* photo_title_label_ = nullptr;
    lv_obj_t* photo_detail_label_ = nullptr;
    lv_obj_t* photo_refresh_label_ = nullptr;
    bool photo_show_a_ = true;
    std::string photo_app_status_ = "SD Slideshow";
    lv_color_t photo_app_color_ = {};
    std::function<void(bool)> photo_active_callback_;
    std::function<void()> photo_refresh_callback_;
    bool photo_segment_swipe_active_ = false;
    uint16_t photo_segment_start_x_ = 0;
    uint16_t photo_segment_start_y_ = 0;
    uint16_t photo_segment_min_x_ = 0;
    uint16_t photo_segment_max_x_ = 0;
    int64_t photo_segment_last_ms_ = 0;

    // FC emulator page elements
    lv_obj_t* fc_list_group_ = nullptr;
    lv_obj_t* fc_game_group_ = nullptr;
    lv_obj_t* fc_screen_image_ = nullptr;
    lv_obj_t* fc_title_label_ = nullptr;
    lv_obj_t* fc_detail_label_ = nullptr;
    lv_obj_t* fc_list_label_ = nullptr;
    bool fc_playing_view_ = false;
    bool fc_list_touch_latched_ = false;
    std::string fc_app_status_ = "SD ROMs";
    lv_color_t fc_app_color_ = {};
    std::function<void(bool)> fc_active_callback_;
    std::function<void()> fc_exit_callback_;

    // Calendar page elements
    lv_obj_t* calendar_title_label_ = nullptr;
    lv_obj_t* calendar_today_label_ = nullptr;
    lv_obj_t* calendar_card_day_label_ = nullptr;
    lv_obj_t* calendar_card_weekday_label_ = nullptr;
    lv_obj_t* calendar_card_date_label_ = nullptr;
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

    // Music request page elements
    lv_obj_t* music_title_label_ = nullptr;
    lv_obj_t* music_artist_label_ = nullptr;
    lv_obj_t* music_line_label_ = nullptr;
    lv_obj_t* music_side_lyric_label_ = nullptr;
    lv_obj_t* music_cover_disc_ = nullptr;
    lv_obj_t* music_cover_note_ = nullptr;
    lv_obj_t* music_cover_bars_[4] = {};
    lv_timer_t* music_cover_timer_ = nullptr;
    uint8_t music_cover_phase_ = 0;
    lv_obj_t* music_hint_label_ = nullptr;
    lv_obj_t* music_recent_clear_button_ = nullptr;
    static constexpr size_t kMusicRecentCount = 3;
    struct MusicRecentTrack {
        std::string title;
        std::string artist;
        std::string url;
        std::string lyrics_json;
    };
    MusicRecentTrack music_recent_[kMusicRecentCount];
    lv_obj_t* music_recent_buttons_[kMusicRecentCount] = {};
    lv_obj_t* music_recent_labels_[kMusicRecentCount] = {};
    size_t music_recent_pending_index_ = kMusicRecentCount;
    std::string music_recent_pending_title_;
    size_t music_recent_failed_index_ = kMusicRecentCount;
    std::string music_recent_failed_reason_;
    int64_t music_control_last_ms_ = 0;
    std::string music_title_ = "No song yet";
    std::string music_artist_ = "Ask XiaoZhi to play NetEase music";
    std::string music_line_ = "Tap Ask and say a song name.";

    // Podcast page elements
    lv_obj_t* podcast_list_group_ = nullptr;
    lv_obj_t* podcast_detail_group_ = nullptr;
    lv_obj_t* podcast_cover_image_ = nullptr;
    lv_obj_t* podcast_title_label_ = nullptr;
    lv_obj_t* podcast_state_label_ = nullptr;
    lv_obj_t* podcast_meta_label_ = nullptr;
    lv_obj_t* podcast_summary_label_ = nullptr;
    lv_obj_t* podcast_list_label_ = nullptr;
    lv_obj_t* podcast_progress_slider_ = nullptr;
    lv_obj_t* podcast_progress_label_ = nullptr;
    bool podcast_detail_view_ = false;
    bool podcast_progress_dragging_ = false;
    std::string podcast_app_status_ = "Episodes";
    lv_color_t podcast_app_color_ = {};

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
    uint32_t focus_count_date_ = 0;
    bool focus_auto_rotated_ = false;
    std::function<void(bool)> focus_rotation_callback_;

    // Hourglass timer page elements
    lv_obj_t* hourglass_page_ = nullptr;
    lv_obj_t* hourglass_portrait_ = nullptr;
    lv_obj_t* hourglass_time_label_ = nullptr;
    lv_obj_t* hourglass_status_label_ = nullptr;
    lv_obj_t* hourglass_top_sand_ = nullptr;
    lv_obj_t* hourglass_bottom_sand_ = nullptr;
    lv_obj_t* hourglass_top_sand_strips_[16] = {};
    lv_obj_t* hourglass_bottom_sand_strips_[22] = {};
    lv_obj_t* hourglass_falling_dots_[12] = {};
    lv_obj_t* hourglass_preset_buttons_[4] = {};
    lv_obj_t* hourglass_preset_labels_[4] = {};
    lv_timer_t* hourglass_tick_timer_ = nullptr;
    lv_timer_t* hourglass_anim_timer_ = nullptr;
    DesktopPage hourglass_return_page_ = DesktopPage::MAIN;
    bool hourglass_running_ = false;
    bool hourglass_done_ = false;
    bool hourglass_alarm_played_ = false;
    bool hourglass_motion_active_ = false;
    uint8_t hourglass_selected_index_ = 2;
    uint8_t hourglass_anim_tick_ = 0;
    uint32_t hourglass_total_sec_ = 15 * 60;
    uint32_t hourglass_remaining_sec_ = 15 * 60;

    // Settings page elements
    lv_obj_t* settings_brightness_slider_ = nullptr;
    lv_obj_t* settings_brightness_value_ = nullptr;
    lv_obj_t* settings_volume_slider_ = nullptr;
    lv_obj_t* settings_volume_value_ = nullptr;
    lv_obj_t* settings_content_ = nullptr;
    lv_obj_t* settings_firmware_version_label_ = nullptr;
    lv_obj_t* settings_firmware_status_label_ = nullptr;
    lv_obj_t* settings_firmware_button_ = nullptr;
    lv_obj_t* settings_firmware_button_label_ = nullptr;
    lv_obj_t* settings_theme_value_ = nullptr;
    lv_obj_t* settings_theme_button_ = nullptr;
    lv_obj_t* settings_theme_button_label_ = nullptr;
    lv_obj_t* settings_profile_logo_value_ = nullptr;
    lv_obj_t* settings_profile_owner_value_ = nullptr;
    lv_obj_t* settings_weather_value_ = nullptr;
    lv_obj_t* settings_wifi_config_status_label_ = nullptr;
    lv_obj_t* settings_ble_status_label_ = nullptr;
    std::string settings_wifi_config_status_ = "WiFi config idle";
    std::string settings_ble_status_ = "BLE idle";
    std::string firmware_update_status_ = "Not checked";
    bool firmware_update_available_ = false;
    bool firmware_update_busy_ = false;
    int firmware_update_progress_ = -1;
    size_t firmware_update_asset_size_ = 0;
    size_t firmware_update_partition_size_ = 0;
    std::string network_app_status_ = "WiFi Hub";
    lv_color_t network_app_color_ = {};

    // Network page elements
    lv_obj_t* network_list_container_ = nullptr;
    lv_obj_t* network_saved_count_label_ = nullptr;
    lv_obj_t* network_detail_label_ = nullptr;

    // Diagnostics page elements
    lv_obj_t* diagnostics_labels_[10] = {};

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
    lv_obj_t* themed_face_gif_ = nullptr;
    const lv_image_dsc_t* themed_face_src_ = nullptr;
    lv_obj_t* cat_nose_ = nullptr;
    lv_obj_t* cat_status_mark_1_ = nullptr;
    lv_obj_t* cat_status_mark_2_ = nullptr;
    lv_obj_t* cat_status_mark_3_ = nullptr;
    lv_obj_t* xiaozhi_state_label_ = nullptr;
    lv_obj_t* xiaozhi_message_label_ = nullptr;
    lv_obj_t* xiaozhi_hint_label_ = nullptr;
    lv_obj_t* music_lyric_panel_ = nullptr;
    lv_obj_t* music_lyric_label_ = nullptr;
    lv_obj_t* qr_overlay_ = nullptr;
    lv_obj_t* qr_title_label_ = nullptr;
    lv_obj_t* qr_hint_label_ = nullptr;
    lv_obj_t* qr_code_ = nullptr;

    // Animation state
    std::string emotion_ = "neutral";
    uint32_t anim_tick_ = 0;
    float pupil_offset_x_ = 0;
    float pupil_offset_y_ = 0;
    float pupil_target_x_ = 0;
    float pupil_target_y_ = 0;
    uint8_t blush_alpha_ = 0;
    bool blush_increasing_ = true;
    int64_t music_lyric_hold_until_ms_ = 0;
    // Internal methods
    void CreateMainPage(lv_obj_t* root);
    void CreateAppsPage(lv_obj_t* root);
    void CreatePhotoPage(lv_obj_t* root);
    void CreateFcPage(lv_obj_t* root);
    void CreateCalendarPage(lv_obj_t* root);
    void CreateRadioPage(lv_obj_t* root);
    void CreateMusicPage(lv_obj_t* root);
    void CreateMediaPage(lv_obj_t* root);
    void CreatePodcastPage(lv_obj_t* root);
    void CreateFocusPage(lv_obj_t* root);
    void CreateHourglassPage(lv_obj_t* root);
    void CreateXiaozhiPage(lv_obj_t* root);
    void CreateNetworkPage(lv_obj_t* root);
    void CreateSettingsPage(lv_obj_t* root);
    void CreateDiagnosticsPage(lv_obj_t* root);
    void CreateStatusBar(lv_obj_t* parent);
    void CreateBigTime(lv_obj_t* parent);
    void CreateWeatherPanel(lv_obj_t* parent);
    void CreateQuotePanel(lv_obj_t* parent);
    lv_obj_t* CreateAppTile(lv_obj_t* parent, uint8_t index, const char* cn, const char* en, const char* status, lv_color_t color);
    void CreateFaceUI(lv_obj_t* parent);
    void CreateQrOverlay(lv_obj_t* root);
    void EnsureThemedFaceGif();
    void ReleaseThemedFaceGif();
    lv_obj_t* CreateButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb);
    lv_obj_t* CreatePanel(lv_obj_t* parent, int16_t w, int16_t h, int16_t x, int16_t y);
    void UpdateWifiList();
    void RefreshDiagnostics();
    void SetAppTileStatus(uint8_t index, const char* status, lv_color_t color);
    void RefreshAppTileStatuses();
    void LoadMusicRecent();
    void SaveMusicRecent();
    void RefreshMusicRecent();
    bool HandleSettingsSliderRelease(uint16_t start_x, uint16_t start_y, uint16_t end_x);
    void RenderCalendar();
    void ApplyWeatherVisual(int weather_code);
    void SelectHourglassPreset(uint8_t index);
    void ResetHourglassToDefault();
    void UpdateHourglassUI();
    void UpdateHourglassButtons();
    bool HandleHourglassTap(uint16_t x, uint16_t y);
    void UpdateFocusUI();
    uint32_t CurrentFocusDateKey() const;
    void ReconcileFocusDate(bool persist);

    void RenderBigTime(int hour, int minute, bool animate);
    void FlipDigit(uint8_t index, uint8_t digit, bool animate);
    void SetFacePart(lv_obj_t* obj, int x, int y, int w, int h, int radius);
    bool IsMusicLyricActive(int64_t now_ms) const;

    static void ObjOpaCb(void* obj, int32_t value);
    static void ObjXCb(void* obj, int32_t value);
    static void ObjYCb(void* obj, int32_t value);
    static void ColonTimerCb(lv_timer_t* timer);
    static void FaceTimerCb(lv_timer_t* timer);
    static void FocusTimerCb(lv_timer_t* timer);
    static void HourglassTickCb(lv_timer_t* timer);
    static void HourglassAnimCb(lv_timer_t* timer);
    static void DailyCardBreathCb(lv_timer_t* timer);
    static void ClockShadowCb(lv_timer_t* timer);
    static void WeatherParticleCb(lv_timer_t* timer);
    static void MusicCoverTimerCb(lv_timer_t* timer);
};
