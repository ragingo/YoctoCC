#pragma once
#include <memory>

namespace yoctocc {

struct Node;
struct Token;

enum class TypeKind {
    INT,
    POINTER,
    FUNCTION,
};

struct Type {
    TypeKind kind;
    std::shared_ptr<Type> base;
    std::shared_ptr<Token> name;
    std::shared_ptr<Type> returnType;
    std::shared_ptr<Type> parameters;
    std::shared_ptr<Type> next;

    Type(TypeKind kind): kind(kind), base(nullptr) {}
};

namespace type {
    static inline const Type intType{TypeKind::INT};

    inline bool isInteger(const std::shared_ptr<Type>& type) {
        return type && type->kind == TypeKind::INT;
    }

    std::shared_ptr<Type> pointerTo(const std::shared_ptr<Type>& base);

    std::shared_ptr<Type> functionType(const std::shared_ptr<Type>& returnType);

    void addType(const std::shared_ptr<Node>& node);
}

} // namespace yoctocc
