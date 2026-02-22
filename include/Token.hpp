#pragma once
#include <cassert>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>

namespace yoctocc {

// https://learn.microsoft.com/ja-jp/cpp/c-language/lexical-grammar?view=msvc-170
enum class TokenKind {
    UNKNOWN,
    IDENTIFIER,
    PUNCTUATOR,
    KEYWORD,
    STRING,
    DIGIT,
    TERMINATOR,
};

} // namespace yoctocc

template <>
struct std::formatter<yoctocc::TokenKind> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const yoctocc::TokenKind& type, std::format_context& ctx) const -> std::format_context::iterator {
        using enum yoctocc::TokenKind;
        std::string_view name;
        switch (type) {
            case UNKNOWN:    name = "UNKNOWN";    break;
            case IDENTIFIER: name = "IDENTIFIER"; break;
            case PUNCTUATOR: name = "PUNCTUATOR"; break;
            case KEYWORD:    name = "KEYWORD";    break;
            case STRING:     name = "STRING";     break;
            case DIGIT:      name = "DIGIT";      break;
            case TERMINATOR: name = "TERMINATOR"; break;
            default:         name = "???";        break;
        }
        return std::format_to(ctx.out(), "{}", name);
    }
};

namespace yoctocc {

struct Type;

struct Token {
    TokenKind kind;
    std::string originalValue;
    int64_t numberValue;
    size_t location;
    size_t line;
    std::shared_ptr<Type> type;
    std::unique_ptr<Token> next;

    Token(TokenKind kind = TokenKind::UNKNOWN) : kind(kind), numberValue(0), location(0), line(0) {}
};

namespace token {
    inline bool is(const Token* token, std::string_view originalValue) {
        return token && token->originalValue == originalValue;
    }

    inline Token* skipIf(Token* token, std::string_view originalValue) {
        if (is(token, originalValue)) {
            return token->next.get();
        }
        return token;
    }

    inline bool consume(Token*& token, std::string_view originalValue) {
        if (is(token, originalValue)) {
            token = token->next.get();
            return true;
        }
        return false;
    }

    inline const std::string& getIdentifier(const Token* token) {
        assert(token && token->kind == TokenKind::IDENTIFIER);
        return token->originalValue;
    }

    inline int64_t getNumber(const Token* token) {
        assert(token && token->kind == TokenKind::DIGIT);
        return token->numberValue;
    }
}

} // namespace yoctocc
