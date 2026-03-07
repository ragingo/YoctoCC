#pragma once
#include <cassert>
#include <memory>
#include <string>

namespace yoctocc {

struct Object;
struct Token;
struct Type;

struct VariableScope {
    std::string name;
    std::unique_ptr<VariableScope> next;
    Object* variable = nullptr;
    std::shared_ptr<Type> typeDef;
};

struct TagScope {
    std::string name;
    std::shared_ptr<Type> type;
    std::unique_ptr<TagScope> next;
};

struct Scope {
    std::unique_ptr<VariableScope> variables;
    std::unique_ptr<TagScope> tags;
    std::unique_ptr<Scope> next;
};

class ParseScope final {
public:
    void enterScope();
    void leaveScope();

    VariableScope* pushVariableScope(const std::string& name);
    VariableScope* findVariable(const Token* token) const;

    void pushTagScope(const std::string& name, std::shared_ptr<Type> type);
    std::shared_ptr<Type> findTag(const Token* token) const;

    std::shared_ptr<Type> findTypeDef(const Token* token) const;

    inline Scope* currentScope() const {
        return _currentScope.get();
    }

private:
    std::unique_ptr<Scope> _currentScope;
};

} // namespace yoctocc
