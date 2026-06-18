#include "nes_cpu.h"
#include "nes_bus.h"

#include <esp_log.h>

#define TAG "NesCpu"

#define C_FLAG 0x01
#define Z_FLAG 0x02
#define I_FLAG 0x04
#define D_FLAG 0x08
#define B_FLAG 0x10
#define V_FLAG 0x40
#define N_FLAG 0x80

namespace {
uint8_t BaseCycles(uint8_t op) {
    switch (op) {
        case 0x00: return 7;
        case 0x01: return 6;
        case 0x04: return 3;
        case 0x05: return 3;
        case 0x06: return 5;
        case 0x08: return 3;
        case 0x09: return 2;
        case 0x0A: return 2;
        case 0x0C: return 4;
        case 0x0D: return 4;
        case 0x0E: return 6;
        case 0x10: return 2;
        case 0x11: return 5;
        case 0x14: return 4;
        case 0x15: return 4;
        case 0x16: return 6;
        case 0x18: return 2;
        case 0x19: return 4;
        case 0x1A: return 2;
        case 0x1C: return 4;
        case 0x1D: return 4;
        case 0x1E: return 7;
        case 0x20: return 6;
        case 0x21: return 6;
        case 0x24: return 3;
        case 0x25: return 3;
        case 0x26: return 5;
        case 0x28: return 4;
        case 0x29: return 2;
        case 0x2A: return 2;
        case 0x2C: return 4;
        case 0x2D: return 4;
        case 0x2E: return 6;
        case 0x30: return 2;
        case 0x31: return 5;
        case 0x34: return 4;
        case 0x35: return 4;
        case 0x36: return 6;
        case 0x38: return 2;
        case 0x39: return 4;
        case 0x3A: return 2;
        case 0x3C: return 4;
        case 0x3D: return 4;
        case 0x3E: return 7;
        case 0x40: return 6;
        case 0x41: return 6;
        case 0x44: return 3;
        case 0x45: return 3;
        case 0x46: return 5;
        case 0x48: return 3;
        case 0x49: return 2;
        case 0x4A: return 2;
        case 0x4C: return 3;
        case 0x4D: return 4;
        case 0x4E: return 6;
        case 0x50: return 2;
        case 0x51: return 5;
        case 0x54: return 4;
        case 0x55: return 4;
        case 0x56: return 6;
        case 0x58: return 2;
        case 0x59: return 4;
        case 0x5A: return 2;
        case 0x5C: return 4;
        case 0x5D: return 4;
        case 0x5E: return 7;
        case 0x60: return 6;
        case 0x61: return 6;
        case 0x64: return 3;
        case 0x65: return 3;
        case 0x66: return 5;
        case 0x68: return 4;
        case 0x69: return 2;
        case 0x6A: return 2;
        case 0x6C: return 5;
        case 0x6D: return 4;
        case 0x6E: return 6;
        case 0x70: return 2;
        case 0x71: return 5;
        case 0x74: return 4;
        case 0x75: return 4;
        case 0x76: return 6;
        case 0x78: return 2;
        case 0x79: return 4;
        case 0x7A: return 2;
        case 0x7C: return 4;
        case 0x7D: return 4;
        case 0x7E: return 7;
        case 0x80: return 2;
        case 0x81: return 6;
        case 0x82: return 2;
        case 0x83: return 6;
        case 0x84: return 3;
        case 0x85: return 3;
        case 0x86: return 3;
        case 0x87: return 3;
        case 0x88: return 2;
        case 0x89: return 2;
        case 0x8A: return 2;
        case 0x8C: return 4;
        case 0x8D: return 4;
        case 0x8E: return 4;
        case 0x8F: return 4;
        case 0x90: return 2;
        case 0x91: return 6;
        case 0x94: return 4;
        case 0x95: return 4;
        case 0x96: return 4;
        case 0x97: return 4;
        case 0x98: return 2;
        case 0x99: return 5;
        case 0x9A: return 2;
        case 0x9D: return 5;
        case 0xA0: return 2;
        case 0xA1: return 6;
        case 0xA2: return 2;
        case 0xA3: return 6;
        case 0xA4: return 3;
        case 0xA5: return 3;
        case 0xA6: return 3;
        case 0xA7: return 3;
        case 0xA8: return 2;
        case 0xA9: return 2;
        case 0xAA: return 2;
        case 0xAC: return 4;
        case 0xAD: return 4;
        case 0xAE: return 4;
        case 0xAF: return 4;
        case 0xB0: return 2;
        case 0xB1: return 5;
        case 0xB3: return 5;
        case 0xB4: return 4;
        case 0xB5: return 4;
        case 0xB6: return 4;
        case 0xB7: return 4;
        case 0xB8: return 2;
        case 0xB9: return 4;
        case 0xBA: return 2;
        case 0xBC: return 4;
        case 0xBD: return 4;
        case 0xBE: return 4;
        case 0xBF: return 4;
        case 0xC0: return 2;
        case 0xC1: return 6;
        case 0xC2: return 2;
        case 0xC4: return 3;
        case 0xC5: return 3;
        case 0xC6: return 5;
        case 0xC8: return 2;
        case 0xC9: return 2;
        case 0xCA: return 2;
        case 0xCC: return 4;
        case 0xCD: return 4;
        case 0xCE: return 6;
        case 0xD0: return 2;
        case 0xD1: return 5;
        case 0xD4: return 4;
        case 0xD5: return 4;
        case 0xD6: return 6;
        case 0xD8: return 2;
        case 0xD9: return 4;
        case 0xDA: return 2;
        case 0xDC: return 4;
        case 0xDD: return 4;
        case 0xDE: return 7;
        case 0xE0: return 2;
        case 0xE1: return 6;
        case 0xE2: return 2;
        case 0xE4: return 3;
        case 0xE5: return 3;
        case 0xE6: return 5;
        case 0xE8: return 2;
        case 0xE9: return 2;
        case 0xEA: return 2;
        case 0xEB: return 2;
        case 0xEC: return 4;
        case 0xED: return 4;
        case 0xEE: return 6;
        case 0xF0: return 2;
        case 0xF1: return 5;
        case 0xF4: return 4;
        case 0xF5: return 4;
        case 0xF6: return 6;
        case 0xF8: return 2;
        case 0xF9: return 4;
        case 0xFA: return 2;
        case 0xFC: return 4;
        case 0xFD: return 4;
        case 0xFE: return 7;
        default: return 2;
    }
}
} // namespace

