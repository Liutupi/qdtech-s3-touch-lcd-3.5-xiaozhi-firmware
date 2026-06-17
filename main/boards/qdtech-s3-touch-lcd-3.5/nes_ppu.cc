#include "nes_ppu.h"
#include "nes_bus.h"

#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>

#define TAG "NesPpu"

static uint8_t ReverseByte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

static const uint16_t kNesPalette[64] = {
    0x4208, 0x900A, 0xA804, 0x8812, 0x4018, 0x1021, 0x0120, 0x012C,
    0x0138, 0x0020, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0xA514, 0xF98A, 0xF104, 0xD818, 0x8821, 0x3029, 0x0131, 0x09BD,
    0x0251, 0x02A0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xFDEA, 0xFCB0, 0xFC58, 0xFB9E, 0xA3BF, 0x53DF, 0x2D7F,
    0x15DF, 0x0EB0, 0x23B0, 0x4390, 0x838F, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xFF7F, 0xFEBF, 0xFDBF, 0xFCBF, 0xB5BF, 0x6DFF, 0x46FF,
    0x36FF, 0x37F0, 0x5FF0, 0x87D7, 0xC7D7, 0x0000, 0x0000, 0x0000,
};

uint16_t NesPpu::Rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void NesPpu::Reset(NesBus* bus) {
    bus_ = bus;
    if (!framebuffer_) {
        framebuffer_ = static_cast<uint8_t*>(heap_caps_malloc(256 * 240, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (framebuffer_) {
            ESP_LOGI(TAG, "framebuffer allocated in internal RAM");
        }
    }
    if (!framebuffer_) {
        framebuffer_ = static_cast<uint8_t*>(heap_caps_malloc(256 * 240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!framebuffer_) {
            ESP_LOGE(TAG, "framebuffer alloc failed");
        } else {
            ESP_LOGW(TAG, "framebuffer allocated in PSRAM fallback");
        }
    }
    v_ = t_ = x_ = w_ = 0;
    ctrl_ = mask_ = status_ = oam_addr_ = 0;
    read_buffer_ = 0;
    scanline_ = 0;
    cycle_ = 0;
    frame_ready_ = false;
    nmi_pending_ = false;
    odd_frame_ = false;
    memset(vram_, 0, sizeof(vram_));
    memset(oam_, 0, sizeof(oam_));
    memset(palette_, 0, sizeof(palette_));
    if (framebuffer_) {
        memset(framebuffer_, 0, 256 * 240);
    }
}

uint8_t NesPpu::ReadRegister(uint16_t addr) {
    switch (addr & 0x07) {
        case 0x02: {
            uint8_t val = status_;
            status_ &= ~0x80;
            w_ = 0;
            return val;
        }
        case 0x04:
            return oam_[oam_addr_];
        case 0x07: {
            uint8_t val = read_buffer_;
            read_buffer_ = PpuRead(v_);
            if ((v_ & 0x3FFF) >= 0x3F00) {
                val = GetPaletteColor(v_ & 0x1F);
            }
            v_ += (ctrl_ & 0x04) ? 32 : 1;
            return val;
        }
        default:
            return 0;
    }
}

void NesPpu::WriteRegister(uint16_t addr, uint8_t val) {
    switch (addr & 0x07) {
        case 0x00:
            ctrl_ = val;
            t_ = (t_ & 0xF3FF) | ((val & 0x03) << 10);
            break;
        case 0x01:
            mask_ = val;
            break;
        case 0x03:
            oam_addr_ = val;
            break;
        case 0x04:
            oam_[oam_addr_++] = val;
            break;
        case 0x05:
            if (w_ == 0) {
                t_ = (t_ & 0xFFE0) | (val >> 3);
                x_ = val & 0x07;
                w_ = 1;
            } else {
                t_ = (t_ & 0x8C1F) | ((val & 0xF8) << 2) | ((val & 0x07) << 12);
                w_ = 0;
            }
            break;
        case 0x06:
            if (w_ == 0) {
                t_ = (t_ & 0x80FF) | ((val & 0x3F) << 8);
                w_ = 1;
            } else {
                t_ = (t_ & 0xFF00) | val;
                v_ = t_;
                w_ = 0;
            }
            break;
        case 0x07:
            PpuWrite(v_, val);
            v_ += (ctrl_ & 0x04) ? 32 : 1;
            break;
    }
}

uint16_t NesPpu::PpuRead(uint16_t addr) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return bus_->PpuRead(addr);
    } else if (addr < 0x3F00) {
        return vram_[bus_->MirrorVramAddr(addr)];
    } else {
        return GetPaletteColor(addr & 0x1F);
    }
}

void NesPpu::PpuWrite(uint16_t addr, uint8_t val) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        bus_->PpuWrite(addr, val);
    } else if (addr < 0x3F00) {
        vram_[bus_->MirrorVramAddr(addr)] = val;
    } else {
        uint8_t idx = addr & 0x1F;
        if ((idx & 0x13) == 0x10) idx &= ~0x10;
        palette_[idx] = val;
    }
}

