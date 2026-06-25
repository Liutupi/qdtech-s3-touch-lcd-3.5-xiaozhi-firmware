#include "qdtech_nofrendo.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <bitmap.h>
#include <event.h>
#include <nofconfig.h>
#include <nofrendo.h>
#include <noftypes.h>
#include <osd.h>
#include <vid_drv.h>
#include <nes.h>
#include <nesinput.h>

#define TAG "NofrendoQD"
#define QD_AUDIO_SAMPLE_RATE 22050
#define QD_AUDIO_REFRESH_RATE 60
#define QD_AUDIO_SAMPLES_PER_TICK (QD_AUDIO_SAMPLE_RATE / QD_AUDIO_REFRESH_RATE)

static qd_nofrendo_frame_cb_t s_frame_cb;
static void* s_frame_user;
static qd_nofrendo_audio_cb_t s_audio_cb;
static void* s_audio_user;
static void (*s_sound_playfunc)(void* buffer, int length);
static int16_t s_audio_buffer[QD_AUDIO_SAMPLES_PER_TICK];
static uint32_t s_audio_frames;
static int64_t s_audio_last_log_us;
static volatile uint8_t s_controller;
static volatile bool s_stop_requested;
static uint16_t s_palette[256];
static uint16_t* s_rgb_frame;
static size_t s_rgb_frame_pixels;
static bitmap_t* s_hw_bitmap;
static uint8_t* s_hw_pixels;
static size_t s_hw_pixel_count;
static uint8_t* s_rom_data;
static size_t s_rom_size;
static char s_rom_path[256];
static esp_timer_handle_t s_tick_timer;

static void qd_tick_timer_cb(void* arg) {
    (void)arg;
    ++nofrendo_ticks;
}

void qd_nofrendo_set_frame_callback(qd_nofrendo_frame_cb_t callback, void* user) {
    s_frame_cb = callback;
    s_frame_user = user;
}

void qd_nofrendo_set_audio_callback(qd_nofrendo_audio_cb_t callback, void* user) {
    s_audio_cb = callback;
    s_audio_user = user;
}

void qd_nofrendo_set_controller(uint8_t controller) {
    s_controller = controller;
}

void qd_nofrendo_request_stop(void) {
    s_stop_requested = true;
}

static void qd_free_rom(void) {
    if (s_rom_data) {
        heap_caps_free(s_rom_data);
        s_rom_data = NULL;
    }
    s_rom_size = 0;
}

static bool qd_load_rom_file(const char* path) {
    qd_free_rom();
    if (!path) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "rom open failed path=%s errno=%d %s", path, errno, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long size = ftell(file);
    if (size <= 16 || size > 2 * 1024 * 1024) {
        fclose(file);
        ESP_LOGE(TAG, "rom size invalid path=%s size=%ld", path, size);
        return false;
    }
    rewind(file);

    s_rom_data = heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_rom_data) {
        fclose(file);
        ESP_LOGE(TAG, "rom alloc failed size=%ld", size);
        return false;
    }
    size_t read = fread(s_rom_data, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        ESP_LOGE(TAG, "rom read incomplete read=%u size=%ld", (unsigned)read, size);
        qd_free_rom();
        return false;
    }

    s_rom_size = (size_t)size;
    strlcpy(s_rom_path, path, sizeof(s_rom_path));
    ESP_LOGI(TAG, "rom loaded to psram size=%u path=%s", (unsigned)s_rom_size, path);
    return true;
}

char* osd_getromdata(void) {
    return (char*)s_rom_data;
}

static bool qd_config_open(void) {
    return false;
}

static void qd_config_close(void) {
}

static int qd_config_read_int(const char* group, const char* key, int def) {
    (void)group;
    (void)key;
    return def;
}

static const char* qd_config_read_string(const char* group, const char* key, const char* def) {
    (void)group;
    (void)key;
    return def;
}

static void qd_config_write_int(const char* group, const char* key, int value) {
    (void)group;
    (void)key;
    (void)value;
}

static void qd_config_write_string(const char* group, const char* key, const char* value) {
    (void)group;
    (void)key;
    (void)value;
}

config_t config = {
    .open = qd_config_open,
    .close = qd_config_close,
    .read_int = qd_config_read_int,
    .read_string = qd_config_read_string,
    .write_int = qd_config_write_int,
    .write_string = qd_config_write_string,
    .filename = NULL,
};

