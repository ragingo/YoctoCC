#pragma once
#include "ParseDecl.hpp"
#include "ParseScope.hpp"
#include <cassert>
#include <memory>

namespace yoctocc {

struct Node;
struct Object;
struct Token;
struct Type;

struct ParseResult {
    std::unique_ptr<Node> node;
    Token* rest = nullptr;
};

class Parser final {
public:
    std::unique_ptr<Object> parse(Token* token);

private:
    bool isFunction(Token* token);
    Object* createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    Object* createTemporaryLocalVariable(const std::shared_ptr<Type>& type);
    Object* createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type);
    std::unique_ptr<Node> toAssign(std::unique_ptr<Node>&& binary);
    std::unique_ptr<Node> createIncDecNode(const Token* token, std::unique_ptr<Node> node, bool isInc);
    ParseResult createBitAndNode(Token* token);
    ParseResult createBitOrNode(Token* token);
    ParseResult createBitXorNode(Token* token);
    ParseResult createLogicalAndNode(Token* token);
    ParseResult createLogicalOrNode(Token* token);


    ParseResult declaration(Token* token, const std::shared_ptr<Type>& baseType);
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
    Token* parseGlobalVariable(Token* token, std::shared_ptr<Type>& baseType);
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
    ParseDecl _parseDecl{_parseScope};
};

} // namespace yoctocc