uint8_t NesPpu::GetPaletteColor(uint8_t index) {
    if ((index & 0x03) == 0) {
        index = 0;
    }
    return palette_[index & 0x1F] & 0x3F;
}

void NesPpu::IncrementX() {
    if ((v_ & 0x001F) == 0x001F) {
        v_ &= ~0x001F;
        v_ ^= 0x0400;
    } else {
        v_++;
    }
}

void NesPpu::IncrementY() {
    if ((v_ & 0x7000) != 0x7000) {
        v_ += 0x1000;
    } else {
        v_ &= ~0x7000;
        int y = (v_ & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            v_ ^= 0x0800;
        } else if (y == 31) {
            y = 0;
        } else {
            y++;
        }
        v_ = (v_ & ~0x03E0) | (y << 5);
    }
}

void NesPpu::FetchBackground() {
    uint16_t tile_addr = 0x2000 | (v_ & 0x0FFF);
    uint16_t attr_addr = 0x23C0 | (v_ & 0x0C00) | ((v_ >> 4) & 0x38) | ((v_ >> 2) & 0x07);

    uint16_t fine_y = (v_ >> 12) & 0x07;
    uint16_t pattern_base = (ctrl_ & 0x10) ? 0x1000 : 0x0000;
    uint16_t tile_num = PpuRead(tile_addr);

    uint16_t pattern_addr = pattern_base + tile_num * 16 + fine_y;
    bg_pattern_lo_ = PpuRead(pattern_addr);
    bg_pattern_hi_ = PpuRead(pattern_addr + 8);

    uint8_t attr = PpuRead(attr_addr);
    uint8_t shift = ((v_ >> 4) & 0x04) | (v_ & 0x02);
    bg_next_attr_ = ((attr >> shift) & 0x03) << 2;
}

void NesPpu::FetchSprites() {
    sprite_count_ = 0;
    uint16_t pattern_base = (ctrl_ & 0x08) ? 0x1000 : 0x0000;

    for (int i = 0; i < 64 && sprite_count_ < 8; i++) {
        uint8_t y = oam_[i * 4];
        int16_t diff = scanline_ - y;
        uint8_t sprite_height = (ctrl_ & 0x20) ? 16 : 8;

        if (diff >= 0 && diff < sprite_height) {
            sprite_indices_[sprite_count_] = i;
            uint8_t tile = oam_[i * 4 + 1];
            uint8_t attr = oam_[i * 4 + 2];
            sprite_attrs_[sprite_count_] = attr;
            sprite_x_counters_[sprite_count_] = oam_[i * 4 + 3];

            uint16_t pattern_addr;
            if (sprite_height == 16) {
                pattern_addr = ((tile & 0x01) ? 0x1000 : 0x0000) | ((tile & 0xFE) * 16);
                if (attr & 0x80) diff = 15 - diff;
                if (diff >= 8) {
                    pattern_addr += 16;
                    diff -= 8;
                }
            } else {
                pattern_addr = pattern_base + tile * 16;
                if (attr & 0x80) diff = 7 - diff;
            }

            pattern_addr += diff;
            uint8_t lo = PpuRead(pattern_addr);
            uint8_t hi = PpuRead(pattern_addr + 8);

            if (attr & 0x40) {
                lo = ReverseByte(lo);
                hi = ReverseByte(hi);
            }

            sprite_patterns_[sprite_count_] = lo | hi;
            sprite_count_++;
        }
    }
}

