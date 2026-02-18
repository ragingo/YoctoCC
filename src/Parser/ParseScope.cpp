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

    void ParseScope::pushVariableScope(const std::string& name, Object* variable) {
        if (!_currentScope) {
            _currentScope = std::make_unique<Scope>();
        }
        auto variableScope = std::make_unique<VariableScope>();
        variableScope->name = name;
        variableScope->variable = variable;
        variableScope->next = std::move(_currentScope->variables);
        _currentScope->variables = std::move(variableScope);
    }

    Object* ParseScope::findVariable(const Token* token) {
        for (Scope* scope = _currentScope.get(); scope; scope = scope->next.get()) {
            for (VariableScope* variableScope = scope->variables.get(); variableScope; variableScope = variableScope->next.get()) {
                if (variableScope->name == token->originalValue) {
                    return variableScope->variable;
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

    std::shared_ptr<Type> ParseScope::findTag(const Token* token) {
        for (Scope* scope = _currentScope.get(); scope; scope = scope->next.get()) {
            for (TagScope* tagScope = scope->tags.get(); tagScope; tagScope = tagScope->next.get()) {
                if (tagScope->name == token->originalValue) {
                    return tagScope->type;
                }
            }
        }
        return nullptr;
    }

} // namespace yoctocc
