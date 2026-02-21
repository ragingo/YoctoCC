#pragma once
#include <array>
#include <string>

namespace yoctocc {

enum class Register {
    // 64ビット
    RAX, RBX, RCX, RDX,
    RSI, RDI, RBP, RSP,
    R8, R9, R10, R11,
    R12, R13, R14, R15,

    // 32ビット
    EAX, EBX, ECX, EDX,
    ESI, EDI, EBP, ESP,
    R8D, R9D, R10D, R11D,
    R12D, R13D, R14D, R15D,

    // 16ビット
    AX, BX, CX, DX,
    SI, DI, BP, SP,
    R8W, R9W, R10W, R11W,
    R12W, R13W, R14W, R15W,

    // 下位8ビット
    AL, BL, CL, DL,
    SIL, DIL, BPL, SPL,
    R8B, R9B, R10B, R11B,
    R12B, R13B, R14B, R15B,

    // 上位8ビット (legacy)
    AH, BH, CH, DH,

    // 特殊レジスタ
    RIP, EIP
};

namespace {
    using enum Register;
}

static constexpr std::array ARG_REGISTERS8 = {
    DIL,
    SIL,
    DL,
    CL,
    R8B,
    R9B,
};

static constexpr std::array ARG_REGISTERS32 = {
    EDI,
    ESI,
    EDX,
    ECX,
    R8D,
    R9D,
};

static constexpr std::array ARG_REGISTERS64 = {
    RDI,
    RSI,
    RDX,
    RCX,
    R8,
    R9,
};

inline constexpr std::string to_string(Register reg) {
    switch (reg) {
        case RAX: return "rax";
        case RBX: return "rbx";
        case RCX: return "rcx";
        case RDX: return "rdx";
        case RSI: return "rsi";
        case RDI: return "rdi";
        case RBP: return "rbp";
        case RSP: return "rsp";
        case R8: return "r8";
        case R9: return "r9";
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
        case ESI: return "esi";
        case EDI: return "edi";
        case EBP: return "ebp";
        case ESP: return "esp";
        case R8D: return "r8d";
        case R9D: return "r9d";
        case R10D: return "r10d";
        case R11D: return "r11d";
        case R12D: return "r12d";
        case R13D: return "r13d";
        case R14D: return "r14d";
        case R15D: return "r15d";
        case AX: return "ax";
        case BX: return "bx";
        case CX: return "cx";
        case DX: return "dx";
        case SI: return "si";
        case DI: return "di";
        case BP: return "bp";
        case SP: return "sp";
        case R8W: return "r8w";
        case R9W: return "r9w";
        case R10W: return "r10w";
        case R11W: return "r11w";
        case R12W: return "r12w";
        case R13W: return "r13w";
        case R14W: return "r14w";
        case R15W: return "r15w";
        case AL:  return "al";
        case BL: return "bl";
        case CL: return "cl";
        case DL: return "dl";
        case SIL: return "sil";
        case DIL: return "dil";
        case BPL: return "bpl";
        case SPL: return "spl";
        case R8B: return "r8b";
        case R9B: return "r9b";
        case R10B: return "r10b";
        case R11B: return "r11b";
        case R12B: return "r12b";
        case R13B: return "r13b";
        case R14B: return "r14b";
        case R15B: return "r15b";
        case AH: return "ah";
        case BH: return "bh";
        case CH: return "ch";
        case DH: return "dh";
        case RIP: return "rip";
        case EIP: return "eip";
        default: return "???";
    }
}

} // namespace yoctocc
