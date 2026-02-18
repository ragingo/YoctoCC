#pragma once
#include <memory>
#include "ParseScope.hpp"

namespace yoctocc {

struct Token;
struct Type;

class ParseDecl final {
public:
    ParseDecl(ParseScope& scope) : _scope(scope) {}

    // struct-members = (declspec declarator (","  declarator)* ";")*
    void structMembers(Token*& token, std::shared_ptr<Type>& structType);
    // struct-decl = ident? "{" struct-members
    const std::shared_ptr<Type> structDecl(Token*& token);
    // declspec = "char" | "int" | struct-decl
    const std::shared_ptr<Type> declSpec(Token*& token);
    // declarator = "*"* ident type-suffix
    const std::shared_ptr<Type> declarator(Token*& token, std::shared_ptr<Type>& type);
    // func-params = (param ("," param)*)? ")"
    // param       = declspec declarator
    const std::shared_ptr<Type> functionParameters(Token*& token, std::shared_ptr<Type>& type);
    // type-suffix = "(" func-params
    //             | "[" num "]" type-suffix
    //             | ε
    const std::shared_ptr<Type> typeSuffix(Token*& token, std::shared_ptr<Type>& type);

private:
    ParseScope& _scope;
};

} // namespace yoctocc
