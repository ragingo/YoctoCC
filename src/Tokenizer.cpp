#include "Tokenizer.hpp"

#include <format>
#include <fstream>
#include <string>
#include "Logger.hpp"
#include "Node/Keywords.hpp"
#include "String/String.hpp"
#include "Token.hpp"
#include "Type.hpp"

using namespace std::string_view_literals;

namespace yoctocc {

std::shared_ptr<Token> tokenize(std::ifstream& ifs) {
    auto head = std::make_shared<Token>();
    auto current = head;

    std::string number;

    std::string content{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
    Log::sourceCode = content;
    auto it = content.begin();

    while (it != content.end()) {
        char ch = *it;

        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            ++it;
            continue;
        }

        if (std::isdigit(ch)) {
            number += ch;

            if (std::next(it) != content.end()) {
                ++it;
                continue;
            }
        }

        if (!number.empty()) {
            auto next = std::make_shared<Token>(TokenKind::DIGIT);
            next->originalValue = number;
            next->numberValue = std::stoi(number);
            next->location = std::distance(content.begin(), it) - number.size();
            current->next = next;
            current = next;
            number.clear();
        }

        // string literal
        if (ch == '"') {
            std::string str;
            size_t startLocation = std::distance(content.begin(), it);
            ++it;
            while (it != content.end() && *it != '"') {
                if (*it == '\n' || *it == '\r' || *it == '\0') {
                    Log::error(std::distance(content.begin(), it), "unclosed string literal"sv);
                    return nullptr;
                }
                // escape sequences
                if (*it == '\\') {
                    ++it;
                    switch (*it) {
                        case 'n': str += '\n'; break;
                        case 't': str += '\t'; break;
                        case 'r': str += '\r'; break;
                        case 'a': str += '\a'; break;
                        case 'b': str += '\b'; break;
                        case 'f': str += '\f'; break;
                        case 'v': str += '\v'; break;
                        case 'e': str += 27; break;
                        default: str += *it; break;
                    }
                } else {
                    str += *it;
                }
                ++it;
            }
            ++it;

            auto next = std::make_shared<Token>(TokenKind::STRING);
            next->type = type::arrayOf(type::charType(), str.size() + 1);
            next->originalValue = str;
            next->location = startLocation;
            current->next = next;
            current = next;
            continue;
        }

        if (isIdentifierChar(ch, true)) {
            std::string identifier;
            identifier += ch;
            ++it;
            while (it != content.end() && isIdentifierChar(*it, false)) {
                identifier += *it;
                ++it;
            }
            auto next = std::make_shared<Token>(TokenKind::IDENTIFIER);
            next->originalValue = identifier;
            next->location = std::distance(content.begin(), it) - identifier.size();
            current->next = next;
            current = next;
            continue;
        }

        if (std::ispunct(ch)) {
            auto nextCh = std::next(it) != content.end() ? *std::next(it) : '\0';
            std::array<char, 2> chars = { ch, nextCh };
            if (chars == std::array{ '=', '=' } || chars == std::array{ '!', '=' } ||
                chars == std::array{ '<', '=' } || chars == std::array{ '>', '=' }) {
                auto next = std::make_shared<Token>(TokenKind::PUNCTUATOR);
                next->originalValue = std::string{ ch, nextCh };
                next->location = std::distance(content.begin(), it);
                current->next = next;
                current = next;
                it += 2;
                continue;
            }

            auto next = std::make_shared<Token>(TokenKind::PUNCTUATOR);
            next->originalValue = ch;
            next->location = std::distance(content.begin(), it);
            current->next = next;
            current = next;
            ++it;
            continue;
        }

        Log::error(std::distance(content.begin(), it), std::format("Unexpected character '{}'", ch));
        return nullptr;
    }

    auto terminator = std::make_shared<Token>(TokenKind::TERMINATOR);
    current->next = terminator;

    for (auto token = head->next; token; token = token->next) {
        if (token->kind == TokenKind::TERMINATOR) {
            break;
        }
        if (token->kind == TokenKind::IDENTIFIER) {
            if (KEYWORDS.find(token->originalValue) != KEYWORDS.end()) {
                token->kind = TokenKind::KEYWORD;
            }
        }
    }

    return head->next;
}

} // namespace yoctocc
