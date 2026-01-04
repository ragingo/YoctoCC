#pragma once
#include <memory>

namespace yoctocc {

struct Node;
struct Token;

enum class TypeKind {
    INT,
    POINTER,
    FUNCTION,
    ARRAY,
};

struct Type {
    TypeKind kind;

    // sizeof()
    int size;

    // array type
    int arraySize;

    // pointer type and array base type
    std::shared_ptr<Type> base;

    // declaration
    std::shared_ptr<Token> name;

    // function type
    std::shared_ptr<Type> returnType;
    std::shared_ptr<Type> parameters;
    std::shared_ptr<Type> next;

    Type(TypeKind kind, int size = 0): kind(kind), size(size), arraySize(0), base(nullptr) {}
};

namespace type {
    inline std::shared_ptr<Type> intType() {
        return std::make_shared<Type>(TypeKind::INT, 8);
    }

    inline bool isInteger(const std::shared_ptr<Type>& type) {
        return type && type->kind == TypeKind::INT;
    }

    std::shared_ptr<Type> pointerTo(const std::shared_ptr<Type>& base);
    std::shared_ptr<Type> functionType(const std::shared_ptr<Type>& returnType);
    std::shared_ptr<Type> arrayOf(const std::shared_ptr<Type>& base, int size);
    void addType(const std::shared_ptr<Node>& node);
}

} // namespace yoctocc
