#pragma once

#include <cstdint>
#include <cstddef>

class NesBus;

class NesPpu {
public:
    void Reset(NesBus* bus);
    void Step();

    uint8_t ReadRegister(uint16_t addr);
    void WriteRegister(uint16_t addr, uint8_t val);

    bool nmi_pending() const { return nmi_pending_; }
    void ClearNmi() { nmi_pending_ = false; }

    const uint8_t* FrameBuffer() const { return framebuffer_; }
    bool FrameReady() const { return frame_ready_; }
    void ClearFrameReady() { frame_ready_ = false; }

    void SetController(uint8_t controller) { controller_ = controller; }

    uint8_t oam_[256] = {};

private:
    NesBus* bus_ = nullptr;

    uint8_t vram_[2048] = {};
    uint8_t palette_[32] = {};

    uint8_t* framebuffer_ = nullptr;

    uint16_t v_ = 0;
    uint16_t t_ = 0;
    uint8_t x_ = 0;
    uint8_t w_ = 0;

    uint8_t ctrl_ = 0;
    uint8_t mask_ = 0;
    uint8_t status_ = 0;
    uint8_t oam_addr_ = 0;

    uint8_t read_buffer_ = 0;

    int scanline_ = 0;
    int cycle_ = 0;
    bool frame_ready_ = false;
    bool nmi_pending_ = false;
    bool odd_frame_ = false;

    uint8_t controller_ = 0;

    uint16_t bg_pattern_lo_ = 0;
    uint16_t bg_pattern_hi_ = 0;
    uint16_t bg_attr_lo_ = 0;
    uint16_t bg_attr_hi_ = 0;
    uint8_t bg_next_attr_ = 0;

    uint8_t sprite_count_ = 0;
    uint8_t sprite_indices_[8] = {};
    uint8_t sprite_pattern_lo_[8] = {};
    uint8_t sprite_pattern_hi_[8] = {};
    uint8_t sprite_attrs_[8] = {};
    uint8_t sprite_x_counters_[8] = {};

    uint16_t PpuRead(uint16_t addr);
    void PpuWrite(uint16_t addr, uint8_t val);

    void RenderPixel();
    void FetchBackground();
    void FetchSprites();
    uint8_t GetPaletteColor(uint8_t index);
    uint16_t Rgb565(uint8_t r, uint8_t g, uint8_t b);

    void IncrementX();
    void IncrementY();
};
