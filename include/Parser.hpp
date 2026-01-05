#pragma once
#include <memory>

namespace yoctocc {

struct Node;
struct Object;
struct Token;
struct Type;

class Parser final {
public:
    std::shared_ptr<Object> parse(std::shared_ptr<Token>& token);

private:
    std::shared_ptr<Object> findVariable(std::shared_ptr<Token>& token);
    std::shared_ptr<Object> createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    std::shared_ptr<Object> createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    std::shared_ptr<Node> declaration(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseExpression(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseAssignment(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseCompoundStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseExpressionStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseEquality(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseRelational(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseAdditive(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseMultiply(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseUnary(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parsePostfix(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parseFunctionCall(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Token> parseFunction(std::shared_ptr<Token>& result, std::shared_ptr<Type>& baseType);
    std::shared_ptr<Token> parseGlobalVariable(std::shared_ptr<Token>& token, std::shared_ptr<Type>& baseType);
    std::shared_ptr<Node> parsePrimary(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    void applyParamLVars(const std::shared_ptr<Type>& parameter);
private:
    std::shared_ptr<Object> _locals;
    std::shared_ptr<Object> _globals;
};

} // namespace yoctocc
