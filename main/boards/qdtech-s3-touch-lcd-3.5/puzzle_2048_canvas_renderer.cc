#include "puzzle_2048_canvas_renderer.h"

#include <algorithm>
#include <cstdio>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);

namespace {

constexpr const char* kTag = "Puzzle2048Cache";
#if defined(CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING) && \
    CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING
constexpr int64_t kInputIntervalMs = 130;
#else
constexpr int64_t kInputIntervalMs = 90;
#endif

const lv_color_t kInk = lv_color_hex(0x403744);
const lv_color_t kMuted = lv_color_hex(0x715f70);
const lv_color_t kPurple = lv_color_hex(0x76508f);
const lv_color_t kGreen = lv_color_hex(0x47785f);
const lv_color_t kGold = lv_color_hex(0xb66f25);
const lv_color_t kPink = lv_color_hex(0xc95f7e);
const lv_color_t kPaper = lv_color_hex(0xfffbf7);

void DrawRect(lv_layer_t* layer, int x, int y, int w, int h,
              lv_color_t color, int radius = 5,
              lv_color_t border = lv_color_hex(0xd8c4ca),
              int border_width = 1) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = radius;
    dsc.border_color = border;
    dsc.border_width = border_width;
    dsc.border_opa = border_width > 0 ? LV_OPA_COVER : LV_OPA_TRANSP;
    lv_area_t area{x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &dsc, &area);
}

void DrawText(lv_layer_t* layer, int x, int y, int w, int h,
              const char* value, lv_color_t color, const lv_font_t* font,
              lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font = font;
    dsc.text = value;
    dsc.align = align;
    dsc.text_local = 1;
    lv_area_t area{x, y, x + w - 1, y + h - 1};
    lv_draw_label(layer, &dsc, &area);
}

void DrawButton(lv_layer_t* layer, int x, int y, int w, int h,
                const char* value, lv_color_t color,
                const lv_font_t* font) {
    const lv_color_t fill = lv_color_mix(color, kPaper, 84);
    DrawRect(layer, x, y, w, h, fill, 11, color, 2);
    DrawText(layer, x, y + 2, w, h - 2, value, kInk, font);
}

}  // namespace