void NesCpu::Reset(NesBus* bus) {
    bus_ = bus;
    a_ = x_ = y_ = 0;
    sp_ = 0xFD;
    status_ = 0x24;
    cycles_ = 0;
    pc_ = Read(0xFFFC) | (Read(0xFFFD) << 8);
}

void NesCpu::Nmi() {
    Push16(pc_);
    Push(status_ & ~B_FLAG);
    status_ |= I_FLAG;
    pc_ = Read(0xFFFA) | (Read(0xFFFB) << 8);
    cycles_ += 7;
}

void NesCpu::Irq() {
    if (status_ & I_FLAG) return;
    Push16(pc_);
    Push(status_ & ~B_FLAG);
    status_ |= I_FLAG;
    pc_ = Read(0xFFFE) | (Read(0xFFFF) << 8);
    cycles_ += 7;
}

uint8_t NesCpu::Read(uint16_t addr) {
    return bus_->CpuRead(addr);
}

void NesCpu::Write(uint16_t addr, uint8_t val) {
    bus_->CpuWrite(addr, val);
}

uint8_t NesCpu::Fetch() {
    return Read(pc_++);
}

uint16_t NesCpu::Fetch16() {
    uint8_t lo = Fetch();
    uint8_t hi = Fetch();
    return lo | (hi << 8);
}

void NesCpu::Push(uint8_t val) {
    Write(0x100 + sp_--, val);
}

uint8_t NesCpu::Pull() {
    return Read(0x100 + ++sp_);
}

void NesCpu::Push16(uint16_t val) {
    Push(val >> 8);
    Push(val & 0xFF);
}

uint16_t NesCpu::Pull16() {
    uint8_t lo = Pull();
    uint8_t hi = Pull();
    return lo | (hi << 8);
}

void NesCpu::SetNZ(uint8_t val) {
    status_ = (status_ & ~(N_FLAG | Z_FLAG)) |
              (val & N_FLAG) |
              (val == 0 ? Z_FLAG : 0);
}

void NesCpu::Branch(bool cond) {
    int8_t offset = static_cast<int8_t>(Fetch());
    if (cond) {
        ++cycles_;
        uint16_t new_pc = pc_ + offset;
        if ((new_pc & 0xFF00) != (pc_ & 0xFF00)) ++cycles_;
        pc_ = new_pc;
    }
}

