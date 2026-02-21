#pragma once
#include <functional>
#include <memory>
#include "Token.hpp"

namespace yoctocc {

struct Member;
struct Node;

enum class TypeKind {
    CHAR,
    INT,
    POINTER,
    FUNCTION,
    ARRAY,
    STRUCT,
    UNION,
    UNKNOWN,
};

struct Type {
    TypeKind kind;

    // sizeof()
    int size;

    int alignment;

    // array type
    int arraySize;

    // pointer type and array base type
    std::shared_ptr<Type> base;

    // declaration
    const Token* name = nullptr;

    // struct type
    std::shared_ptr<Member> members;

    // function type
    std::shared_ptr<Type> returnType;
    std::shared_ptr<Type> parameters;
    std::shared_ptr<Type> next;

    Type(TypeKind kind, int size = 0, int alignment = 0): kind(kind), size(size), alignment(alignment), arraySize(0), base(nullptr) {}
};

enum class DataType {
    CHAR,
    INT,
    STRUCT,
    UNION,
    UNKNOWN,
};

constexpr std::string to_string(DataType type) {
    switch (type) {
        case DataType::CHAR:
            return "char";
        case DataType::INT:
            return "int";
        case DataType::STRUCT:
            return "struct";
        case DataType::UNION:
            return "union";
        default:
            return "???";
    }
}

constexpr DataType to_data_type(const std::string& str) {
    if (str == "char") {
        return DataType::CHAR;
    } else if (str == "int") {
        return DataType::INT;
    } else if (str == "struct") {
        return DataType::STRUCT;
    } else if (str == "union") {
        return DataType::UNION;
    } else {
        return DataType::UNKNOWN;
    }
}

namespace type {
    using enum TypeKind;

    inline std::shared_ptr<Type> charType() {
        return std::make_shared<Type>(CHAR, 1, 1);
    }

    inline std::shared_ptr<Type> intType() {
        return std::make_shared<Type>(INT, 4, 4);
    }

    template <typename T>
        requires std::same_as<std::remove_cv_t<T>, Type*>
            || std::same_as<std::remove_cv_t<T>, const Type*>
            || std::same_as<std::remove_cv_t<T>, std::shared_ptr<Type>>
    inline bool is(const T& type, TypeKind kind) {
        return type && type->kind == kind;
    }

    template <typename T>
        requires std::same_as<std::remove_cv_t<T>, Type*>
            || std::same_as<std::remove_cv_t<T>, const Type*>
            || std::same_as<std::remove_cv_t<T>, std::shared_ptr<Type>>
    inline bool is(const T& type, std::function<bool(TypeKind)> predicate) {
        return type && predicate(type->kind);
    }

    inline bool isInteger(const std::shared_ptr<Type>& type) {
        return is(type, [](TypeKind kind) { return kind == CHAR || kind == INT; });
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