bool Puzzle2048CanvasRenderer::Create(lv_obj_t* parent) {
    if (Active()) return true;
    if (!parent) return false;
    buffer_ = heap_caps_aligned_alloc(
        64, kBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer_) {
        ESP_LOGW(kTag, "PSRAM canvas allocation failed bytes=%u free=%u largest=%u",
                 static_cast<unsigned>(kBufferBytes),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
        return false;
    }
    canvas_ = lv_canvas_create(parent);
    if (!canvas_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
        return false;
    }
    lv_canvas_set_buffer(canvas_, buffer_, kWidth, kHeight,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas_, 0, 0);
    lv_obj_set_size(canvas_, kWidth, kHeight);
    lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(canvas_);
    ESP_LOGI(kTag,
             "created bytes=%u internal_free=%u internal_largest=%u psram_free=%u",
             static_cast<unsigned>(kBufferBytes),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    return true;
}

void Puzzle2048CanvasRenderer::Destroy() {
    if (!canvas_ && !buffer_) return;
    const uint32_t moves = move_count_;
    const uint32_t rejected = rejected_input_count_;
    if (canvas_) {
        lv_obj_delete(canvas_);
        canvas_ = nullptr;
    }
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
    rendering_ = false;
    last_render_finished_ms_ = -1000;
    move_count_ = 0;
    rejected_input_count_ = 0;
    ESP_LOGI(kTag,
             "released moves=%lu rejected=%lu internal_free=%u psram_free=%u",
             static_cast<unsigned long>(moves),
             static_cast<unsigned long>(rejected),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

bool Puzzle2048CanvasRenderer::AcceptInput(int64_t now_ms) {
    if (!Active() || rendering_ ||
        now_ms - last_render_finished_ms_ < kInputIntervalMs) {
        ++rejected_input_count_;
        return false;
    }
    return true;
}

void Puzzle2048CanvasRenderer::DrawTile(lv_layer_t* layer,
                                        int index, uint32_t value) {
    if (!layer || index < 0 || index >= 16) return;
    static constexpr uint32_t tile_colors[] = {
        0xffefe5, 0xf7dca8, 0xf4bd9c, 0xe995ad, 0xd7b5e9,
        0x9fcce0, 0x91c48b, 0xf0ae64, 0xdc7a79, 0x9263a5, 0x5e97aa
    };
    int shade = 0;
    for (uint32_t n = value; n > 2 && shade < 10; n >>= 1) ++shade;
    const int row = index / 4;
    const int col = index % 4;
    const int x = 18 + col * 66;
    const int y = 12 + row * 58;
    const lv_color_t color = lv_color_hex(tile_colors[std::min(shade, 10)]);
    DrawRect(layer, x, y, 58, 50, color, 13, kPaper, 2);
    if (value) {
        char label[12];
        snprintf(label, sizeof(label), "%lu", static_cast<unsigned long>(value));
        DrawText(layer, x, y + 12, 58, 32, label,
                 value >= 16 ? kPaper : kInk,
                 value >= 1024 ? &lv_font_montserrat_16 : &lv_font_montserrat_20);
    }
}

void Puzzle2048CanvasRenderer::RenderTiles(lv_layer_t* layer,
                                            const uint32_t cells[16],
                                            uint16_t changed_mask) {
    if (!Active() || !layer || !cells || changed_mask == 0) return;
    for (int i = 0; i < 16; ++i) {
        if ((changed_mask & (1U << i)) == 0) continue;
        DrawTile(layer, i, cells[i]);
    }
}

void Puzzle2048CanvasRenderer::RenderInfo(lv_layer_t* layer,
                                          uint32_t score,
                                          uint32_t best_tile,
                                          uint32_t high_score,
                                          bool won, bool game_over,
                                          const lv_font_t* chinese_font) {
    if (!Active() || !layer) return;
    // Only clear the two information rows.  The previous 9..63 rectangle
    // overlapped and erased the upper direction button at y=42..78.
    DrawRect(layer, 308, 7, 156, 48, kPaper, 0, kPaper, 0);
    char text[48];
    snprintf(text, sizeof(text), "得分 %lu", static_cast<unsigned long>(score));
    DrawText(layer, 314, 9, 148, 20, text, kInk, chinese_font);
    snprintf(text, sizeof(text), "纪录 %lu · %lu",
             static_cast<unsigned long>(high_score),
             static_cast<unsigned long>(best_tile));
    DrawText(layer, 314, 32, 148, 20, text, kMuted, chinese_font);
#if defined(CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING) && \
    CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING
    DrawRect(layer, 310, 222, 156, 28, kPaper, 0, kPaper, 0);
    constexpr int kStatusY = 226;
#else
    DrawRect(layer, 310, 190, 156, 30, kPaper, 0, kPaper, 0);
    constexpr int kStatusY = 194;
#endif
    if (game_over) {
        DrawText(layer, 314, kStatusY, 148, 24, "游戏结束", kPink, chinese_font);
    } else if (won) {
        DrawText(layer, 314, kStatusY, 148, 24, "2048 达成！", kGreen, chinese_font);
    }
}

void Puzzle2048CanvasRenderer::FinishRender(int64_t started_us,
                                            bool count_move) {
    last_render_us_ = static_cast<uint32_t>(esp_timer_get_time() - started_us);
    last_render_finished_ms_ = esp_timer_get_time() / 1000;
    rendering_ = false;
    if (!count_move) return;
    ++move_count_;
    if ((move_count_ & 31U) == 0U) {
        ESP_LOGI(kTag,
                 "moves=%lu rejected=%lu render_us=%lu internal_free=%u largest=%u minimum=%u psram_free=%u",
                 static_cast<unsigned long>(move_count_),
                 static_cast<unsigned long>(rejected_input_count_),
                 static_cast<unsigned long>(last_render_us_),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    }
}

void Puzzle2048CanvasRenderer::RenderFull(const uint32_t cells[16],
                                          uint32_t score,
                                          uint32_t best_tile,
                                          uint32_t high_score,
                                          bool won, bool game_over,
                                          const lv_font_t* chinese_font) {
    if (!Active() || !cells || !chinese_font) return;
    const int64_t started_us = esp_timer_get_time();
    rendering_ = true;
    lv_canvas_fill_bg(canvas_, kPaper, LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    DrawRect(&layer, 8, 2, 286, 250, lv_color_hex(0xfff1f3), 18, kPink, 2);
#if defined(CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING) && \
    CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING
    DrawRect(&layer, 304, 2, 168, 252, kPaper, 18, kGold, 2);
#else
    DrawRect(&layer, 304, 2, 168, 224, kPaper, 18, kGold, 2);
#endif
    DrawButton(&layer, 374, 62, 48, 36, "上", kPurple, chinese_font);
    DrawButton(&layer, 322, 104, 48, 36, "左", kPurple, chinese_font);
    DrawButton(&layer, 374, 104, 48, 36, "下", kPurple, chinese_font);
    DrawButton(&layer, 426, 104, 48, 36, "右", kPurple, chinese_font);
#if defined(CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING) && \
    CONFIG_QDTECH_EXPERIMENT_2048_INPUT_HARDENING
    DrawButton(&layer, 322, 180, 152, 36, "重新开始", kGold, chinese_font);
#else
    DrawButton(&layer, 322, 150, 152, 32, "重新开始", kGold, chinese_font);
#endif
    RenderTiles(&layer, cells, 0xffffU);
    RenderInfo(&layer, score, best_tile, high_score, won, game_over, chinese_font);
    lv_canvas_finish_layer(canvas_, &layer);
    FinishRender(started_us, false);
}

void Puzzle2048CanvasRenderer::RenderDelta(const uint32_t cells[16],
                                           uint16_t changed_mask,
                                           uint32_t score,
                                           uint32_t best_tile,
                                           uint32_t high_score,
                                           bool won, bool game_over,
                                           const lv_font_t* chinese_font) {
    if (!Active() || !cells || !chinese_font) return;
    const int64_t started_us = esp_timer_get_time();
    rendering_ = true;
    // One move owns exactly one canvas layer and one finish/invalidation.  A
    // canvas finish invalidates the complete 480x266 object, so finishing per
    // tile batch could enqueue several full-frame flushes per move and make a
    // long game progressively lag behind the touch stream.
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    RenderTiles(&layer, cells, changed_mask);
    RenderInfo(&layer, score, best_tile, high_score, won, game_over, chinese_font);
    lv_canvas_finish_layer(canvas_, &layer);
    FinishRender(started_us, true);
}
