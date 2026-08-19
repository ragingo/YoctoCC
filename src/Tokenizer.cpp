#include "Tokenizer.hpp"

#include "Logger.hpp"
#include "Node/Keywords.hpp"
#include "String/String.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>

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

constexpr std::tuple<bool, bool> checkIntegerSuffix(std::string_view s) {
    switch (s.size()) {
        case 3: {
            std::array suffixes {
                "ull"sv, "uLL"sv, "Ull"sv, "ULL"sv,
                "llu"sv, "llU"sv, "LLu"sv, "LLU"sv
            };
            if (std::ranges::contains(suffixes, s)) {
                return {true, true};
            }
            return {false, false};
        }
       case 2: {
            std::array suffixes {
                "ul"sv, "uL"sv, "Ul"sv, "UL"sv,
                "lu"sv, "lU"sv, "Lu"sv, "LU"sv
            };
            if (std::ranges::contains(suffixes, s)) {
                return {true, true};
            }
            if (std::ranges::contains(std::array{"ll"sv, "LL"sv}, s)) {
                return {true, false};
            }
            return {false, false};
        }
        case 1: {
            if (std::ranges::contains(std::array{"u"sv, "U"sv}, s)) {
                return {false, true};
            }
            if (std::ranges::contains(std::array{"l"sv, "L"sv}, s)) {
                return {true, false};
            }
            return {false, false};
        }
        default:
           std::unreachable();
    }
}
static_assert(checkIntegerSuffix("u") == std::make_tuple(false, true));
static_assert(checkIntegerSuffix("U") == std::make_tuple(false, true));
static_assert(checkIntegerSuffix("l") == std::make_tuple(true, false));
static_assert(checkIntegerSuffix("L") == std::make_tuple(true, false));
static_assert(checkIntegerSuffix("ll") == std::make_tuple(true, false));
static_assert(checkIntegerSuffix("LL") == std::make_tuple(true, false));
static_assert(checkIntegerSuffix("ul") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("uL") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("Ul") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("UL") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("lu") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("lU") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("Lu") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("LU") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("ull") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("uLL") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("Ull") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("ULL") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("llu") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("llU") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("LLu") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("LLU") == std::make_tuple(true, true));
static_assert(checkIntegerSuffix("a") == std::make_tuple(false, false));
static_assert(checkIntegerSuffix("Ll") == std::make_tuple(false, false));
static_assert(checkIntegerSuffix("ULU") == std::make_tuple(false, false));
static_assert(checkIntegerSuffix("LUL") == std::make_tuple(false, false));

std::unique_ptr<Token> parseNumber(ParseContext& context) {
    int base = 10;
    std::string numberStr;
    auto prefix2 = std::string_view(context.it, context.it + 2);
    auto prefix1 = std::string_view(context.it, context.it + 1);
    if (prefix2 == "0x"sv || prefix2 == "0X"sv) {
        base = 16;
        context.it += 2;
        numberStr += "0x";
    } else if (prefix2 == "0b"sv || prefix2 == "0B"sv) {
        base = 2;
        context.it += 2;
        numberStr += "0b";
    } else if (prefix1 == "0"sv) {
        base = 8;
        ++context.it;
        numberStr += "0";
    }

    while (hasNext(context)) {
        if (base == 10 && !std::isdigit(*context.it)) {
            break;
        }
        if (base == 16 && !isHexDigit(*context.it)) {
            break;
        }
        if (base == 8 && !isOctalDigit(*context.it)) {
            break;
        }
        if (base == 2 && *context.it != '0' && *context.it != '1') {
            break;
        }
        numberStr += *context.it;
        ++context.it;
    }

    auto value = static_cast<int64_t>(std::stoull(numberStr, nullptr, base));
    bool hasL = false;
    bool hasU = false;
    int count = 3;
    while (count > 0) {
        std::string_view suffix(context.it, context.it + count);
        if (auto [l, r] = checkIntegerSuffix(suffix); l || r) {
            hasL = l;
            hasU = r;
            context.it += count;
        }
        count--;
    }

    std::shared_ptr<Type> type;
    if (base == 10) {
        if (hasL && hasU) {
            type = type::ulongType();
        } else if (hasL) {
            type = type::longType();
        } else if (hasU) {
            type = value >> 32 ? type::ulongType() : type::uintType();
        } else {
            type = value >> 31 ? type::longType() : type::intType();
        }
    } else {
        if (hasL && hasU) {
            type = type::ulongType();
        } else if (hasL) {
            type = value >> 63 ? type::ulongType() : type::longType();
        } else if (hasU) {
            type = value >> 32 ? type::ulongType() : type::uintType();
        } else if (value >> 63) {
            type = type::ulongType();
        } else if (value >> 32) {
            type = type::longType();
        } else if (value >> 31) {
            type = type::uintType();
        } else {
            type = type::intType();
        }
    }

    auto token = std::make_unique<Token>(TokenKind::DIGIT);
    token->originalValue = numberStr;
    token->integerValue = value;
    token->type = type;
    token->location = std::distance(context.begin, context.it - token->originalValue.size());
    token->line = context.line;
    return token;
}

