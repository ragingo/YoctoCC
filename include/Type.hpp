#pragma once
#include <memory>
#include "Token.hpp"

namespace yoctocc {

struct Node;

enum class TypeKind {
    CHAR,
    INT,
    POINTER,
    FUNCTION,
    ARRAY,
    UNKNOWN,
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
    const Token* name = nullptr;

    // function type
    std::shared_ptr<Type> returnType;
    std::shared_ptr<Type> parameters;
    std::shared_ptr<Type> next;

    Type(TypeKind kind, int size = 0): kind(kind), size(size), arraySize(0), base(nullptr) {}
};

enum class DataType {
    CHAR,
    INT,
    UNKNOWN,
};

constexpr std::string to_string(DataType type) {
    switch (type) {
        case DataType::CHAR:
            return "char";
        case DataType::INT:
            return "int";
        default:
            return "???";
    }
}

constexpr DataType to_data_type(const std::string& str) {
    if (str == "char") {
        return DataType::CHAR;
    } else if (str == "int") {
        return DataType::INT;
    } else {
        return DataType::UNKNOWN;
    }
}

namespace type {
    inline std::shared_ptr<Type> charType() {
        return std::make_shared<Type>(TypeKind::CHAR, 1);
    }

    inline std::shared_ptr<Type> intType() {
        return std::make_shared<Type>(TypeKind::INT, 8);
    }

    inline bool isInteger(const std::shared_ptr<Type>& type) {
        return type && (type->kind == TypeKind::CHAR || type->kind == TypeKind::INT);
    }

    inline bool isTypeName(const Token* token) {
        if (!token) {
            return false;
        }
        return to_data_type(token->originalValue) != DataType::UNKNOWN;
    }

    std::shared_ptr<Type> pointerTo(const std::shared_ptr<Type>& base);
    std::shared_ptr<Type> functionType(const std::shared_ptr<Type>& returnType);
    std::shared_ptr<Type> arrayOf(const std::shared_ptr<Type>& base, int size);
    void addType(Node* node);
}

} // namespace yoctocc
