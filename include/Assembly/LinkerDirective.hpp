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

namespace {
    using enum LinkerDirective;
}

namespace directive {
    static constexpr std::string global(const std::string& name) {
        return to_string(GLOBAL) + " " + name;
    }

    static constexpr std::string extern_(const std::string& name) {
        return to_string(EXTERN) + " " + name;
    }
}

} // namespace yoctocc
