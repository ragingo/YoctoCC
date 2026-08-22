#include "Parser/Parser.hpp"

#include <algorithm>
#include "Logger.hpp"
#include "Node/Keywords.hpp"
#include "Node/Node.hpp"
#include "Parser/Common.hpp"
#include "Parser/Util.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include "Utility.hpp"

using namespace std::string_view_literals;

namespace yoctocc {

// struct-members = (declspec declarator (","  declarator)* ";")*
void Parser::structMembers(Token*& token, std::shared_ptr<Type>& structType) {
    auto head = std::make_unique<Member>();
    Member* current = head.get();
    int index = 0;

    while (!token::is(token, "}")) {
        VariableAttribute attr;
        auto baseType = declSpec(token, &attr);

        bool isFirst = true;
        while (!token::consume(token, ";")) {
            if (!isFirst) {
                token = token::skipIf(token, ",");
            }
            isFirst = false;
            auto memberType = declarator(token, baseType);
            auto member = std::make_unique<Member>();
            member->type = memberType;
            member->name = memberType->name;
            member->index = index++;
            member->alignment = attr.alignment ? attr.alignment : member->type->alignment;
            current->next = std::move(member);
            current = current->next.get();
        }
    }

    if (current != head.get()) {
        if (current->type->kind == TypeKind::ARRAY && current->type->arraySize < 0) {
            current->type = type::arrayOf(current->type->base, 0);
            structType->isFlexibleArray = true;
        }
    }

    token = token->next.get();
    structType->members = std::move(head->next);
}

// struct-union-decl = ident? ("{" struct-members)?
std::shared_ptr<Type> Parser::structUnionDecl(Token*& token) {
    Token* tag = nullptr;
    if (token->kind == TokenKind::IDENTIFIER) {
        tag = token;
        token = token->next.get();
    }

    if (tag && !token::is(token, "{")) {
        if (auto tagScope = _parseScope.findTag(tag)) {
            return tagScope->type;
        }
        auto type = type::structType();
        type->size = -1;
        _parseScope.pushTagScope(tag->originalValue, type);
        return type;
    }

    token = token::skipIf(token, "{");

    auto type = type::structType();
    structMembers(token, type);

    if (tag) {
        if (auto tagScope = _parseScope.findTag(tag, true)) {
            *tagScope->type = *type;
            return tagScope->type;
        }
        _parseScope.pushTagScope(tag->originalValue, type);
    }

    return type;
}

// struct-decl = struct-union-decl
std::shared_ptr<Type> Parser::structDecl(Token*& token) {
    auto type = structUnionDecl(token);
    type->kind = TypeKind::STRUCT;

    if (type->size < 0) {
        return type;
    }

    int offset = 0;
    for (auto member = type->members.get(); member; member = member->next.get()) {
        offset = alignTo(offset, member->alignment);
        member->offset = offset;
        offset += member->type->size;
        type->alignment = std::max(type->alignment, member->alignment);
    }

    type->size = alignTo(offset, type->alignment);

    return type;
}

// union-decl = struct-union-decl
std::shared_ptr<Type> Parser::unionDecl(Token*& token) {
    auto type = structUnionDecl(token);
    type->kind = TypeKind::UNION;

    if (type->size < 0) {
        return type;
    }

    for (auto member = type->members.get(); member; member = member->next.get()) {
        type->size = std::max(type->size, member->type->size);
        type->alignment = std::max(type->alignment, member->alignment);
    }

    type->size = alignTo(type->size, type->alignment);

    return type;
}

// enum-specifier = ident? "{" enum-list? "}"
//                | ident ("{" enum-list? "}")?
//
// enum-list      = ident ("=" num)? ("," ident ("=" num)?)* ","?
std::shared_ptr<Type> Parser::enumSpecifier(Token*& token) {
    Token* tag = nullptr;

    if (token->kind == TokenKind::IDENTIFIER) {
        tag = token;
        token = token->next.get();
    }

    if (tag && !token::is(token, "{")) {
        if (auto tagScope = _parseScope.findTag(tag); tagScope && tagScope->type->kind == TypeKind::ENUM) {
            return tagScope->type;
        }
        Log::error("Unknown enum type"sv, tag);
        return std::make_shared<Type>(TypeKind::UNKNOWN);
    }

    token = token::skipIf(token, "{");

    int i = 0;
    int value = 0;
    auto type = type::enumType();
    while (!token::consumeEnd(token)) {
        if (i++ > 0) {
            token = token::skipIf(token, ",");
        }
        auto name = token::getIdentifier(token);
        token = token->next.get();

        if (token::is(token, "=")) {
            token = token->next.get();
            value = constExpression(token);
        }

        auto scope = _parseScope.pushVariableScope(name);
        scope->enumType = type;
        scope->enumValue = value++;
    }

    if (tag) {
        _parseScope.pushTagScope(tag->originalValue, type);
    }

    return type;
}

// declspec = ("void" | "_Bool" | "char" | "short" | "int" | "long"
//             | "typedef" | "static" | "extern"
//             | "signed" | "unsigned"
//             | struct-decl | union-decl | typedef-name
//             | enum-specifier
//             | "const" | "volatile" | "auto" | "register" | "restrict"
//             | "__restrict" | "__restrict__" | "_Noreturn")+
std::shared_ptr<Type> Parser::declSpec(Token*& token, VariableAttribute* attr) {
    // clang-format off
    enum {
        VOID     = 1 << 0,
        BOOL     = 1 << 2,
        CHAR     = 1 << 4,
        SHORT    = 1 << 6,
        INT      = 1 << 8,
        LONG     = 1 << 10,
        FLOAT    = 1 << 12,
        DOUBLE   = 1 << 14,
        OTHER    = 1 << 16,
        SIGNED   = 1 << 17,
        UNSIGNED = 1 << 18,
    };
    // clang-format on
    auto type = type::intType();
    int counter = 0;

    while (parser::isTypeName(token, _parseScope)) {
        if (token::is(token, Keyword::TYPEDEF) || token::is(token, Keyword::STATIC) || token::is(token, Keyword::EXTERN)) {
            if (!attr) {
                Log::error("typedef or static is not allowed here"sv, token);
                return nullptr;
            }
            if (token::is(token, Keyword::TYPEDEF)) {
                attr->isTypeDef = true;
            } else if (token::is(token, Keyword::STATIC)) {
                attr->isStatic = true;
            } else {
                attr->isExtern = true;
            }
            if (attr->isTypeDef && (attr->isStatic || attr->isExtern)) {
                Log::error("typedef may not be used together with static or extern"sv, token);
                return nullptr;
            }
            token = token->next.get();
            continue;
        }

        std::array comsumedIgnreKeywords {
            token::consume(token, Keyword::CONST),
            token::consume(token, Keyword::VOLATILE),
            token::consume(token, Keyword::AUTO),
            token::consume(token, Keyword::REGISTER),
            token::consume(token, Keyword::RESTRICT),
            token::consume(token, Keyword::__RESTRICT),
            token::consume(token, Keyword::__RESTRICT__),
            token::consume(token, Keyword::NORETURN),
        };
        if (std::ranges::contains(comsumedIgnreKeywords, true)) {
            continue;
        }

        if (token::is(token, Keyword::ALIGNAS)) {
            if (!attr) {
                Log::error("alignas is not allowed here"sv, token);
                return nullptr;
            }
            token = token::skipIf(token->next.get(), "(");
            if (type::isTypeName(token)) {
                attr->alignment = typeName(token)->alignment;
            } else {
                attr->alignment = constExpression(token);
            }
            token = token::skipIf(token, ")");
            continue;
        }

        auto typeDefType = _parseScope.findTypeDef(token);

        if (token::is(token, Keyword::STRUCT) || token::is(token, Keyword::UNION) || token::is(token, Keyword::ENUM) ||
            typeDefType) {
            if (counter) {
                break;
            }
            if (token::is(token, Keyword::STRUCT)) {
                token = token->next.get();
                type = structDecl(token);
                counter += OTHER;
                continue;
            } else if (token::is(token, Keyword::UNION)) {
                token = token->next.get();
                type = unionDecl(token);
                counter += OTHER;
                continue;
            } else if (token::is(token, Keyword::ENUM)) {
                token = token->next.get();
                type = enumSpecifier(token);
                counter += OTHER;
                continue;
            } else {
                type = typeDefType;
                token = token->next.get();
                counter += OTHER;
                continue;
            }
        } else if (token::is(token, Keyword::VOID)) {
            counter += VOID;
        } else if (token::is(token, Keyword::BOOL)) {
            counter += BOOL;
        } else if (token::is(token, Keyword::CHAR)) {
            counter += CHAR;
        } else if (token::is(token, Keyword::SHORT)) {
            counter += SHORT;
        } else if (token::is(token, Keyword::INT)) {
            counter += INT;
        } else if (token::is(token, Keyword::LONG)) {
            counter += LONG;
        } else if (token::is(token, Keyword::FLOAT)) {
            counter += FLOAT;
        } else if (token::is(token, Keyword::DOUBLE)) {
            counter += DOUBLE;
        } else if (token::is(token, Keyword::SIGNED)) {
            counter |= SIGNED;
        } else if (token::is(token, Keyword::UNSIGNED)) {
            counter |= UNSIGNED;
        } else {
            Log::unreachable();
            return nullptr;
        }

        switch (counter) {
            case VOID:
                type = type::voidType();
                break;
            case BOOL:
                type = type::boolType();
                break;
            case CHAR:
            case SIGNED + CHAR:
                type = type::charType();
                break;
            case UNSIGNED + CHAR:
                type = type::ucharType();
                break;
            case SHORT:
            case SHORT + INT:
            case SIGNED + SHORT:
            case SIGNED + SHORT + INT:
                type = type::shortType();
                break;
            case UNSIGNED + SHORT:
            case UNSIGNED + SHORT + INT:
                type = type::ushortType();
                break;
            case INT:
            case SIGNED:
            case SIGNED + INT:
                type = type::intType();
                break;
            case UNSIGNED:
            case UNSIGNED + INT:
                type = type::uintType();
                break;
            case LONG:
            case LONG + INT:
            case LONG + LONG:
            case LONG + LONG + INT:
            case SIGNED + LONG:
            case SIGNED + LONG + INT:
            case SIGNED + LONG + LONG:
            case SIGNED + LONG + LONG + INT:
                type = type::longType();
                break;
            case UNSIGNED + LONG:
            case UNSIGNED + LONG + INT:
            case UNSIGNED + LONG + LONG:
            case UNSIGNED + LONG + LONG + INT:
                type = type::ulongType();
                break;
            case FLOAT:
                type = type::floatType();
                break;
            case DOUBLE:
            case LONG + DOUBLE:
                type = type::doubleType();
                break;
            default:
                Log::error("Invalid type specifier"sv, token);
                return nullptr;
        }

        token = token->next.get();
    }

    return type;
}

// abstract-declarator = pointers ("(" abstract-declarator ")")? type-suffix
std::shared_ptr<Type> Parser::abstractDeclarator(Token*& token, std::shared_ptr<Type>& type) {
    type = pointers(token, type);

    if (token::is(token, "(")) {
        auto start = token;
        auto next = start->next.get();
        auto dummyType = std::make_shared<Type>(TypeKind::UNKNOWN);
        abstractDeclarator(next, dummyType);
        token = token::skipIf(next, ")");
        type = typeSuffix(token, type);
        next = start->next.get();
        type = abstractDeclarator(next, type);
    } else {
        type = typeSuffix(token, type);
    }

    return type;
}

// pointers = ("*" ("const" | "volatile" | "restrict")*)*
std::shared_ptr<Type> Parser::pointers(Token*& token, const std::shared_ptr<Type>& baseType) {
    auto type = baseType;
    while (token::consume(token, "*")) {
        type = type::pointerTo(type);

        while (true) {
            std::array results {
                token::is(token, Keyword::CONST),
                token::is(token, Keyword::VOLATILE),
                token::is(token, Keyword::RESTRICT),
                token::is(token, Keyword::__RESTRICT),
                token::is(token, Keyword::__RESTRICT__)
            };
            if (std::ranges::contains(results, true)) {
                token = token->next.get();
            } else {
                break;
            }
        }
    }
    return type;
}

// declarator = pointers ("(" ident ")" | "(" declarator ")" | ident) type-suffix
std::shared_ptr<Type> Parser::declarator(Token*& token, const std::shared_ptr<Type>& baseType) {
    auto type = pointers(token, baseType);

    if (token::is(token, "(")) {
        auto start = token;
        auto next = start->next.get();
        auto dummyType = std::make_shared<Type>(TypeKind::UNKNOWN);
        declarator(next, dummyType);
        token = token::skipIf(next, ")");
        type = typeSuffix(token, type);
        next = start->next.get();
        type = declarator(next, type);
        return type;
    }

    Token* name = nullptr;
    Token* namePos = token;

    if (token->kind == TokenKind::IDENTIFIER) {
        name = token;
        token = token->next.get();
    }

    type = typeSuffix(token, type);
    type->name = name;
    type->namePos = namePos;

    return type;
}

// type-name = declspec abstract-declarator
std::shared_ptr<Type> Parser::typeName(Token*& token) {
    auto baseType = declSpec(token, nullptr);
    return abstractDeclarator(token, baseType);
}

// func-params = ("void" | param ("," param)* ("," "...")?)? ")"
// param       = declspec declarator
std::shared_ptr<Type> Parser::functionParameters(Token*& token, std::shared_ptr<Type>& type) {
    if (token::is(token, "void") && token::is(token->next.get(), ")")) {
        token = token->next->next.get();
        return type::functionType(type);
    }
    std::shared_ptr<Type> head;
    auto current = &head;
    bool isVariadic = false;

    while (!token::is(token, ")")) {
        if (head) {
            token = token::skipIf(token, ",");
        }

        if (token::is(token, "...")) {
            isVariadic = true;
            token = token->next.get();
            token::skipIf(token, ")");
            break;
        }

        auto paramType = declSpec(token, nullptr);
        paramType = declarator(token, paramType);

        if (paramType->kind == TypeKind::ARRAY) {
            auto name = paramType->name;
            paramType = type::pointerTo(paramType->base);
            paramType->name = name;
        }
        *current = paramType;
        current = &paramType->next;
    }

    if (current == &head) {
        isVariadic = true;
    }

    type = type::functionType(type);
    type->parameters = head;
    type->isVariadic = isVariadic;
    token = token->next.get();

    return type;
}

// array-dimensions = ("static" | "restrict")* const-expr? "]" type-suffix
std::shared_ptr<Type> Parser::arrayDimensions(Token*& token, std::shared_ptr<Type>& type) {
    while (true) {
        std::array results {
            token::is(token, Keyword::STATIC),
            token::is(token, Keyword::RESTRICT)
        };
        if (std::ranges::contains(results, true)) {
            token = token->next.get();
        } else {
            break;
        }
    }
    if (token::is(token, "]")) {
        token = token->next.get();
        type = typeSuffix(token, type);
        return type::arrayOf(type, -1);
    }

    int size = constExpression(token);
    token = token::skipIf(token, "]");
    type = typeSuffix(token, type);
    return type::arrayOf(type, size);
}

// type-suffix = "(" func-params
//             | "[" array-dimensions
//             | ε
std::shared_ptr<Type> Parser::typeSuffix(Token*& token, std::shared_ptr<Type>& type) {
    if (token::is(token, "(")) {
        token = token->next.get();
        return functionParameters(token, type);
    }

    if (token::is(token, "[")) {
        token = token->next.get();
        return arrayDimensions(token, type);
    }

    return type;
}


} // namespace yoctocc