void NesPpu::RenderPixel() {
    if (cycle_ == 0 || cycle_ > 256 || scanline_ >= 240) return;

    int x = cycle_ - 1;
    int y = scanline_;
    uint8_t color_index = 0;

    if (mask_ & 0x08) {
        const uint8_t bit = 7 - ((x + x_) & 0x07);
        uint8_t p0 = (bg_pattern_lo_ >> bit) & 0x01;
        uint8_t p1 = ((bg_pattern_hi_ >> bit) & 0x01) << 1;
        uint8_t bg_pixel = p0 | p1;

        if (bg_pixel != 0) {
            color_index = bg_next_attr_ | bg_pixel;
        }
    }

    if (mask_ & 0x10) {
        for (int i = 0; i < sprite_count_; i++) {
            int16_t sx = sprite_x_counters_[i] - x;
            if (sx >= 0 && sx < 8) {
                uint8_t shift = 7 - sx;
                uint8_t pixel = (sprite_patterns_[i] >> shift) & 0x03;
                if (pixel != 0) {
                    if (i == 0 && (mask_ & 0x08) && color_index != 0 && x < 255) {
                        status_ |= 0x40;
                    }
                    if (!(sprite_attrs_[i] & 0x20) || color_index == 0) {
                        color_index = ((sprite_attrs_[i] & 0x03) << 2) | pixel | 0x10;
                    }
                    break;
                }
            }
        }
    }

    if (framebuffer_ && x < 256 && y < 240) {
        uint8_t pal_idx = GetPaletteColor(color_index);
        framebuffer_[y * 256 + x] = pal_idx & 0x3F;
    }
}

void NesPpu::Step() {
    bool rendering = (mask_ & 0x18) != 0;

    if (scanline_ < 240) {
        if (rendering) {
            if (cycle_ > 0 && cycle_ <= 256) {
                RenderPixel();
                if (cycle_ % 8 == 0) {
                    FetchBackground();
                    IncrementX();
                    if (cycle_ == 256) {
                        IncrementY();
                    }
                }
            }
            if (cycle_ == 257) {
                v_ = (v_ & ~0x041F) | (t_ & 0x041F);
                FetchSprites();
            }
        }
    } else if (scanline_ == 241) {
        if (cycle_ == 1) {
            status_ |= 0x80;
            if (ctrl_ & 0x80) {
                nmi_pending_ = true;
            }
            frame_ready_ = true;
        }
    } else if (scanline_ == 261) {
        if (cycle_ == 1) {
            status_ &= ~0xE0;
        }
        if (rendering && cycle_ >= 280 && cycle_ <= 304) {
            v_ = (v_ & ~0x7BE0) | (t_ & 0x7BE0);
        }
    }

    cycle_++;
    if (cycle_ > 340) {
        cycle_ = 0;
        scanline_++;
        if (scanline_ > 261) {
            scanline_ = 0;
            odd_frame_ = !odd_frame_;
        }
    }

    if (rendering && odd_frame_ && scanline_ == 261 && cycle_ == 339) {
        cycle_ = 0;
        scanline_ = 0;
        odd_frame_ = !odd_frame_;
    }
}
