#pragma once
#include "Common.hpp"
#include "ParseScope.hpp"
#include <memory>

namespace yoctocc {

struct Token;
struct Type;

class ParseDecl final {
public:
    ParseDecl(ParseScope& scope) : _scope(scope) {
    }

    // struct-members = (declspec declarator (","  declarator)* ";")*
    void structMembers(Token*& token, std::shared_ptr<Type>& structType);
    // struct-decl = struct-union-decl
    std::shared_ptr<Type> structDecl(Token*& token);
    // union-decl = struct-union-decl
    std::shared_ptr<Type> unionDecl(Token*& token);
    // struct-union-decl = ident? ("{" struct-members)?
    std::shared_ptr<Type> structUnionDecl(Token*& token);
    // enum-specifier = ident? "{" enum-list? "}"
    //                | ident ("{" enum-list? "}")?
    //
    // enum-list      = ident ("=" num)? ("," ident ("=" num)?)*
    std::shared_ptr<Type> enumSpecifier(Token*& token);
    // declspec = ("void" | "_Bool" | "char" | "short" | "int" | "long"
    //             | "typedef"
    //             | struct-decl | union-decl | typedef-name
    //             | enum-specifier)+
    std::shared_ptr<Type> declSpec(Token*& token, VariableAttribute* attr);
    // abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
    std::shared_ptr<Type> abstractDeclarator(Token*& token, std::shared_ptr<Type>& type);
    // declarator = "*"* ident type-suffix
    std::shared_ptr<Type> declarator(Token*& token, const std::shared_ptr<Type>& baseType);
    // type-name = declspec abstract-declarator
    std::shared_ptr<Type> typeName(Token*& token);
    // func-params = (param ("," param)*)? ")"
    // param       = declspec declarator
    std::shared_ptr<Type> functionParameters(Token*& token, std::shared_ptr<Type>& type);
    // type-suffix = "(" func-params
    //             | "[" num "]" type-suffix
    //             | ε
    std::shared_ptr<Type> typeSuffix(Token*& token, std::shared_ptr<Type>& type);

private:
    ParseScope& _scope;
};

} // namespace yoctocc
