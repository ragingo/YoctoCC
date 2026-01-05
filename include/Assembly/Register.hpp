#pragma once
#include <array>
#include <string>

namespace yoctocc {

enum class Register {
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    RBP,
    RSP,
    RIP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    EAX,
    EBX,
    ECX,
    EDX,
    AL,
    AH,
    AX,
    BL,
    BH,
    BX,
    CL,
    CH,
    CX,
    DL,
    DH,
    DX,
};

static constexpr std::array<Register, 6> ARG_REGISTERS = {
    Register::RDI,
    Register::RSI,
    Register::RDX,
    Register::RCX,
    Register::R8,
    Register::R9,
};

inline constexpr std::string to_string(Register reg) {
    using enum Register;
    switch (reg) {
        case RAX: return "rax";
        case RBX: return "rbx";
        case RCX: return "rcx";
        case RDX: return "rdx";
        case RSI: return "rsi";
        case RDI: return "rdi";
        case RBP: return "rbp";
        case RSP: return "rsp";
        case RIP: return "rip";
        case R8:  return "r8";
        case R9:  return "r9";
        case R10: return "r10";
        case R11: return "r11";
        case R12: return "r12";
        case R13: return "r13";
        case R14: return "r14";
        case R15: return "r15";
        case EAX: return "eax";
        case EBX: return "ebx";
        case ECX: return "ecx";
        case EDX: return "edx";
        case AL:  return "al";
        case AH:  return "ah";
        case AX:  return "ax";
        case BL:  return "bl";
        case BH:  return "bh";
        case BX:  return "bx";
        case CL:  return "cl";
        case CH:  return "ch";
        case CX:  return "cx";
        case DL:  return "dl";
        case DH:  return "dh";
        case DX:  return "dx";
        default:  return "???";
    }
}

} // namespace yoctocc