uint16_t NesCpu::AddrImmediate() { return pc_++; }
uint16_t NesCpu::AddrZeroPage() { return Fetch(); }
uint16_t NesCpu::AddrZeroPageX() { return (Fetch() + x_) & 0xFF; }
uint16_t NesCpu::AddrZeroPageY() { return (Fetch() + y_) & 0xFF; }
uint16_t NesCpu::AddrAbsolute() { return Fetch16(); }

uint16_t NesCpu::AddrAbsoluteX(bool check_page) {
    uint16_t base = Fetch16();
    uint16_t addr = base + x_;
    if (check_page && ((addr & 0xFF00) != (base & 0xFF00))) ++cycles_;
    return addr;
}

uint16_t NesCpu::AddrAbsoluteY(bool check_page) {
    uint16_t base = Fetch16();
    uint16_t addr = base + y_;
    if (check_page && ((addr & 0xFF00) != (base & 0xFF00))) ++cycles_;
    return addr;
}

uint16_t NesCpu::AddrIndirect() {
    uint16_t ptr = Fetch16();
    uint8_t lo = Read(ptr);
    uint8_t hi = Read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    return lo | (hi << 8);
}

uint16_t NesCpu::AddrIndirectX() {
    uint8_t ptr = (Fetch() + x_) & 0xFF;
    return Read(ptr) | (Read((ptr + 1) & 0xFF) << 8);
}

uint16_t NesCpu::AddrIndirectY(bool check_page) {
    uint8_t ptr = Fetch();
    uint16_t base = Read(ptr) | (Read((ptr + 1) & 0xFF) << 8);
    uint16_t addr = base + y_;
    if (check_page && ((addr & 0xFF00) != (base & 0xFF00))) ++cycles_;
    return addr;
}

