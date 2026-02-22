#include "Parser/ParseDecl.hpp"

#include "Logger.hpp"
#include "Node/Node.hpp"
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
        auto baseType = declSpec(token);

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

// declspec = "char" | "short" | "int" | "long" | struct-decl | union-decl
const std::shared_ptr<Type> ParseDecl::declSpec(Token*& token) {
    if (token::is(token, "char")) {
        token = token->next.get();
        return type::charType();
    }
    if (token::is(token, "int")) {
        token = token->next.get();
        return type::intType();
    }
    if (token::is(token, "long")) {
        token = token->next.get();
        return type::longType();
    }
    if (token::is(token, "struct")) {
        token = token->next.get();
        return structDecl(token);
    }
    if (token::is(token, "union")) {
        token = token->next.get();
        return unionDecl(token);
    }
    Log::error("Expected a type specifier"sv, token);
    return std::make_shared<Type>(TypeKind::UNKNOWN);
}

// declarator = "*"* ident type-suffix
const std::shared_ptr<Type> ParseDecl::declarator(Token*& token, const std::shared_ptr<Type>& baseType) {
    auto type = baseType;
    while (token::consume(token, "*")) {
        type = type::pointerTo(type);
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

// func-params = (param ("," param)*)? ")"
// param       = declspec declarator
const std::shared_ptr<Type> ParseDecl::functionParameters(Token*& token, std::shared_ptr<Type>& type) {
    std::shared_ptr<Type> head;
    auto current = &head;

    while (!token::is(token, ")")) {
        if (head) {
            token = token::skipIf(token, ",");
        }
        auto paramType = declSpec(token);
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
