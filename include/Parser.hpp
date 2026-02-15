#pragma once
#include <cassert>
#include <memory>

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
    Object* findVariable(const Token* token);
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

    struct VariableScope {
        std::string name;
        std::unique_ptr<VariableScope> next;
        Object* variable = nullptr;
    };

    struct Scope {
        std::unique_ptr<VariableScope> variable;
        std::unique_ptr<Scope> next;
    };

    void enterScope() {
        if (!_currentScope) {
            _currentScope = std::make_unique<Scope>();
            return;
        }
        auto scope = std::make_unique<Scope>();
        scope->next = std::move(_currentScope);
        _currentScope = std::move(scope);
    }

    void leaveScope() {
        assert(_currentScope);
        _currentScope = std::move(_currentScope->next);
    }

    void pushVariableScope(const std::string& name, Object* variable) {
        if (!_currentScope) {
            _currentScope = std::make_unique<Scope>();
        }
        auto variableScope = std::make_unique<VariableScope>();
        variableScope->name = name;
        variableScope->variable = variable;
        variableScope->next = std::move(_currentScope->variable);
        _currentScope->variable = std::move(variableScope);
    }

private:
    std::unique_ptr<Object> _locals;
    std::unique_ptr<Object> _globals;
    std::unique_ptr<Scope> _currentScope;
};

} // namespace yoctocc
