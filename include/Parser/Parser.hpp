#pragma once
#include "ParseScope.hpp"
#include <cassert>
#include <memory>

namespace yoctocc {

struct Initializer;
struct Node;
struct Object;
struct Token;
struct Type;
struct VariableAttribute;

struct ParseResult {
    std::unique_ptr<Node> node;
    Token* rest = nullptr;
};

class Parser final {
public:
    std::unique_ptr<Object> parse(Token* token);

// Decl
private:
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
    // enum-list      = ident ("=" num)? ("," ident ("=" num)?)* ","?
    std::shared_ptr<Type> enumSpecifier(Token*& token);
    // declspec = ("void" | "_Bool" | "char" | "short" | "int" | "long"
    //             | "typedef" | "static" | "extern"
    //             | "signed"
    //             | struct-decl | union-decl | typedef-name
    //             | enum-specifier)+
    std::shared_ptr<Type> declSpec(Token*& token, VariableAttribute* attr);
    // abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
    std::shared_ptr<Type> abstractDeclarator(Token*& token, std::shared_ptr<Type>& type);
    // declarator = "*"* ident type-suffix
    std::shared_ptr<Type> declarator(Token*& token, const std::shared_ptr<Type>& baseType);
    // type-name = declspec abstract-declarator
    std::shared_ptr<Type> typeName(Token*& token);
    // func-params = ("void" | param ("," param)* ("," "...")?)? ")"
    // param       = declspec declarator
    std::shared_ptr<Type> functionParameters(Token*& token, std::shared_ptr<Type>& type);
    // array-dimensions = num? "]" type-suffix
    std::shared_ptr<Type> arrayDimensions(Token*& token, std::shared_ptr<Type>& type);
    // type-suffix = "(" func-params
    //             | "[" array-dimensions
    //             | ε
    std::shared_ptr<Type> typeSuffix(Token*& token, std::shared_ptr<Type>& type);

private:
    bool isFunction(Token* token);
    Object* createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    Object* createTemporaryLocalVariable(const std::shared_ptr<Type>& type);
    Object* createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    Object* createGlobalAnonymousVariable(const std::shared_ptr<Type>& type);
    int64_t constExpression(Token*& token);
    std::unique_ptr<Node> toAssign(std::unique_ptr<Node>&& binary);
    std::unique_ptr<Node> createIncDecNode(const Token* token, std::unique_ptr<Node> node, bool isInc);
    ParseResult createBitAndNode(Token* token);
    ParseResult createBitOrNode(Token* token);
    ParseResult createBitXorNode(Token* token);
    ParseResult createLogicalAndNode(Token* token);
    ParseResult createLogicalOrNode(Token* token);
    ParseResult declaration(Token* token, const std::shared_ptr<Type>& baseType, const VariableAttribute* attr);
    ParseResult parseVariableInitializer(Token* token, Object* variable);
    std::unique_ptr<Initializer> parseInitializer(Token*& token, std::shared_ptr<Type>& type);
    void stringInitializer(Token*& token, std::unique_ptr<Initializer>& initializer);
    int countElements(Token* token, std::shared_ptr<Type> type);
    void arrayInitializer1(Token*& token, std::unique_ptr<Initializer>& initializer);
    void arrayInitializer2(Token*& token, std::unique_ptr<Initializer>& initializer);
    void structInitializer1(Token*& token, std::unique_ptr<Initializer>& initializer);
    void structInitializer2(Token*& token, std::unique_ptr<Initializer>& initializer);
    void unionInitializer(Token*& token, std::unique_ptr<Initializer>& initializer);
    void parseInitializer2(Token*& token, std::unique_ptr<Initializer>& initializer);
    void skipExcessElement(Token*& token);
    void globalVariableInitializer(Token*& token, Object* variable);
    ParseResult parseExpression(Token* token);
    ParseResult parseAssignment(Token* token);
    ParseResult parseConditional(Token* token);
    ParseResult parseStatement(Token* token);
    ParseResult parseCompoundStatement(Token* token);
    ParseResult parseExpressionStatement(Token* token);
    ParseResult parseEquality(Token* token);
    ParseResult parseRelational(Token* token);
    ParseResult parseShift(Token* token);
    ParseResult parseAdditive(Token* token);
    ParseResult parseMultiply(Token* token);
    ParseResult parseCast(Token* token);
    ParseResult parseUnary(Token* token);
    ParseResult parsePostfix(Token* token);
    ParseResult parseFunctionCall(Token* token);
    Token* parseTypeDef(Token* token, std::shared_ptr<Type>& baseType);
    Token* parseFunction(Token* token, std::shared_ptr<Type>& baseType, const VariableAttribute& attr);
    Token* parseGlobalVariable(Token* token, std::shared_ptr<Type>& baseType, const VariableAttribute& attr);
    ParseResult parsePrimary(Token* token);
    void applyParamLVars(const std::shared_ptr<Type>& parameter);
    void resolveGotoLabels();

    std::unique_ptr<Object> _locals;
    std::unique_ptr<Object> _globals;
    std::unique_ptr<Object> _currentFunction;
    Node* _gotos;
    Node* _labels;
    std::string _breakLabel;
    std::string _continueLabel;
    Node* _currentSwitch;
    ParseScope _parseScope;
};

} // namespace yoctocc
