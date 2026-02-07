#pragma once
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include "Token.hpp"

namespace yoctocc::Log {

inline std::string sourceFileName;
inline std::string sourceCode;

inline size_t getColumn(size_t location, size_t line) {
    size_t column = 0;
    size_t currentLine = 1;
    std::string::const_iterator it = sourceCode.cbegin();

    while (it != sourceCode.cend()) {
        if (std::next(it) == sourceCode.cend()) {
            break;
        }

        if (*it == '\n') {
            ++currentLine;
            if (currentLine > line) {
                break;
            }
        } else {
            size_t currentLocation = std::distance(sourceCode.cbegin(), it);
            if (currentLine == line && currentLocation <= location) {
                ++column;
            }
        }

        ++it;
    }

    return column;
}

struct SourceInfo {
    size_t location;
    size_t line;

    SourceInfo(size_t loc) : location(loc), line(0) {}
    SourceInfo(size_t loc, size_t line) : location(loc), line(line) {}
    SourceInfo(const yoctocc::Token* token) : location(token->location), line(token->line) {}
};

inline void error(std::string_view message, std::optional<SourceInfo> sourceInfo = std::nullopt, bool exit = true) {
    std::string formattedMessage;
    if (sourceInfo) {
        size_t column = getColumn(sourceInfo->location, sourceInfo->line);
        formattedMessage = std::format("\033[31mError at {} {}:{}: {}\033[0m", sourceFileName, sourceInfo->line, column, message);
    } else {
        formattedMessage = std::format("\033[31mError: {}\033[0m", message);
    }
    std::println(stderr, "{}", formattedMessage);
    if (exit) {
        std::exit(1);
    }
}

} // namespace yoctocc::Log
