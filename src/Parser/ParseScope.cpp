#include "Parser/ParseScope.hpp"

#include "Token.hpp"

namespace yoctocc {

    void ParseScope::enterScope() {
        if (!_currentScope) {
            _currentScope = std::make_unique<Scope>();
            return;
        }
        auto scope = std::make_unique<Scope>();
        scope->next = std::move(_currentScope);
        _currentScope = std::move(scope);
    }

    void ParseScope::leaveScope() {
        assert(_currentScope);
        _currentScope = std::move(_currentScope->next);
    }

    VariableScope* ParseScope::pushVariableScope(const std::string& name) {
        if (!_currentScope) {
            _currentScope = std::make_unique<Scope>();
        }
        auto variableScope = std::make_unique<VariableScope>();
        variableScope->name = name;
        variableScope->variable = nullptr;
        variableScope->next = std::move(_currentScope->variables);
        _currentScope->variables = std::move(variableScope);
        return _currentScope->variables.get();
    }

    VariableScope* ParseScope::findVariable(const Token* token) const {
        for (Scope* scope = _currentScope.get(); scope; scope = scope->next.get()) {
            for (VariableScope* variableScope = scope->variables.get(); variableScope; variableScope = variableScope->next.get()) {
                if (variableScope->name == token->originalValue) {
                    return variableScope;
                }
            }
        }
        return nullptr;
    }

    void ParseScope::pushTagScope(const std::string& name, std::shared_ptr<Type> type) {
        if (!_currentScope) {
            _currentScope = std::make_unique<Scope>();
        }
        auto tagScope = std::make_unique<TagScope>();
        tagScope->name = name;
        tagScope->type = type;
        tagScope->next = std::move(_currentScope->tags);
        _currentScope->tags = std::move(tagScope);
    }

    std::shared_ptr<Type> ParseScope::findTag(const Token* token) const {
        for (Scope* scope = _currentScope.get(); scope; scope = scope->next.get()) {
            for (TagScope* tagScope = scope->tags.get(); tagScope; tagScope = tagScope->next.get()) {
                if (tagScope->name == token->originalValue) {
                    return tagScope->type;
                }
            }
        }
        return nullptr;
    }

    std::shared_ptr<Type> ParseScope::findTypeDef(const Token* token) const {
        if (token->kind != TokenKind::IDENTIFIER) {
            return nullptr;
        }
        auto var = findVariable(token);
        if (var) {
            return var->typeDef;
        }
        return nullptr;
    }

} // namespace yoctocc
