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
    SIGNED,
    UNSIGNED,
    RETURN,
    IF,
    ELSE,
    FOR,
    WHILE,
    DO,
    SIZEOF,
    STRUCT,
    UNION,
    ENUM,
    TYPEDEF,
    STATIC,
    EXTERN,
    GOTO,
    BREAK,
    CONTINUE,
    SWITCH,
    CASE,
    DEFAULT,
    ALIGNOF,
    ALIGNAS,
    CONST,
    VOLATILE,
    AUTO,
    REGISTER,
    RESTRICT,
    __RESTRICT,
    __RESTRICT__,
    NORETURN,
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
        case SIGNED:
            return "signed";
        case UNSIGNED:
            return "unsigned";
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
        case DO:
            return "do";
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
        case STATIC:
            return "static";
        case EXTERN:
            return "extern";
        case GOTO:
            return "goto";
        case BREAK:
            return "break";
        case CONTINUE:
            return "continue";
        case SWITCH:
            return "switch";
        case CASE:
            return "case";
        case DEFAULT:
            return "default";
        case ALIGNOF:
            return "_Alignof";
        case ALIGNAS:
            return "_Alignas";
        case CONST:
            return "const";
        case VOLATILE:
            return "volatile";
        case AUTO:
            return "auto";
        case REGISTER:
            return "register";
        case RESTRICT:
            return "restrict";
        case __RESTRICT:
            return "__restrict";
        case __RESTRICT__:
            return "__restrict__";
        case NORETURN:
            return "_Noreturn";
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
    {to_string_view(Keyword::SIGNED), Keyword::SIGNED},
    {to_string_view(Keyword::UNSIGNED), Keyword::UNSIGNED},
    {to_string_view(Keyword::RETURN), Keyword::RETURN},
    {to_string_view(Keyword::IF), Keyword::IF},
    {to_string_view(Keyword::ELSE), Keyword::ELSE},
    {to_string_view(Keyword::FOR), Keyword::FOR},
    {to_string_view(Keyword::WHILE), Keyword::WHILE},
    {to_string_view(Keyword::DO), Keyword::DO},
    {to_string_view(Keyword::SIZEOF), Keyword::SIZEOF},
    {to_string_view(Keyword::STRUCT), Keyword::STRUCT},
    {to_string_view(Keyword::UNION), Keyword::UNION},
    {to_string_view(Keyword::ENUM), Keyword::ENUM},
    {to_string_view(Keyword::TYPEDEF), Keyword::TYPEDEF},
    {to_string_view(Keyword::STATIC), Keyword::STATIC},
    {to_string_view(Keyword::EXTERN), Keyword::EXTERN},
    {to_string_view(Keyword::GOTO), Keyword::GOTO},
    {to_string_view(Keyword::BREAK), Keyword::BREAK},
    {to_string_view(Keyword::CONTINUE), Keyword::CONTINUE},
    {to_string_view(Keyword::SWITCH), Keyword::SWITCH},
    {to_string_view(Keyword::CASE), Keyword::CASE},
    {to_string_view(Keyword::DEFAULT), Keyword::DEFAULT},
    {to_string_view(Keyword::ALIGNOF), Keyword::ALIGNOF},
    {to_string_view(Keyword::ALIGNAS), Keyword::ALIGNAS},
    {to_string_view(Keyword::CONST), Keyword::CONST},
    {to_string_view(Keyword::VOLATILE), Keyword::VOLATILE},
    {to_string_view(Keyword::AUTO), Keyword::AUTO},
    {to_string_view(Keyword::REGISTER), Keyword::REGISTER},
    {to_string_view(Keyword::RESTRICT), Keyword::RESTRICT},
    {to_string_view(Keyword::__RESTRICT), Keyword::__RESTRICT},
    {to_string_view(Keyword::__RESTRICT__), Keyword::__RESTRICT__},
    {to_string_view(Keyword::NORETURN), Keyword::NORETURN},
};

} // namespace yoctocc
