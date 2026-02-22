#pragma once
#include <string_view>
#include <unordered_map>

namespace yoctocc {

    enum class Keyword {
        CHAR,
        SHORT,
        INT,
        LONG,
        RETURN,
        IF,
        ELSE,
        FOR,
        WHILE,
        SIZEOF,
        STRUCT,
        UNION,
    };

    inline const std::unordered_map<std::string_view, Keyword> KEYWORDS = {
        { "char", Keyword::CHAR },
        { "short", Keyword::SHORT },
        { "int", Keyword::INT },
        { "long", Keyword::LONG },
        { "return", Keyword::RETURN },
        { "if", Keyword::IF },
        { "else", Keyword::ELSE },
        { "for", Keyword::FOR },
        { "while", Keyword::WHILE },
        { "sizeof", Keyword::SIZEOF },
        { "struct", Keyword::STRUCT },
        { "union", Keyword::UNION },
    };

} // namespace yoctocc
