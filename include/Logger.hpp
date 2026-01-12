#pragma once
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>

namespace yoctocc::Log {

inline std::string sourceFileName;
inline std::string sourceCode;

inline std::tuple<size_t, size_t> getLineAndColumn(size_t location) {
    size_t line = 1;
    size_t column = 1;
    for (auto it = sourceCode.cbegin(); it != sourceCode.cbegin() + location; ++it) {
        if (*it == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return { line, column };
}

inline void error(std::string_view message, std::optional<size_t> position = std::nullopt, bool exit = true) {
    std::string formattedMessage;
    if (position) {
        auto [line, column] = getLineAndColumn(*position);
        formattedMessage = std::format("\033[31mError at {} {}:{}: {}\033[0m", sourceFileName, line, column, message);
    } else {
        formattedMessage = std::format("\033[31mError: {}\033[0m", message);
    }
    std::println(stderr, "{}", formattedMessage);
    if (exit) {
        std::exit(1);
    }
}

} // namespace yoctocc::Log
