#pragma once
#include <optional>
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
    ASCIZ,
    LOC,
    FILE
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
        case LOC: return ".loc";
        case FILE: return ".file";
        default: return "???";
    }
}

namespace directive {
    using enum GasDirective;

    inline constexpr std::string extern_(const std::string& symbol) {
        return to_string(EXTERN) + " " + symbol;
    }

    inline constexpr std::string global(const std::string& symbol) {
        return to_string(GLOBAL) + " " + symbol;
    }

    inline constexpr std::string zero(size_t size) {
        return to_string(ZERO) + " " + to_string(size);
    }

    inline constexpr std::string byte(uint8_t value) {
        return to_string(BYTE) + " " + to_string(static_cast<uint32_t>(value));
    }

    inline constexpr std::string loc(int fileNumber, int line, std::optional<int> column = std::nullopt) {
        return to_string(LOC) + " " + to_string(fileNumber) + " " + to_string(line) + " " + (column ? to_string(*column) : "");
    }
    static_assert(directive::loc(1, 2, 3) == ".loc 1 2 3");
    static_assert(directive::loc(1, 2, std::nullopt) == ".loc 1 2 ");

    inline constexpr std::string file(int fileNumber, const std::string& filename) {
        return to_string(FILE) + " " + to_string(fileNumber) + " \"" + filename + "\"";
    }

} // namespace directive

} // namespace yoctocc