std::string parseEscapeSequence(ParseContext& context) {
    std::string str;
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
        --context.it; // while 内で次の文字に進んでいるため、最後に1つ戻す
        return str;
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
        --context.it; // while 内で次の文字に進んでいるため、最後に1つ戻す
        return str;
    }
    switch (*context.it) {
        case 'n':
            str += '\n';
            break;
        case 't':
            str += '\t';
            break;
        case 'r':
            str += '\r';
            break;
        case 'a':
            str += '\a';
            break;
        case 'b':
            str += '\b';
            break;
        case 'f':
            str += '\f';
            break;
        case 'v':
            str += '\v';
            break;
        case 'e':
            str += static_cast<char>(27);
            break;
        default:
            str += *context.it;
            break;
    }
    return str;
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
            str += parseEscapeSequence(context);
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

std::unique_ptr<Token> parseCharacterLiteral(ParseContext& context) {
    if (!hasNext(context)) {
        Log::error("empty character literal"sv, std::distance(context.begin, context.it));
        return nullptr;
    }
    ++context.it; // 最初の ' をスキップ

    if (*context.it == '\0') {
        Log::error("unclosed character literal"sv, std::distance(context.begin, context.it));
        return nullptr;
    }

    // arm64 上でも x86-64 と同様に char を signed として扱うため、
    // unsigned char で一旦受け取った後に int8_t で符号拡張する
    unsigned char rawValue;
    if (hasNext(context) && *context.it == '\\') {
        rawValue = static_cast<unsigned char>(parseEscapeSequence(context)[0]);
    } else {
        rawValue = static_cast<unsigned char>(*context.it);
    }
    int value = static_cast<signed char>(rawValue);

    if (!hasNext(context)) {
        Log::error("unclosed character literal"sv, std::distance(context.begin, context.it));
        return nullptr;
    }

    ++context.it;

    if (*context.it != '\'') {
        Log::error("unclosed character literal"sv, std::distance(context.begin, context.it));
        return nullptr;
    }

    ++context.it; // 最後の ' をスキップ

    auto token = std::make_unique<Token>(TokenKind::DIGIT);
    token->type = type::intType();
    token->integerValue = value;
    token->location = std::distance(context.begin, context.it - 2);
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
    // clang-format off
    static const std::array operators = {
        "=="sv, "!="sv, "<="sv, ">="sv, "->"sv, "+="sv, "-="sv, "*="sv, "/="sv, "%="sv,
        "<<="sv, ">>="sv, "<<"sv, ">>"sv, "++"sv, "--"sv, "&="sv, "|="sv, "^="sv, "&&"sv,
        "||"sv, "..."sv
    };
    // clang-format on

    for (const auto& op : operators) {
        if (std::string_view(context.it, context.it + op.size()) == op) {
            auto token = std::make_unique<Token>(TokenKind::PUNCTUATOR);
            token->originalValue = op;
            token->location = std::distance(context.begin, context.it - token->originalValue.size());
            token->line = context.line;
            context.it += op.size();
            return token;
        }
    }

    auto token = std::make_unique<Token>(TokenKind::PUNCTUATOR);
    token->originalValue = ch;
    token->location = std::distance(context.begin, context.it - token->originalValue.size());
    token->line = context.line;

    ++context.it;

    return token;
}
} // namespace

namespace yoctocc {

std::unique_ptr<Token> tokenize(std::ifstream& ifs) {
    auto head = std::make_unique<Token>();
    Token* current = head.get();

    std::string content{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
    Log::sourceCode = content;
    auto it = content.cbegin();
    auto startLocation = [&it, &content]() { return static_cast<size_t>(std::distance(content.cbegin(), it)); };
    ParseContext context{content.cbegin(), content.cend(), it, 1};

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
        } else if (ch == '\'') {
            next = parseCharacterLiteral(context);
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
            if (KEYWORDS.contains(tok->originalValue)) {
                tok->kind = TokenKind::KEYWORD;
            }
        }
    }

    return std::move(head->next);
}

} // namespace yoctocc
