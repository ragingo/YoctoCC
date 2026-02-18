#include "Tokenizer.hpp"

#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include "Logger.hpp"
#include "Node/Keywords.hpp"
#include "String/String.hpp"
#include "Token.hpp"
#include "Type.hpp"

using namespace std::string_view_literals;

namespace {
    using namespace yoctocc;

    struct ParseContext {
        const std::string::const_iterator begin;
        const std::string::const_iterator end;
        std::string::const_iterator& it;
        size_t line;
    };

    inline bool isEOF(const ParseContext& context) {
        return context.it == context.end;
    }

    inline bool hasNext(const ParseContext& context) {
        return isEOF(context) ? false : std::next(context.it) != context.end;
    }

    bool parseLineComment(ParseContext& context) {
        if (*context.it != '/' || !hasNext(context) || *std::next(context.it) != '/') {
            return false;
        }
        context.it += 2;
        context.it = std::find_if(context.it, context.end, [](char ch) { return ch == '\n'; });
        return true;
    }

    bool parseBlockComment(ParseContext& context) {
        if (*context.it != '/' || !hasNext(context) || *std::next(context.it) != '*') {
            return false;
        }
        auto start = context.it;
        context.it += 2;
        constexpr auto endComment = "*/"sv;
        context.it = std::search(context.it, context.end, endComment.cbegin(), endComment.cend());
        if (isEOF(context)) {
            Log::error("unclosed block comment"sv, std::distance(context.begin, context.it));
            return false;
        }
        context.it += 2;
        context.line += std::count(start, context.it, '\n');
        return true;
    }

    std::unique_ptr<Token> parseNumber(ParseContext& context) {
        std::string number;
        while (hasNext(context) && std::isdigit(*context.it)) {
            number += *context.it;
            ++context.it;
        }
        auto token = std::make_unique<Token>(TokenKind::DIGIT);
        token->originalValue = number;
        token->numberValue = std::stoi(number);
        token->location = std::distance(context.begin, context.it - token->originalValue.size());
        token->line = context.line;
        return token;
    }

    std::unique_ptr<Token> parseStringLiteral(ParseContext& context) {
        std::string str;
        ++context.it; // 最初の " をスキップ

        while (hasNext(context) && *context.it != '"') {
            if (*context.it == '\n' || *context.it == '\r' || *context.it == '\0') {
                Log::error("unclosed string literal"sv, std::distance(context.begin, context.it));
                return nullptr;
            }
            // escape sequences
            if (*context.it == '\\') {
                ++context.it;
                // octal
                if (hasNext(context) && isOctalDigit(*context.it)) {
                    int value = 0;
                    int count = 0;
                    while (hasNext(context) && isOctalDigit(*context.it) && count < 3) {
                        value = (value << 3) + yoctocc::atoi(*context.it);
                        ++context.it;
                        ++count;
                    }
                    str += static_cast<char>(value);
                    continue;
                }
                // hex
                if (hasNext(context) && *context.it == 'x') {
                    ++context.it;
                    int value = 0;
                    while (hasNext(context) && isHexDigit(*context.it)) {
                        value = (value << 4) + hexCharToInt(*context.it);
                        ++context.it;
                    }
                    str += static_cast<char>(value);
                    continue;
                }
                switch (*context.it) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    case 'a': str += '\a'; break;
                    case 'b': str += '\b'; break;
                    case 'f': str += '\f'; break;
                    case 'v': str += '\v'; break;
                    case 'e': str += 27; break;
                    default: str += *context.it; break;
                }
            } else {
                str += *context.it;
            }
            ++context.it;
        }
        ++context.it;

        auto token = std::make_unique<Token>(TokenKind::STRING);
        token->type = type::arrayOf(type::charType(), str.size() + 1);
        token->originalValue = str;
        token->location = std::distance(context.begin, context.it - token->originalValue.size());
        token->line = context.line;
        return token;
    }

    std::unique_ptr<Token> parseIdentifier(ParseContext& context) {
        std::string identifier;
        while (hasNext(context) && isIdentifierChar(*context.it, false)) {
            identifier += *context.it;
            ++context.it;
        }
        auto token = std::make_unique<Token>(TokenKind::IDENTIFIER);
        token->originalValue = identifier;
        token->location = std::distance(context.begin, context.it - token->originalValue.size());
        token->line = context.line;
        return token;
    }

    std::unique_ptr<Token> parsePunctuator(char ch, ParseContext& context) {
        auto nextCh = hasNext(context) ? *std::next(context.it) : '\0';
        std::string chars{ ch, nextCh };
        static const std::array operators = { "==", "!=", "<=", ">=", "->" };

        if (std::ranges::find(operators, chars) != operators.end()) {
            auto token = std::make_unique<Token>(TokenKind::PUNCTUATOR);
            token->originalValue = std::string{ ch, nextCh };
            token->location = std::distance(context.begin, context.it - token->originalValue.size());
            token->line = context.line;
            context.it += 2;
            return token;
        }

        auto token = std::make_unique<Token>(TokenKind::PUNCTUATOR);
        token->originalValue = ch;
        token->location = std::distance(context.begin, context.it - token->originalValue.size());
        token->line = context.line;

        ++context.it;

        return token;
    }
}

namespace yoctocc {

std::unique_ptr<Token> tokenize(std::ifstream& ifs) {
    auto head = std::make_unique<Token>();
    Token* current = head.get();

    std::string content{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
    Log::sourceCode = content;
    auto it = content.cbegin();
    auto startLocation = [&it, &content]() { return static_cast<size_t>(std::distance(content.cbegin(), it)); };
    ParseContext context{ content.cbegin(), content.cend(), it, 1 };

    while (it != content.cend()) {
        char ch = *it;

        if (parseLineComment(context)) {
            continue;
        }

        if (parseBlockComment(context)) {
            continue;
        }

        if (std::isspace(ch)) {
            if (ch == '\n') {
                ++context.line;
            }
            ++it;
            continue;
        }

        std::unique_ptr<Token> next;

        if (std::isdigit(ch)) {
            next = parseNumber(context);
        } else if (ch == '"') {
            next = parseStringLiteral(context);
        } else if (isIdentifierChar(ch, true)) {
            next = parseIdentifier(context);
        } else if (std::ispunct(ch)) {
            next = parsePunctuator(ch, context);
        } else {
            Log::error(std::format("Unexpected character '{}'", ch), startLocation());
            return nullptr;
        }

        if (!next) {
            return nullptr;
        }

        current->next = std::move(next);
        current = current->next.get();
    }

    auto terminator = std::make_unique<Token>(TokenKind::TERMINATOR);
    current->next = std::move(terminator);

    for (Token* tok = head->next.get(); tok; tok = tok->next.get()) {
        if (tok->kind == TokenKind::TERMINATOR) {
            break;
        }
        if (tok->kind == TokenKind::IDENTIFIER) {
            if (KEYWORDS.find(tok->originalValue) != KEYWORDS.end()) {
                tok->kind = TokenKind::KEYWORD;
            }
        }
    }

    return std::move(head->next);
}

} // namespace yoctocc
