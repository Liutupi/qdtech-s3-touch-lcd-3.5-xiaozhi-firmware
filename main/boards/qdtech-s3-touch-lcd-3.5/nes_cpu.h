#pragma once

#include <cstdint>
#include <cstddef>

class NesBus;

class NesCpu {
public:
    void Reset(NesBus* bus);
    void Step();
    void Nmi();
    void Irq();

    uint64_t cycles() const { return cycles_; }

private:
    NesBus* bus_ = nullptr;

    uint8_t a_ = 0;
    uint8_t x_ = 0;
    uint8_t y_ = 0;
    uint8_t sp_ = 0xFD;
    uint16_t pc_ = 0;
    uint8_t status_ = 0x24;

    uint64_t cycles_ = 0;

    uint8_t Read(uint16_t addr);
    void Write(uint16_t addr, uint8_t val);
    uint8_t Fetch();
    uint16_t Fetch16();

    void Push(uint8_t val);
    uint8_t Pull();
    void Push16(uint16_t val);
    uint16_t Pull16();

    void SetNZ(uint8_t val);
    void Branch(bool cond);

    uint16_t AddrImmediate();
    uint16_t AddrZeroPage();
    uint16_t AddrZeroPageX();
    uint16_t AddrZeroPageY();
    uint16_t AddrAbsolute();
    uint16_t AddrAbsoluteX(bool check_page = true);
    uint16_t AddrAbsoluteY(bool check_page = true);
    uint16_t AddrIndirect();
    uint16_t AddrIndirectX();
    uint16_t AddrIndirectY(bool check_page = true);
};
