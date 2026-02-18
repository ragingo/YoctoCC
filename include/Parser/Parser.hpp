#pragma once
#include <cassert>
#include <memory>
#include "ParseDecl.hpp"
#include "ParseScope.hpp"

namespace yoctocc {

struct Node;
struct Object;
struct Token;
struct Type;

struct ParseResult {
    std::unique_ptr<Node> node;
    Token* rest;
};

class Parser final {
public:
    std::unique_ptr<Object> parse(Token* token);

private:
    bool isFunction(Token* token);
    Object* createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    Object* createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    ParseResult declaration(Token* token);
    ParseResult parseExpression(Token* token);
    ParseResult parseAssignment(Token* token);
    ParseResult parseStatement(Token* token);
    ParseResult parseCompoundStatement(Token* token);
    ParseResult parseExpressionStatement(Token* token);
    ParseResult parseEquality(Token* token);
    ParseResult parseRelational(Token* token);
    ParseResult parseAdditive(Token* token);
    ParseResult parseMultiply(Token* token);
    ParseResult parseUnary(Token* token);
    ParseResult parsePostfix(Token* token);
    ParseResult parseFunctionCall(Token* token);
    Token* parseFunction(Token* token, std::shared_ptr<Type>& baseType);
    Token* parseGlobalVariable(Token* token, std::shared_ptr<Type>& baseType);
    ParseResult parsePrimary(Token* token);
    void applyParamLVars(const std::shared_ptr<Type>& parameter);

private:
    std::unique_ptr<Object> _locals;
    std::unique_ptr<Object> _globals;
    ParseScope _parseScope;
    ParseDecl _parseDecl {_parseScope};
};

} // namespace yoctocc
