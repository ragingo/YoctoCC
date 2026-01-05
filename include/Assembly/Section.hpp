#pragma once
#include <string>

namespace yoctocc {

enum class Section {
    TEXT,
    DATA,
    BSS
};

constexpr std::string to_string(Section sec) {
    using enum Section;
    switch (sec) {
        case TEXT: return ".text";
        case DATA: return ".data";
        case BSS:  return ".bss";
        default: return "???";
    }
}

} // namespace yoctocc
