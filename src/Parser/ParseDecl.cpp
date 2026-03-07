#include "Parser/ParseDecl.hpp"

#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Parser/Util.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include "Utility.hpp"

using namespace std::string_view_literals;

namespace yoctocc {

// struct-members = (declspec declarator (","  declarator)* ";")*
void ParseDecl::structMembers(Token*& token, std::shared_ptr<Type>& structType) {
    auto head = std::make_unique<Member>();
    Member* current = head.get();

    while (!token::is(token, "}")) {
        auto baseType = declSpec(token, nullptr);

        int i = 0;
        while (!token::consume(token, ";")) {
            if (i++ > 0) {
                token = token::skipIf(token, ",");
            }
            auto memberType = declarator(token, baseType);
            auto member = std::make_unique<Member>();
            member->type = memberType;
            member->name = memberType->name;
            current->next = std::move(member);
            current = current->next.get();
        }
    }

    token = token->next.get();
    structType->members = std::move(head->next);
}

// struct-union-decl = ident? ("{" struct-members)?
const std::shared_ptr<Type> ParseDecl::structUnionDecl(Token*& token) {
    Token* tag = nullptr;
    if (token->kind == TokenKind::IDENTIFIER) {
        tag = token;
        token = token->next.get();
    }

    if (tag && !token::is(token, "{")) {
        if (auto type = _scope.findTag(tag)) {
            return type;
        } else {
            Log::error("Unknown struct/union type"sv, tag);
            return std::make_shared<Type>(TypeKind::UNKNOWN);
        }
    }

    token = token::skipIf(token, "{");

    auto type = std::make_shared<Type>(TypeKind::STRUCT);
    structMembers(token, type);
    type->alignment = 1;

    if (tag) {
        _scope.pushTagScope(tag->originalValue, type);
    }

    return type;
}

// struct-decl = struct-union-decl
const std::shared_ptr<Type> ParseDecl::structDecl(Token*& token) {
    auto type = structUnionDecl(token);
    type->kind = TypeKind::STRUCT;

    int offset = 0;
    for (auto member = type->members.get(); member; member = member->next.get()) {
        offset = alignTo(offset, member->type->alignment);
        member->offset = offset;
        offset += member->type->size;
        type->alignment = std::max(type->alignment, member->type->alignment);
    }

    type->size = alignTo(offset, type->alignment);

    return type;
}

// union-decl = struct-union-decl
const std::shared_ptr<Type> ParseDecl::unionDecl(Token*& token) {
    auto type = structUnionDecl(token);
    type->kind = TypeKind::UNION;

    for (auto member = type->members.get(); member; member = member->next.get()) {
        type->size = std::max(type->size, member->type->size);
        type->alignment = std::max(type->alignment, member->type->alignment);
    }

    type->size = alignTo(type->size, type->alignment);

    return type;
}

// declspec = ("void" | "char" | "short" | "int" | "long"
//             | "typedef"
//             | struct-decl | union-decl | typedef-name)+
const std::shared_ptr<Type> ParseDecl::declSpec(Token*& token, VariableAttribute* attr) {
    enum {
        VOID  = 1 <<  0,
        CHAR  = 1 <<  2,
        SHORT = 1 <<  4,
        INT   = 1 <<  6,
        LONG  = 1 <<  8,
        OTHER = 1 << 10,
    };
    auto type = type::intType();
    int counter = 0;

    while (parser::isTypeName(token, _scope)) {
        if (token::is(token, "typedef")) {
            if (!attr) {
                Log::error("typedef is not allowed here"sv, token);
                return nullptr;
            }
            attr->isTypeDef = true;
            token = token->next.get();
            continue;
        }

        auto typeDefType = _scope.findTypeDef(token);

        if (token::is(token, "struct") || token::is(token, "union") || typeDefType) {
            if (counter) {
                break;
            }
            if (token::is(token, "struct")) {
                token = token->next.get();
                type = structDecl(token);
                counter += OTHER;
                continue;
            }
            else if (token::is(token, "union")) {
                token = token->next.get();
                type = unionDecl(token);
                counter += OTHER;
                continue;
            } else {
                type = typeDefType;
                token = token->next.get();
                counter += OTHER;
                continue;
            }
        }
        else if (token::is(token, "void")) {
            counter += VOID;
        }
        else if (token::is(token, "char")) {
            counter += CHAR;
        }
        else if (token::is(token, "short")) {
            counter += SHORT;
        }
        else if (token::is(token, "int")) {
            counter += INT;
        }
        else if (token::is(token, "long")) {
            counter += LONG;
        }
        else {
            Log::unreachable();
            return nullptr;
        }

        switch (counter) {
            case VOID:
                type = type::voidType();
                break;
            case CHAR:
                type = type::charType();
                break;
            case SHORT:
            case SHORT + INT:
                type = type::shortType();
                break;
            case INT:
                type = type::intType();
                break;
            case LONG:
            case LONG + INT:
            case LONG + LONG:
            case LONG + LONG + INT:
                type = type::longType();
                break;
            default:
                Log::error("Invalid type specifier"sv, token);
                return nullptr;
        }

        token = token->next.get();
    }

    return type;
}

// abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
const std::shared_ptr<Type> ParseDecl::abstractDeclarator(Token*& token, std::shared_ptr<Type>& type) {
    while (token::is(token, "*")) {
        type = type::pointerTo(type);
        token = token->next.get();
    }

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

// declarator = "*"* ("(" ident ")" | "(" declarator ")" | ident) type-suffix
const std::shared_ptr<Type> ParseDecl::declarator(Token*& token, const std::shared_ptr<Type>& baseType) {
    auto type = baseType;
    while (token::consume(token, "*")) {
        type = type::pointerTo(type);
    }

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

    if (token->kind == TokenKind::IDENTIFIER) {
        auto name = token;
        token = token->next.get();
        type = typeSuffix(token, type);
        type->name = name;
    } else {
        Log::error("Expected an identifier"sv, token);
    }

    return type;
}

// type-name = declspec abstract-declarator
const std::shared_ptr<Type> ParseDecl::typeName(Token*& token) {
    auto baseType = declSpec(token, nullptr);
    return abstractDeclarator(token, baseType);
}

// func-params = (param ("," param)*)? ")"
// param       = declspec declarator
const std::shared_ptr<Type> ParseDecl::functionParameters(Token*& token, std::shared_ptr<Type>& type) {
    std::shared_ptr<Type> head;
    auto current = &head;

    while (!token::is(token, ")")) {
        if (head) {
            token = token::skipIf(token, ",");
        }
        auto paramType = declSpec(token, nullptr);
        paramType = declarator(token, paramType);
        *current = paramType;
        current = &paramType->next;
    }

    type = type::functionType(type);
    type->parameters = head;
    token = token->next.get();

    return type;
}

// type-suffix = "(" func-params
//             | "[" num "]" type-suffix
//             | ε
const std::shared_ptr<Type> ParseDecl::typeSuffix(Token*& token, std::shared_ptr<Type>& type) {
    if (token::is(token, "(")) {
        token = token->next.get();
        return functionParameters(token, type);
    }

    if (token::is(token, "[")) {
        int size = token::getNumber(token->next.get());
        token = token->next.get()->next.get();
        token = token::skipIf(token, "]");
        type = typeSuffix(token, type);
        return type::arrayOf(type, size);
    }

    return type;
}


} // namespace yoctocc
