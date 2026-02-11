#pragma once
#include <string_view>
#include <unordered_map>

namespace yoctocc {

    enum class Keyword {
        RETURN,
        IF,
        ELSE,
        FOR,
        WHILE,
        INT,
        CHAR,
        SIZEOF,
        STRUCT,
    };

    inline const std::unordered_map<std::string_view, Keyword> KEYWORDS = {
        { "return", Keyword::RETURN },
        { "if", Keyword::IF },
        { "else", Keyword::ELSE },
        { "for", Keyword::FOR },
        { "while", Keyword::WHILE },
        { "int", Keyword::INT },
        { "char", Keyword::CHAR },
        { "sizeof", Keyword::SIZEOF },
        { "struct", Keyword::STRUCT },
    };

} // namespace yoctocc
