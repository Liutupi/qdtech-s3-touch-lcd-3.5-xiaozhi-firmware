#include "nes_bus.h"
#include "nes_cpu.h"
#include "nes_ppu.h"

#include <cstring>
#include <esp_log.h>
#include <esp_heap_caps.h>

#define TAG "NesBus"

void NesBus::Reset() {
    memset(ram_, 0, sizeof(ram_));
    cpu_.Reset(this);
    ppu_.Reset(this);
}

void NesBus::Step() {
    const uint64_t cpu_before = cpu_.cycles();
    cpu_.Step();
    const uint32_t cpu_cycles = static_cast<uint32_t>(cpu_.cycles() - cpu_before);
    for (uint32_t i = 0; i < cpu_cycles * 3; ++i) {
        ppu_.Step();
    }

    if (ppu_.nmi_pending()) {
        ppu_.ClearNmi();
        const uint64_t nmi_before = cpu_.cycles();
        cpu_.Nmi();
        const uint32_t nmi_cycles = static_cast<uint32_t>(cpu_.cycles() - nmi_before);
        for (uint32_t i = 0; i < nmi_cycles * 3; ++i) {
            ppu_.Step();
        }
    }
}

void NesBus::SetController(uint8_t controller) {
    controller_ = controller;
    controller_shift_ = controller;
    controller2_shift_ = controller;
    if (controller == 0) {
        last_logged_controller_read_ = 0;
    }
}

