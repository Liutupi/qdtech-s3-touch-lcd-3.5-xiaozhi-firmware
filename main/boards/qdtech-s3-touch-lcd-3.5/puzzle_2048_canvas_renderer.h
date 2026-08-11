#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

class Puzzle2048CanvasRenderer {
public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 266;
    static constexpr size_t kBufferBytes =
        static_cast<size_t>(kWidth) * kHeight * sizeof(uint16_t);

    bool Create(lv_obj_t* parent);
    void Destroy();
    bool Active() const { return canvas_ != nullptr && buffer_ != nullptr; }
    bool AcceptInput(int64_t now_ms);
    void RenderFull(const uint32_t cells[16], uint32_t score,
                    uint32_t best_tile, uint32_t high_score,
                    bool won, bool game_over,
                    const lv_font_t* chinese_font);
    void RenderDelta(const uint32_t cells[16], uint16_t changed_mask,
                     uint32_t score, uint32_t best_tile,
                     uint32_t high_score, bool won, bool game_over,
                     const lv_font_t* chinese_font);

private:
    void DrawTile(lv_layer_t* layer, int index, uint32_t value);
    void RenderTiles(lv_layer_t* layer, const uint32_t cells[16],
                     uint16_t changed_mask);
    void RenderInfo(lv_layer_t* layer, uint32_t score, uint32_t best_tile,
                    uint32_t high_score, bool won, bool game_over,
                    const lv_font_t* chinese_font);
    void FinishRender(int64_t started_us, bool count_move);

    lv_obj_t* canvas_ = nullptr;
    void* buffer_ = nullptr;
    bool rendering_ = false;
    int64_t last_render_finished_ms_ = -1000;
    uint32_t move_count_ = 0;
    uint32_t rejected_input_count_ = 0;
    uint32_t last_render_us_ = 0;
};
