#pragma once
#include <memory>

namespace yoctocc {

struct Function;
struct Node;
struct Object;
struct Token;
struct Type;

class Parser final {
public:
    std::shared_ptr<Function> parse(std::shared_ptr<Token>& token);

private:
    std::shared_ptr<Object> findLocalVariable(std::shared_ptr<Token>& token);
    std::shared_ptr<Object> createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type);
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
    std::shared_ptr<Node> parseFunctionCall(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Function> parseFunction(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
    std::shared_ptr<Node> parsePrimary(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token);
private:
    std::shared_ptr<Object> _locals;
};

} // namespace yoctocc
