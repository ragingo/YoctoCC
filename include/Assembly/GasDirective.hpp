#pragma once
#include <optional>
#include <string>

// https://sourceware.org/binutils/docs/as/Pseudo-Ops.html

namespace yoctocc {

enum class GasDirective {
    EXTERN,
    GLOBAL,
    LOCAL,
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
    FILE,
    ALIGN,
    INTEL_SYNTAX,
    SECTION,
};

constexpr std::string to_string(GasDirective directive) {
    using enum GasDirective;
    switch (directive) {
        case EXTERN:
            return ".extern";
        case GLOBAL:
            return ".globl";
        case LOCAL:
            return ".local";
        case TEXT:
            return ".text";
        case DATA:
            return ".data";
        case BSS:
            return ".bss";
        case ZERO:
            return ".zero";
        case BYTE:
            return ".byte";
        case WORD:
            return ".word";
        case LONG:
            return ".long";
        case QUAD:
            return ".quad";
        case ASCII:
            return ".ascii";
        case ASCIZ:
            return ".asciz";
        case LOC:
            return ".loc";
        case FILE:
            return ".file";
        case ALIGN:
            return ".align";
        case INTEL_SYNTAX:
            return ".intel_syntax";
        case SECTION:
            return ".section";
        default:
            return "???";
    }
}

namespace directive {
using enum GasDirective;

namespace sections {

inline constexpr std::string text = to_string(TEXT);
inline constexpr std::string data = to_string(DATA);
inline constexpr std::string bss = to_string(BSS);

} // namespace sections

inline constexpr std::string extern_(const std::string& symbol) {
    return to_string(EXTERN) + " " + symbol;
}

inline constexpr std::string global(const std::string& symbol) {
    return to_string(GLOBAL) + " " + symbol;
}

inline constexpr std::string local(const std::string& symbol) {
    return to_string(LOCAL) + " " + symbol;
}

inline constexpr std::string zero(size_t size) {
    return to_string(ZERO) + " " + to_string(size);
}

inline constexpr std::string byte(uint8_t value) {
    return to_string(BYTE) + " " + to_string(value);
}

inline constexpr std::string word(uint16_t value) {
    return to_string(WORD) + " " + to_string(value);
}

inline constexpr std::string long_(uint32_t value) {
    return to_string(LONG) + " " + to_string(value);
}

inline constexpr std::string quad(uint64_t value) {
    return to_string(QUAD) + " " + to_string(value);
}

template <typename T>
    requires std::is_integral_v<T>
inline constexpr std::string allocate(GasDirective directive, const std::string& symbol, T offset) {
    auto sign = offset >= 0 ? "+" : "-";
    return to_string(directive) + " " + symbol + sign + to_string(abs(offset));
}
static_assert(allocate(BYTE, "foo", 0) == ".byte foo+0");
static_assert(allocate(BYTE, "foo", 1) == ".byte foo+1");
static_assert(allocate(BYTE, "foo", -1) == ".byte foo-1");

inline constexpr std::string loc(int fileNumber, int line, std::optional<int> column = std::nullopt) {
    return to_string(LOC) + " " + to_string(fileNumber) + " " + to_string(line) + " " +
           (column ? to_string(*column) : "");
}
static_assert(loc(1, 2, 3) == ".loc 1 2 3");
static_assert(loc(1, 2, std::nullopt) == ".loc 1 2 ");

inline constexpr std::string file(int fileNumber, const std::string& filename) {
    return to_string(FILE) + " " + to_string(fileNumber) + " \"" + filename + "\"";
}

inline constexpr std::string align(int size) {
    return to_string(ALIGN) + " " + to_string(size);
}

inline constexpr std::string intelSyntax(bool prefix) {
    return to_string(INTEL_SYNTAX) + " " + (prefix ? " prefix" : " noprefix");
}

inline constexpr std::string section(const std::string& name, const std::string& flag, const std::string& type) {
    return to_string(SECTION) + " " + name + "," + flag + "," + type;
}

} // namespace directive

} // namespace yoctocc
