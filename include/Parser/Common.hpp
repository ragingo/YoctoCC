#pragma once
#include <memory>
#include <vector>

namespace yoctocc {

struct Member;
struct Node;
struct Object;
struct Token;
struct Type;

struct VariableAttribute {
    bool isTypeDef = false;
    bool isStatic = false;
    bool isExtern = false;
    int alignment = 0;
};

struct Initializer {
    Initializer* next;
    std::shared_ptr<Type> type;
    Token* token;
    bool isFlexibleArray;
    std::unique_ptr<Node> expression;
    std::vector<std::unique_ptr<Initializer>> children;
};

struct InitDesignator {
    const InitDesignator* next;
    int index;
    Member* member;
    Object* variable;
};

std::unique_ptr<Initializer> createInitializer(const std::shared_ptr<Type>& type, bool isFlexibleArray = false);

} // namespace yoctocc
