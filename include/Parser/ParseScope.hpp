#pragma once
#include <cassert>
#include <memory>
#include <string>

namespace yoctocc {

struct Object;

struct VariableScope {
    std::string name;
    std::unique_ptr<VariableScope> next;
    Object* variable = nullptr;
};

struct Scope {
    std::unique_ptr<VariableScope> variable;
    std::unique_ptr<Scope> next;
};

class ParseScope final {
public:
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

    Scope* currentScope() const {
        return _currentScope.get();
    }

private:
    std::unique_ptr<Scope> _currentScope;
};

} // namespace yoctocc