static int qd_video_init(int width, int height) {
    const size_t pixels = (size_t)width * (size_t)height;
    if (pixels == 0) {
        return -1;
    }
    if (s_hw_bitmap) {
        bmp_destroy(&s_hw_bitmap);
    }
    if (!s_hw_pixels || s_hw_pixel_count < pixels) {
        if (s_hw_pixels) {
            heap_caps_free(s_hw_pixels);
            s_hw_pixels = NULL;
        }
        s_hw_pixels = heap_caps_malloc(pixels, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_hw_pixels) {
            s_hw_pixel_count = 0;
            ESP_LOGE(TAG, "video hw bitmap alloc failed pixels=%u", (unsigned)pixels);
            return -1;
        }
        s_hw_pixel_count = pixels;
    }
    s_hw_bitmap = bmp_createhw(s_hw_pixels, width, height, width);
    if (!s_hw_bitmap) {
        ESP_LOGE(TAG, "video hw bitmap create failed %dx%d", width, height);
        return -1;
    }
    return 0;
}

static void qd_video_shutdown(void) {
    if (s_hw_bitmap) {
        bmp_destroy(&s_hw_bitmap);
    }
    if (s_hw_pixels) {
        heap_caps_free(s_hw_pixels);
        s_hw_pixels = NULL;
    }
    s_hw_pixel_count = 0;
}

static int qd_video_set_mode(int width, int height) {
    return qd_video_init(width, height);
}

static void qd_video_set_palette(rgb_t* pal) {
    for (int i = 0; i < 256; ++i) {
        const uint16_t r = (uint16_t)(pal[i].r >> 3);
        const uint16_t g = (uint16_t)(pal[i].g >> 2);
        const uint16_t b = (uint16_t)(pal[i].b >> 3);
        uint16_t color = (uint16_t)((r << 11) | (g << 5) | b);
        s_palette[i] = (uint16_t)((color << 8) | (color >> 8));
    }
}

static void qd_video_clear(uint8 color) {
    (void)color;
}

static bitmap_t* qd_video_lock_write(void) {
    return s_hw_bitmap;
}

static void qd_video_free_write(int num_dirties, rect_t* dirty_rects) {
    (void)num_dirties;
    (void)dirty_rects;
}

static void qd_video_custom_blit(bitmap_t* bmp, int num_dirties, rect_t* dirty_rects) {
    (void)num_dirties;
    (void)dirty_rects;
    if (!bmp || !s_frame_cb) {
        return;
    }

    const size_t pixels = (size_t)bmp->width * (size_t)bmp->height;
    if (pixels == 0) {
        return;
    }
    if (!s_rgb_frame || s_rgb_frame_pixels < pixels) {
        if (s_rgb_frame) {
            heap_caps_free(s_rgb_frame);
        }
        s_rgb_frame = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_rgb_frame) {
            s_rgb_frame_pixels = 0;
            ESP_LOGE(TAG, "rgb frame alloc failed pixels=%u", (unsigned)pixels);
            return;
        }
        s_rgb_frame_pixels = pixels;
        ESP_LOGI(TAG, "rgb frame buffer ready %dx%d pixels=%u",
                 bmp->width, bmp->height, (unsigned)pixels);
    }

    for (int y = 0; y < bmp->height; ++y) {
        uint8_t* src = bmp->line[y];
        uint16_t* dst = s_rgb_frame + (size_t)y * bmp->width;
        for (int x = 0; x < bmp->width; ++x) {
            dst[x] = s_palette[src[x]];
        }
    }
    s_frame_cb(s_rgb_frame, (uint16_t)bmp->width, (uint16_t)bmp->height, s_frame_user);
}

static void qd_audio_tick(void) {
    if (!s_sound_playfunc || !s_audio_cb || s_stop_requested) {
        return;
    }

    s_sound_playfunc(s_audio_buffer, QD_AUDIO_SAMPLES_PER_TICK);
    s_audio_cb(s_audio_buffer, QD_AUDIO_SAMPLES_PER_TICK, QD_AUDIO_SAMPLE_RATE, s_audio_user);

    ++s_audio_frames;
    const int64_t now = esp_timer_get_time();
    if (s_audio_last_log_us == 0) {
        s_audio_last_log_us = now;
    } else if (now - s_audio_last_log_us >= 2000000) {
        ESP_LOGI(TAG, "audio frames=%lu samples=%d rate=%d",
                 (unsigned long)s_audio_frames, QD_AUDIO_SAMPLES_PER_TICK, QD_AUDIO_SAMPLE_RATE);
        s_audio_frames = 0;
        s_audio_last_log_us = now;
    }
}

