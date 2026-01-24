#pragma once
#include <string>

// https://sourceware.org/binutils/docs/as/Pseudo-Ops.html

namespace yoctocc {

enum class GasDirective {
    EXTERN,
    GLOBAL,
    TEXT,
    DATA,
    BSS,
    ZERO,
    BYTE,
    WORD,
    LONG,
    QUAD,
    ASCII,
    ASCIZ
};

constexpr std::string to_string(GasDirective directive) {
    using enum GasDirective;
    switch (directive) {
        case EXTERN: return ".extern";
        case GLOBAL: return ".globl";
        case TEXT: return ".text";
        case DATA: return ".data";
        case BSS: return ".bss";
        case ZERO: return ".zero";
        case BYTE: return ".byte";
        case WORD: return ".word";
        case LONG: return ".long";
        case QUAD: return ".quad";
        case ASCII: return ".ascii";
        case ASCIZ: return ".asciz";
        default: return "???";
    }
}

namespace directive {

    inline constexpr std::string extern_(const std::string& symbol) {
        return to_string(GasDirective::EXTERN) + " " + symbol;
    }

    inline constexpr std::string global(const std::string& symbol) {
        return to_string(GasDirective::GLOBAL) + " " + symbol;
    }

    inline constexpr std::string zero(size_t size) {
        return to_string(GasDirective::ZERO) + " " + std::to_string(size);
    }

    inline constexpr std::string byte(uint8_t value) {
        return to_string(GasDirective::BYTE) + " " + std::to_string(static_cast<uint32_t>(value));
    }

} // namespace directive

} // namespace yoctocc
