#pragma once

#include "nes_cpu.h"
#include "nes_ppu.h"

#include <cstdint>
#include <cstddef>

class NesBus {
public:
    void Reset();
    void Step();

    uint8_t CpuRead(uint16_t addr);
    void CpuWrite(uint16_t addr, uint8_t val);

    uint8_t PpuRead(uint16_t addr);
    void PpuWrite(uint16_t addr, uint8_t val);

    bool LoadRom(const uint8_t* data, size_t size);
    void SetController(uint8_t controller);
    uint16_t MirrorVramAddr(uint16_t addr) const;

    bool FrameReady() const;
    void ClearFrameReady();
    const uint8_t* FrameBuffer() const;

    NesCpu& cpu() { return cpu_; }
    NesPpu& ppu() { return ppu_; }

private:
    NesCpu cpu_;
    NesPpu ppu_;

    uint8_t ram_[2048] = {};
    uint8_t* prg_rom_ = nullptr;
    uint8_t* chr_rom_ = nullptr;
    size_t prg_size_ = 0;
    size_t chr_size_ = 0;
    uint8_t mapper_ = 0;
    uint8_t mirroring_ = 0;
    bool chr_ram_ = false;

    uint8_t controller_ = 0;
    uint8_t controller_shift_ = 0;
    uint8_t controller2_shift_ = 0;
    uint8_t last_logged_controller_latch_ = 0;
    uint8_t last_logged_controller_read_ = 0;
    uint8_t controller_read_log_count_ = 0;
    bool controller_strobe_ = false;

    uint8_t prg_banks_[4] = {};
    uint8_t chr_banks_[8] = {};

    void SetupMapper();
    uint8_t Mapper0Read(uint16_t addr);
    void Mapper0Write(uint16_t addr, uint8_t val);
    uint8_t Mapper2Read(uint16_t addr);
    void Mapper2Write(uint16_t addr, uint8_t val);
};