bool NesBus::LoadRom(const uint8_t* data, size_t size) {
    if (size < 16) {
        ESP_LOGE(TAG, "ROM too small");
        return false;
    }

    if (memcmp(data, "NES\x1a", 4) != 0) {
        ESP_LOGE(TAG, "Invalid iNES header");
        return false;
    }

    if (prg_rom_) {
        heap_caps_free(prg_rom_);
        prg_rom_ = nullptr;
    }
    if (chr_rom_) {
        heap_caps_free(chr_rom_);
        chr_rom_ = nullptr;
    }

    uint8_t prg_banks = data[4];
    uint8_t chr_banks = data[5];
    mapper_ = (data[6] >> 4) | (data[7] & 0xF0);
    mirroring_ = data[6] & 0x01;
    chr_ram_ = chr_banks == 0;

    if (mapper_ != 0 && mapper_ != 2) {
        ESP_LOGE(TAG, "Unsupported mapper %u", mapper_);
        return false;
    }

    prg_size_ = prg_banks * 16384;
    chr_size_ = chr_banks * 8192;

    bool has_trainer = (data[6] & 0x04) != 0;
    size_t offset = 16 + (has_trainer ? 512 : 0);

    if (offset + prg_size_ > size) {
        ESP_LOGE(TAG, "ROM data too small for PRG");
        return false;
    }

    prg_rom_ = static_cast<uint8_t*>(heap_caps_malloc(prg_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!prg_rom_) {
        ESP_LOGE(TAG, "Failed to allocate PRG ROM");
        return false;
    }
    memcpy(prg_rom_, data + offset, prg_size_);

    if (chr_size_ > 0 && offset + prg_size_ + chr_size_ <= size) {
        chr_rom_ = static_cast<uint8_t*>(heap_caps_malloc(chr_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!chr_rom_) {
            ESP_LOGE(TAG, "Failed to allocate CHR ROM");
            return false;
        }
        memcpy(chr_rom_, data + offset + prg_size_, chr_size_);
    } else if (chr_size_ == 0) {
        chr_size_ = 8192;
        chr_rom_ = static_cast<uint8_t*>(heap_caps_malloc(chr_size_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (chr_rom_) {
            memset(chr_rom_, 0, chr_size_);
        } else {
            ESP_LOGE(TAG, "Failed to allocate CHR RAM");
            return false;
        }
    }

    SetupMapper();
    Reset();

    ESP_LOGI(TAG, "ROM loaded: mapper=%u prg=%uKB chr=%uKB mirroring=%u",
             mapper_, prg_banks * 16, chr_banks * 8, mirroring_);
    return true;
}

void NesBus::SetupMapper() {
    switch (mapper_) {
        case 0:
            prg_banks_[0] = 0;
            prg_banks_[1] = (prg_size_ > 16384) ? 1 : 0;
            break;
        case 2:
            prg_banks_[0] = 0;
            prg_banks_[1] = (prg_size_ / 16384) - 1;
            break;
        default:
            break;
    }

    for (int i = 0; i < 8; i++) {
        chr_banks_[i] = i;
    }
}

uint8_t NesBus::CpuRead(uint16_t addr) {
    if (addr < 0x2000) {
        return ram_[addr & 0x07FF];
    } else if (addr < 0x4000) {
        return ppu_.ReadRegister(addr);
    } else if (addr == 0x4016 || addr == 0x4017) {
        uint8_t& shift = (addr == 0x4016) ? controller_shift_ : controller2_shift_;
        uint8_t bit = 0;
        if (controller_strobe_) {
            bit = controller_ & 0x01;
        } else {
            bit = shift & 0x01;
            shift = (shift >> 1) | 0x80;
        }
        return bit | 0x40;
    } else if (addr >= 0x8000) {
        switch (mapper_) {
            case 0: return Mapper0Read(addr);
            case 2: return Mapper2Read(addr);
            default: return 0;
        }
    }
    return 0;
}

void NesBus::CpuWrite(uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        ram_[addr & 0x07FF] = val;
    } else if (addr < 0x4000) {
        ppu_.WriteRegister(addr, val);
    } else if (addr == 0x4014) {
        uint16_t base = val << 8;
        for (int i = 0; i < 256; i++) {
            ppu_.oam_[i] = CpuRead(base + i);
        }
    } else if (addr == 0x4016) {
        const bool strobe = (val & 0x01) != 0;
        if (strobe || (controller_strobe_ && !strobe)) {
            controller_shift_ = controller_;
            controller2_shift_ = controller_;
            if (controller_shift_ != 0 && controller_shift_ != last_logged_controller_latch_) {
                last_logged_controller_latch_ = controller_shift_;
                controller_read_log_count_ = 0;
                ESP_LOGI(TAG, "controller latch=0x%02x", controller_shift_);
            }
        }
        controller_strobe_ = strobe;
    } else if (addr >= 0x8000) {
        switch (mapper_) {
            case 2: Mapper2Write(addr, val); break;
            default: break;
        }
    }
}

uint8_t NesBus::PpuRead(uint16_t addr) {
    addr &= 0x1FFF;
    if (chr_rom_ && addr < chr_size_) {
        uint8_t bank = addr / 1024;
        uint16_t offset = addr & 0x3FF;
        return chr_rom_[chr_banks_[bank] * 1024 + offset];
    }
    return 0;
}

void NesBus::PpuWrite(uint16_t addr, uint8_t val) {
    addr &= 0x1FFF;
    if (chr_rom_ && chr_ram_ && addr < chr_size_) {
        uint8_t bank = addr / 1024;
        uint16_t offset = addr & 0x3FF;
        chr_rom_[chr_banks_[bank] * 1024 + offset] = val;
    }
}

uint16_t NesBus::MirrorVramAddr(uint16_t addr) const {
    const uint16_t mirrored = (addr - 0x2000) & 0x0FFF;
    const uint16_t table = mirrored / 0x0400;
    const uint16_t offset = mirrored & 0x03FF;
    if (mirroring_ == 0) {
        return ((table / 2) * 0x0400 + offset) & 0x07FF;
    }
    return ((table & 0x01) * 0x0400 + offset) & 0x07FF;
}

uint8_t NesBus::Mapper0Read(uint16_t addr) {
    if (prg_size_ <= 16384) {
        return prg_rom_[(addr - 0x8000) & (prg_size_ - 1)];
    } else {
        uint32_t rom_addr = (addr - 0x8000) % prg_size_;
        return prg_rom_[rom_addr];
    }
}

void NesBus::Mapper0Write(uint16_t addr, uint8_t val) {
    (void)addr;
    (void)val;
}

uint8_t NesBus::Mapper2Read(uint16_t addr) {
    uint32_t bank = (addr < 0xC000) ? prg_banks_[0] : prg_banks_[1];
    return prg_rom_[bank * 16384 + (addr & 0x3FFF)];
}

void NesBus::Mapper2Write(uint16_t addr, uint8_t val) {
    (void)addr;
    const uint8_t bank_count = prg_size_ / 16384;
    if (bank_count == 0) {
        return;
    }
    prg_banks_[0] = val % bank_count;
    prg_banks_[1] = bank_count - 1;
}

bool NesBus::FrameReady() const {
    return ppu_.FrameReady();
}

void NesBus::ClearFrameReady() {
    ppu_.ClearFrameReady();
}

const uint8_t* NesBus::FrameBuffer() const {
    return ppu_.FrameBuffer();
}
