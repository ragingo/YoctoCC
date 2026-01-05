#pragma once
#include <string>

namespace yoctocc {

enum class LinkerDirective {
    EXTERN,
    GLOBAL
};

constexpr std::string to_string(LinkerDirective directive) {
    using enum LinkerDirective;
    switch (directive) {
        case EXTERN: return ".extern";
        case GLOBAL: return ".globl";
        default: return "???";
    }
}

} // namespace yoctocc
