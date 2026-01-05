#pragma once
#include <cstdint>
#include <cmath>
#include <string>

namespace yoctocc {

constexpr bool isIdentifierChar(char ch, bool isFirstChar) {
    bool isAlpha = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    bool isUnderscore = (ch == '_');
    bool isNumber = (ch >= '0' && ch <= '9');

    if (isFirstChar) {
        return isAlpha || isUnderscore;
    } else {
        return isAlpha || isUnderscore || isNumber;
    }
}

constexpr int atoi(char ch) {
    return ch - '0';
}

constexpr char itoa(int value) {
    return static_cast<char>(value + '0');
}

template<std::integral T>
constexpr std::string to_string(T value) {
    if (value == 0) {
        return "0";
    }

    std::string result;
    bool isNegative = value < 0;
    auto uvalue = static_cast<std::make_unsigned_t<T>>(std::abs(value));

    while (uvalue > 0) {
        char digit = itoa(uvalue % 10);
        result.insert(result.begin(), digit);
        uvalue /= 10;
    }

    if (isNegative) {
        result.insert(result.begin(), '-');
    }

    return result;
}
static_assert(to_string(0) == "0");
static_assert(to_string(12345) == "12345");
static_assert(to_string(-6789) == "-6789");

} // namespace yoctocc