static viddriver_t qd_video_driver = {
    "QDTech direct RGB565",
    qd_video_init,
    qd_video_shutdown,
    qd_video_set_mode,
    qd_video_set_palette,
    qd_video_clear,
    qd_video_lock_write,
    qd_video_free_write,
    qd_video_custom_blit,
    false,
};

int osd_installtimer(int frequency, void* func, int funcsize, void* counter, int countersize) {
    (void)func;
    (void)funcsize;
    (void)counter;
    (void)countersize;
    if (s_tick_timer) {
        esp_timer_stop(s_tick_timer);
        esp_timer_delete(s_tick_timer);
        s_tick_timer = NULL;
    }
    nofrendo_ticks = 0;
    const esp_timer_create_args_t args = {
        .callback = qd_tick_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nof_tick",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &s_tick_timer) != ESP_OK) {
        return -1;
    }
    const uint64_t period_us = 1000000ULL / (uint64_t)(frequency > 0 ? frequency : 60);
    return esp_timer_start_periodic(s_tick_timer, period_us) == ESP_OK ? 0 : -1;
}

void osd_setsound(void (*playfunc)(void* buffer, int length)) {
    s_sound_playfunc = playfunc;
    s_audio_frames = 0;
    s_audio_last_log_us = 0;
}

void osd_getsoundinfo(sndinfo_t* info) {
    info->sample_rate = QD_AUDIO_SAMPLE_RATE;
    info->bps = 16;
}

void osd_getvideoinfo(vidinfo_t* info) {
    info->default_width = NES_SCREEN_WIDTH;
    info->default_height = NES_SCREEN_HEIGHT;
    info->driver = &qd_video_driver;
}

int osd_init(void) {
    s_stop_requested = false;
    s_controller = 0;
    return 0;
}

void osd_togglefullscreen(int code) {
    (void)code;
}

static void qd_emit_button(event_t event, bool pressed) {
    if (event) {
        event(pressed ? INP_STATE_MAKE : INP_STATE_BREAK);
    }
}

void osd_getinput(void) {
    static uint8_t old_controller = 0xff;
    if (s_stop_requested) {
        main_quit();
        return;
    }

    qd_audio_tick();

    const uint8_t controller = s_controller;
    const uint8_t changed = (uint8_t)(controller ^ old_controller);
    if (changed == 0) {
        return;
    }
    old_controller = controller;

    struct ButtonMap {
        uint8_t mask;
        int event_id;
    };
    static const struct ButtonMap kMap[] = {
        {0x01, event_joypad1_a},
        {0x02, event_joypad1_b},
        {0x04, event_joypad1_select},
        {0x08, event_joypad1_start},
        {0x10, event_joypad1_up},
        {0x20, event_joypad1_down},
        {0x40, event_joypad1_left},
        {0x80, event_joypad1_right},
    };

    for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i) {
        if (changed & kMap[i].mask) {
            qd_emit_button(event_get(kMap[i].event_id), (controller & kMap[i].mask) != 0);
        }
    }
}

void osd_getmouse(int* x, int* y, int* button) {
    if (x) *x = 0;
    if (y) *y = 0;
    if (button) *button = 0;
}

void osd_fullname(char* fullname, const char* shortname) {
    strlcpy(fullname, shortname ? shortname : s_rom_path, 256);
}

char* osd_newextension(char* string, char* ext) {
    char* dot = strrchr(string, '.');
    if (!dot) {
        dot = string + strlen(string);
    }
    strlcpy(dot, ext, 256 - (dot - string));
    return string;
}

int osd_makesnapname(char* filename, int len) {
    if (filename && len > 0) {
        filename[0] = '\0';
    }
    return -1;
}

void osd_shutdown(void) {
    s_sound_playfunc = NULL;
    if (s_tick_timer) {
        esp_timer_stop(s_tick_timer);
        esp_timer_delete(s_tick_timer);
        s_tick_timer = NULL;
    }
}

int osd_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return main_loop(s_rom_path, system_nes);
}

int qd_nofrendo_run(const char* rom_path) {
    if (!qd_load_rom_file(rom_path)) {
        return -1;
    }
    s_stop_requested = false;
    s_controller = 0;
    ESP_LOGI(TAG, "starting nofrendo");
    const int result = nofrendo_main(0, NULL);
    ESP_LOGI(TAG, "nofrendo stopped result=%d", result);
    qd_free_rom();
    return result;
}