void NesCpu::Step() {
    const uint16_t op_pc = pc_;
    uint8_t op = Fetch();
    cycles_ += BaseCycles(op);

    switch (op) {
        case 0xA9: { uint8_t v = Read(AddrImmediate()); a_ = v; SetNZ(a_); break; }
        case 0xA5: { uint8_t v = Read(AddrZeroPage()); a_ = v; SetNZ(a_); break; }
        case 0xB5: { uint8_t v = Read(AddrZeroPageX()); a_ = v; SetNZ(a_); break; }
        case 0xAD: { uint8_t v = Read(AddrAbsolute()); a_ = v; SetNZ(a_); break; }
        case 0xBD: { uint8_t v = Read(AddrAbsoluteX()); a_ = v; SetNZ(a_); break; }
        case 0xB9: { uint8_t v = Read(AddrAbsoluteY()); a_ = v; SetNZ(a_); break; }
        case 0xA1: { uint8_t v = Read(AddrIndirectX()); a_ = v; SetNZ(a_); break; }
        case 0xB1: { uint8_t v = Read(AddrIndirectY()); a_ = v; SetNZ(a_); break; }

        case 0xA2: { uint8_t v = Read(AddrImmediate()); x_ = v; SetNZ(x_); break; }
        case 0xA6: { uint8_t v = Read(AddrZeroPage()); x_ = v; SetNZ(x_); break; }
        case 0xB6: { uint8_t v = Read(AddrZeroPageY()); x_ = v; SetNZ(x_); break; }
        case 0xAE: { uint8_t v = Read(AddrAbsolute()); x_ = v; SetNZ(x_); break; }
        case 0xBE: { uint8_t v = Read(AddrAbsoluteY()); x_ = v; SetNZ(x_); break; }

        case 0xA0: { uint8_t v = Read(AddrImmediate()); y_ = v; SetNZ(y_); break; }
        case 0xA4: { uint8_t v = Read(AddrZeroPage()); y_ = v; SetNZ(y_); break; }
        case 0xB4: { uint8_t v = Read(AddrZeroPageX()); y_ = v; SetNZ(y_); break; }
        case 0xAC: { uint8_t v = Read(AddrAbsolute()); y_ = v; SetNZ(y_); break; }
        case 0xBC: { uint8_t v = Read(AddrAbsoluteX()); y_ = v; SetNZ(y_); break; }

        case 0x85: Write(AddrZeroPage(), a_); break;
        case 0x95: Write(AddrZeroPageX(), a_); break;
        case 0x8D: Write(AddrAbsolute(), a_); break;
        case 0x9D: Write(AddrAbsoluteX(false), a_); break;
        case 0x99: Write(AddrAbsoluteY(false), a_); break;
        case 0x81: Write(AddrIndirectX(), a_); break;
        case 0x91: Write(AddrIndirectY(false), a_); break;

        case 0x86: Write(AddrZeroPage(), x_); break;
        case 0x96: Write(AddrZeroPageY(), x_); break;
        case 0x8E: Write(AddrAbsolute(), x_); break;

        case 0x84: Write(AddrZeroPage(), y_); break;
        case 0x94: Write(AddrZeroPageX(), y_); break;
        case 0x8C: Write(AddrAbsolute(), y_); break;

        case 0xAA: x_ = a_; SetNZ(x_); break;
        case 0xA8: y_ = a_; SetNZ(y_); break;
        case 0x8A: a_ = x_; SetNZ(a_); break;
        case 0x98: a_ = y_; SetNZ(a_); break;
        case 0xBA: x_ = sp_; SetNZ(x_); break;
        case 0x9A: sp_ = x_; break;

        case 0x69: { uint8_t v = Read(AddrImmediate()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x65: { uint8_t v = Read(AddrZeroPage()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x75: { uint8_t v = Read(AddrZeroPageX()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x6D: { uint8_t v = Read(AddrAbsolute()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x7D: { uint8_t v = Read(AddrAbsoluteX()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x79: { uint8_t v = Read(AddrAbsoluteY()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x61: { uint8_t v = Read(AddrIndirectX()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0x71: { uint8_t v = Read(AddrIndirectY()); uint16_t r = a_ + v + (status_ & C_FLAG); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r > 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }

        case 0xE9: { uint8_t v = Read(AddrImmediate()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xE5: { uint8_t v = Read(AddrZeroPage()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xF5: { uint8_t v = Read(AddrZeroPageX()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xED: { uint8_t v = Read(AddrAbsolute()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xFD: { uint8_t v = Read(AddrAbsoluteX()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xF9: { uint8_t v = Read(AddrAbsoluteY()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xE1: { uint8_t v = Read(AddrIndirectX()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }
        case 0xF1: { uint8_t v = Read(AddrIndirectY()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }

        case 0x29: { uint8_t v = Read(AddrImmediate()); a_ &= v; SetNZ(a_); break; }
        case 0x25: { uint8_t v = Read(AddrZeroPage()); a_ &= v; SetNZ(a_); break; }
        case 0x35: { uint8_t v = Read(AddrZeroPageX()); a_ &= v; SetNZ(a_); break; }
        case 0x2D: { uint8_t v = Read(AddrAbsolute()); a_ &= v; SetNZ(a_); break; }
        case 0x3D: { uint8_t v = Read(AddrAbsoluteX()); a_ &= v; SetNZ(a_); break; }
        case 0x39: { uint8_t v = Read(AddrAbsoluteY()); a_ &= v; SetNZ(a_); break; }
        case 0x21: { uint8_t v = Read(AddrIndirectX()); a_ &= v; SetNZ(a_); break; }
        case 0x31: { uint8_t v = Read(AddrIndirectY()); a_ &= v; SetNZ(a_); break; }

        case 0x49: { uint8_t v = Read(AddrImmediate()); a_ ^= v; SetNZ(a_); break; }
        case 0x45: { uint8_t v = Read(AddrZeroPage()); a_ ^= v; SetNZ(a_); break; }
        case 0x55: { uint8_t v = Read(AddrZeroPageX()); a_ ^= v; SetNZ(a_); break; }
        case 0x4D: { uint8_t v = Read(AddrAbsolute()); a_ ^= v; SetNZ(a_); break; }
        case 0x5D: { uint8_t v = Read(AddrAbsoluteX()); a_ ^= v; SetNZ(a_); break; }
        case 0x59: { uint8_t v = Read(AddrAbsoluteY()); a_ ^= v; SetNZ(a_); break; }
        case 0x41: { uint8_t v = Read(AddrIndirectX()); a_ ^= v; SetNZ(a_); break; }
        case 0x51: { uint8_t v = Read(AddrIndirectY()); a_ ^= v; SetNZ(a_); break; }

        case 0x09: { uint8_t v = Read(AddrImmediate()); a_ |= v; SetNZ(a_); break; }
        case 0x05: { uint8_t v = Read(AddrZeroPage()); a_ |= v; SetNZ(a_); break; }
        case 0x15: { uint8_t v = Read(AddrZeroPageX()); a_ |= v; SetNZ(a_); break; }
        case 0x0D: { uint8_t v = Read(AddrAbsolute()); a_ |= v; SetNZ(a_); break; }
        case 0x1D: { uint8_t v = Read(AddrAbsoluteX()); a_ |= v; SetNZ(a_); break; }
        case 0x19: { uint8_t v = Read(AddrAbsoluteY()); a_ |= v; SetNZ(a_); break; }
        case 0x01: { uint8_t v = Read(AddrIndirectX()); a_ |= v; SetNZ(a_); break; }
        case 0x11: { uint8_t v = Read(AddrIndirectY()); a_ |= v; SetNZ(a_); break; }

        case 0xC9: { uint8_t v = Read(AddrImmediate()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xC5: { uint8_t v = Read(AddrZeroPage()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xD5: { uint8_t v = Read(AddrZeroPageX()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xCD: { uint8_t v = Read(AddrAbsolute()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xDD: { uint8_t v = Read(AddrAbsoluteX()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xD9: { uint8_t v = Read(AddrAbsoluteY()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xC1: { uint8_t v = Read(AddrIndirectX()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xD1: { uint8_t v = Read(AddrIndirectY()); uint16_t r = a_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (a_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }

        case 0xE0: { uint8_t v = Read(AddrImmediate()); uint16_t r = x_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (x_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xE4: { uint8_t v = Read(AddrZeroPage()); uint16_t r = x_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (x_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xEC: { uint8_t v = Read(AddrAbsolute()); uint16_t r = x_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (x_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }

        case 0xC0: { uint8_t v = Read(AddrImmediate()); uint16_t r = y_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (y_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xC4: { uint8_t v = Read(AddrZeroPage()); uint16_t r = y_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (y_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }
        case 0xCC: { uint8_t v = Read(AddrAbsolute()); uint16_t r = y_ - v; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (y_ >= v ? C_FLAG : 0) | ((r & 0x80) ? N_FLAG : 0) | ((r & 0xFF) == 0 ? Z_FLAG : 0); break; }

        case 0x0A: { uint8_t c = a_ & 0x80; a_ <<= 1; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(a_); break; }
        case 0x06: { uint16_t addr = AddrZeroPage(); uint8_t v = Read(addr); uint8_t c = v & 0x80; v <<= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }
        case 0x16: { uint16_t addr = AddrZeroPageX(); uint8_t v = Read(addr); uint8_t c = v & 0x80; v <<= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }
        case 0x0E: { uint16_t addr = AddrAbsolute(); uint8_t v = Read(addr); uint8_t c = v & 0x80; v <<= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }
        case 0x1E: { uint16_t addr = AddrAbsoluteX(false); uint8_t v = Read(addr); uint8_t c = v & 0x80; v <<= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }

        case 0x4A: { uint8_t c = a_ & 0x01; a_ >>= 1; status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(a_); break; }
        case 0x46: { uint16_t addr = AddrZeroPage(); uint8_t v = Read(addr); uint8_t c = v & 0x01; v >>= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }
        case 0x56: { uint16_t addr = AddrZeroPageX(); uint8_t v = Read(addr); uint8_t c = v & 0x01; v >>= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }
        case 0x4E: { uint16_t addr = AddrAbsolute(); uint8_t v = Read(addr); uint8_t c = v & 0x01; v >>= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }
        case 0x5E: { uint16_t addr = AddrAbsoluteX(false); uint8_t v = Read(addr); uint8_t c = v & 0x01; v >>= 1; Write(addr, v); status_ = (status_ & ~(C_FLAG | N_FLAG | Z_FLAG)) | (c ? C_FLAG : 0); SetNZ(v); break; }

        case 0x2A: { uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (a_ & 0x80 ? C_FLAG : 0); a_ = (a_ << 1) | c; SetNZ(a_); break; }
        case 0x26: { uint16_t addr = AddrZeroPage(); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x80 ? C_FLAG : 0); v = (v << 1) | c; Write(addr, v); SetNZ(v); break; }
        case 0x36: { uint16_t addr = AddrZeroPageX(); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x80 ? C_FLAG : 0); v = (v << 1) | c; Write(addr, v); SetNZ(v); break; }
        case 0x2E: { uint16_t addr = AddrAbsolute(); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x80 ? C_FLAG : 0); v = (v << 1) | c; Write(addr, v); SetNZ(v); break; }
        case 0x3E: { uint16_t addr = AddrAbsoluteX(false); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x80 ? C_FLAG : 0); v = (v << 1) | c; Write(addr, v); SetNZ(v); break; }

        case 0x6A: { uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (a_ & 0x01 ? C_FLAG : 0); a_ = (a_ >> 1) | (c << 7); SetNZ(a_); break; }
        case 0x66: { uint16_t addr = AddrZeroPage(); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x01 ? C_FLAG : 0); v = (v >> 1) | (c << 7); Write(addr, v); SetNZ(v); break; }
        case 0x76: { uint16_t addr = AddrZeroPageX(); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x01 ? C_FLAG : 0); v = (v >> 1) | (c << 7); Write(addr, v); SetNZ(v); break; }
        case 0x6E: { uint16_t addr = AddrAbsolute(); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x01 ? C_FLAG : 0); v = (v >> 1) | (c << 7); Write(addr, v); SetNZ(v); break; }
        case 0x7E: { uint16_t addr = AddrAbsoluteX(false); uint8_t v = Read(addr); uint8_t c = status_ & C_FLAG; status_ = (status_ & ~C_FLAG) | (v & 0x01 ? C_FLAG : 0); v = (v >> 1) | (c << 7); Write(addr, v); SetNZ(v); break; }

        case 0xE6: { uint16_t addr = AddrZeroPage(); uint8_t v = Read(addr) + 1; Write(addr, v); SetNZ(v); break; }
        case 0xF6: { uint16_t addr = AddrZeroPageX(); uint8_t v = Read(addr) + 1; Write(addr, v); SetNZ(v); break; }
        case 0xEE: { uint16_t addr = AddrAbsolute(); uint8_t v = Read(addr) + 1; Write(addr, v); SetNZ(v); break; }
        case 0xFE: { uint16_t addr = AddrAbsoluteX(false); uint8_t v = Read(addr) + 1; Write(addr, v); SetNZ(v); break; }
        case 0xE8: ++x_; SetNZ(x_); break;
        case 0xC8: ++y_; SetNZ(y_); break;

        case 0xC6: { uint16_t addr = AddrZeroPage(); uint8_t v = Read(addr) - 1; Write(addr, v); SetNZ(v); break; }
        case 0xD6: { uint16_t addr = AddrZeroPageX(); uint8_t v = Read(addr) - 1; Write(addr, v); SetNZ(v); break; }
        case 0xCE: { uint16_t addr = AddrAbsolute(); uint8_t v = Read(addr) - 1; Write(addr, v); SetNZ(v); break; }
        case 0xDE: { uint16_t addr = AddrAbsoluteX(false); uint8_t v = Read(addr) - 1; Write(addr, v); SetNZ(v); break; }
        case 0xCA: --x_; SetNZ(x_); break;
        case 0x88: --y_; SetNZ(y_); break;

        case 0x4C: pc_ = Fetch16(); break;
        case 0x6C: pc_ = AddrIndirect(); break;

        case 0x20: { uint16_t addr = Fetch16(); Push16(pc_ - 1); pc_ = addr; break; }
        case 0x60: pc_ = Pull16() + 1; break;

        case 0x48: Push(a_); break;
        case 0x08: Push(status_ | B_FLAG); break;
        case 0x68: a_ = Pull(); SetNZ(a_); break;
        case 0x28: status_ = Pull() | 0x20; break;

        case 0x90: Branch(!(status_ & C_FLAG)); break;
        case 0xB0: Branch(status_ & C_FLAG); break;
        case 0xF0: Branch(status_ & Z_FLAG); break;
        case 0xD0: Branch(!(status_ & Z_FLAG)); break;
        case 0x30: Branch(status_ & N_FLAG); break;
        case 0x10: Branch(!(status_ & N_FLAG)); break;
        case 0x50: Branch(!(status_ & V_FLAG)); break;
        case 0x70: Branch(status_ & V_FLAG); break;

        case 0x24: { uint8_t v = Read(AddrZeroPage()); status_ = (status_ & ~(N_FLAG | V_FLAG | Z_FLAG)) | (v & (N_FLAG | V_FLAG)) | ((v & a_) == 0 ? Z_FLAG : 0); break; }
        case 0x2C: { uint8_t v = Read(AddrAbsolute()); status_ = (status_ & ~(N_FLAG | V_FLAG | Z_FLAG)) | (v & (N_FLAG | V_FLAG)) | ((v & a_) == 0 ? Z_FLAG : 0); break; }

        case 0x18: status_ &= ~C_FLAG; break;
        case 0x38: status_ |= C_FLAG; break;
        case 0x58: status_ &= ~I_FLAG; break;
        case 0x78: status_ |= I_FLAG; break;
        case 0xD8: status_ &= ~D_FLAG; break;
        case 0xF8: status_ |= D_FLAG; break;
        case 0xB8: status_ &= ~V_FLAG; break;

        case 0xEA: break;

        case 0x00: {
            Fetch();
            Push16(pc_);
            Push(status_ | B_FLAG);
            status_ |= I_FLAG;
            pc_ = Read(0xFFFE) | (Read(0xFFFF) << 8);
            break;
        }

        case 0x40: status_ = (Pull() & ~B_FLAG) | 0x20; pc_ = Pull16(); break;

        case 0xA7: { uint8_t v = Read(AddrZeroPage()); a_ = x_ = v; SetNZ(a_); break; }
        case 0xB7: { uint8_t v = Read(AddrZeroPageY()); a_ = x_ = v; SetNZ(a_); break; }
        case 0xAF: { uint8_t v = Read(AddrAbsolute()); a_ = x_ = v; SetNZ(a_); break; }
        case 0xBF: { uint8_t v = Read(AddrAbsoluteY()); a_ = x_ = v; SetNZ(a_); break; }
        case 0xA3: { uint8_t v = Read(AddrIndirectX()); a_ = x_ = v; SetNZ(a_); break; }
        case 0xB3: { uint8_t v = Read(AddrIndirectY()); a_ = x_ = v; SetNZ(a_); break; }

        case 0x87: Write(AddrZeroPage(), a_ & x_); break;
        case 0x97: Write(AddrZeroPageY(), a_ & x_); break;
        case 0x8F: Write(AddrAbsolute(), a_ & x_); break;
        case 0x83: Write(AddrIndirectX(), a_ & x_); break;

        case 0xEB: { uint8_t v = Read(AddrImmediate()); uint16_t r = a_ - v - (1 - (status_ & C_FLAG)); status_ = (status_ & ~(C_FLAG | V_FLAG | N_FLAG | Z_FLAG)) | (r <= 0xFF ? C_FLAG : 0) | ((((a_ ^ r) & (~v ^ r)) & 0x80) ? V_FLAG : 0); a_ = r & 0xFF; SetNZ(a_); break; }

        case 0x1A: break;
        case 0x3A: break;
        case 0x5A: break;
        case 0x7A: break;
        case 0xDA: break;
        case 0xFA: break;
        case 0x80: pc_++; break;
        case 0x82: pc_++; break;
        case 0x89: pc_++; break;
        case 0xC2: pc_++; break;
        case 0xE2: pc_++; break;

        case 0x04: pc_++; break;
        case 0x14: pc_++; break;
        case 0x34: pc_++; break;
        case 0x44: pc_++; break;
        case 0x54: pc_++; break;
        case 0x64: pc_++; break;
        case 0x74: pc_++; break;
        case 0xD4: pc_++; break;
        case 0xF4: pc_++; break;

        case 0x0C: pc_ += 2; break;
        case 0x1C: { uint16_t addr = Fetch16(); (void)addr; break; }
        case 0x3C: { uint16_t addr = Fetch16(); (void)addr; break; }
        case 0x5C: { uint16_t addr = Fetch16(); (void)addr; break; }
        case 0x7C: { uint16_t addr = Fetch16(); (void)addr; break; }
        case 0xDC: { uint16_t addr = Fetch16(); (void)addr; break; }
        case 0xFC: { uint16_t addr = Fetch16(); (void)addr; break; }

        default: {
            static uint8_t unsupported_log_count = 0;
            if (unsupported_log_count < 16) {
                ESP_LOGW(TAG, "unsupported opcode=0x%02x pc=0x%04x", op, op_pc);
                ++unsupported_log_count;
            }
            break;
        }
    }
}
