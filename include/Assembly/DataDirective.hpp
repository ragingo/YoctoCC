#pragma once
#include <string>
#include "String/String.hpp"

namespace yoctocc {

enum class DataDirective {
    ZERO,
    BYTE,
    WORD,
    LONG,
    QUAD,
    ASCII,
    ASCIZ
};

constexpr std::string to_string(DataDirective directive) {
    using enum DataDirective;
    switch (directive) {
        case ZERO:  return ".zero";
        case BYTE:  return ".byte";
        case WORD:  return ".word";
        case LONG:  return ".long";
        case QUAD:  return ".quad";
        case ASCII: return ".ascii";
        case ASCIZ: return ".asciz";
        default:    return "???";
    }
}

namespace {
    using enum DataDirective;
}

namespace directive {
    static constexpr std::string zero(size_t size) {
        return to_string(ZERO) + " " + to_string(size);
    }
    static_assert(zero(16) == ".zero 16");
}

} // namespace yoctocc
