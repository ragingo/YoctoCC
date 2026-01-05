#pragma once
#include <format>
#include <string>
#include "Assembly/Register.hpp"
#include "String/String.hpp"

namespace yoctocc {

template <typename T>
struct Address {
    T base;
    int offset;

    constexpr Address(T base, int offset = 0) : base(base), offset(offset) {}

    constexpr Address operator+(int value) const {
        return Address{base, offset + value};
    }

    constexpr Address operator-(int value) const {
        return Address{base, offset - value};
    }
};

inline constexpr std::string to_string(Address<Register>&& addr) {
    if (addr.offset == 0) {
        return "[" + to_string(addr.base) + "]";
    } else if (addr.offset > 0) {
        return "[" + to_string(addr.base) + " + " + to_string(addr.offset) + "]";
    } else {
        return "[" + to_string(addr.base) + " - " + to_string(-addr.offset) + "]";
    }
}

namespace {
using enum Register;
static_assert(to_string(Address{RAX, 1}) == "[rax + 1]");
static_assert(to_string(Address{R8, -2}) == "[r8 - 2]");
}

} // namespace yoctocc

template <typename T>
struct std::formatter<yoctocc::Address<T>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const yoctocc::Address<T>& addr, std::format_context& ctx) const -> std::format_context::iterator {
        if (addr.offset == 0) {
            return std::format_to(ctx.out(), "[{}]", addr.base);
        } else if (addr.offset > 0) {
            return std::format_to(ctx.out(), "[{} + {}]", addr.base, addr.offset);
        } else {
            return std::format_to(ctx.out(), "[{} - {}]", addr.base, -addr.offset);
        }
    }
};
