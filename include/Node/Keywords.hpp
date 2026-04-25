#pragma once
#include <string_view>
#include <unordered_map>

namespace yoctocc {

enum class Keyword {
    VOID,
    BOOL,
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
    ENUM,
    TYPEDEF,
};

inline constexpr std::string_view to_string_view(Keyword keyword) {
    using enum Keyword;
    switch (keyword) {
        case VOID:
            return "void";
        case BOOL:
            return "_Bool";
        case CHAR:
            return "char";
        case SHORT:
            return "short";
        case INT:
            return "int";
        case LONG:
            return "long";
        case RETURN:
            return "return";
        case IF:
            return "if";
        case ELSE:
            return "else";
        case FOR:
            return "for";
        case WHILE:
            return "while";
        case SIZEOF:
            return "sizeof";
        case STRUCT:
            return "struct";
        case UNION:
            return "union";
        case ENUM:
            return "enum";
        case TYPEDEF:
            return "typedef";
    }
    return "";
}
static_assert(to_string_view(Keyword::VOID) == "void");

inline const std::unordered_map<std::string_view, Keyword> KEYWORDS = {
    {to_string_view(Keyword::VOID), Keyword::VOID},
    {to_string_view(Keyword::BOOL), Keyword::BOOL},
    {to_string_view(Keyword::CHAR), Keyword::CHAR},
    {to_string_view(Keyword::SHORT), Keyword::SHORT},
    {to_string_view(Keyword::INT), Keyword::INT},
    {to_string_view(Keyword::LONG), Keyword::LONG},
    {to_string_view(Keyword::RETURN), Keyword::RETURN},
    {to_string_view(Keyword::IF), Keyword::IF},
    {to_string_view(Keyword::ELSE), Keyword::ELSE},
    {to_string_view(Keyword::FOR), Keyword::FOR},
    {to_string_view(Keyword::WHILE), Keyword::WHILE},
    {to_string_view(Keyword::SIZEOF), Keyword::SIZEOF},
    {to_string_view(Keyword::STRUCT), Keyword::STRUCT},
    {to_string_view(Keyword::UNION), Keyword::UNION},
    {to_string_view(Keyword::ENUM), Keyword::ENUM},
    {to_string_view(Keyword::TYPEDEF), Keyword::TYPEDEF},
};

} // namespace yoctocc
