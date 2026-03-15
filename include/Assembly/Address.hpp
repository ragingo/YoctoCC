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

// Intel syntax サイズ修飾子付きアドレス (movsx 等で必要)
inline constexpr std::string byte_ptr(Address<Register>&& addr) {
    return "BYTE PTR " + to_string(std::move(addr));
}

inline constexpr std::string word_ptr(Address<Register>&& addr) {
    return "WORD PTR " + to_string(std::move(addr));
}

inline constexpr std::string dword_ptr(Address<Register>&& addr) {
    return "DWORD PTR " + to_string(std::move(addr));
}

// RIP相対アドレッシング用 (グローバル変数用)
struct RipRelativeAddress {
    std::string symbol;

    constexpr explicit RipRelativeAddress(std::string_view sym) : symbol(sym) {}
};

inline constexpr std::string to_string(RipRelativeAddress&& addr) {
    return "[rip + " + addr.symbol + "]";
}

namespace {
using enum Register;
static_assert(to_string(Address{RAX, 1}) == "[rax + 1]");
static_assert(to_string(Address{R8, -2}) == "[r8 - 2]");
static_assert(to_string(RipRelativeAddress{"symbol"}) == "[rip + symbol]");
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
