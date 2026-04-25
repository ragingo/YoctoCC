#pragma once
#include "Node/Keywords.hpp"
#include "Token.hpp"
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_set>

namespace yoctocc {

struct Member;
struct Node;

enum class TypeKind {
    VOID,
    BOOL,
    CHAR,
    SHORT,
    INT,
    LONG,
    ENUM,
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

    Type(TypeKind kind, int size = 0, int alignment = 0)
        : kind(kind), size(size), alignment(alignment), arraySize(0), base(nullptr) {
    }
};

namespace type {
using enum TypeKind;

inline std::shared_ptr<Type> voidType() {
    return std::make_shared<Type>(VOID, 1, 1);
}

inline std::shared_ptr<Type> boolType() {
    return std::make_shared<Type>(BOOL, 1, 1);
}

inline std::shared_ptr<Type> charType() {
    return std::make_shared<Type>(CHAR, 1, 1);
}

inline std::shared_ptr<Type> shortType() {
    return std::make_shared<Type>(SHORT, 2, 2);
}

inline std::shared_ptr<Type> intType() {
    return std::make_shared<Type>(INT, 4, 4);
}

inline std::shared_ptr<Type> longType() {
    return std::make_shared<Type>(LONG, 8, 8);
}

inline std::shared_ptr<Type> enumType() {
    return std::make_shared<Type>(ENUM, 4, 4);
}

template <typename T>
// clang-format off
    requires std::same_as<std::remove_cv_t<T>, Type*>
        || std::same_as<std::remove_cv_t<T>, const Type*>
        || std::same_as<std::remove_cv_t<T>, std::shared_ptr<Type>>
// clang-format on
inline bool is(const T& type, TypeKind kind) {
    return type && type->kind == kind;
}

template <typename T>
// clang-format off
    requires std::same_as<std::remove_cv_t<T>, Type*>
        || std::same_as<std::remove_cv_t<T>, const Type*>
        || std::same_as<std::remove_cv_t<T>, std::shared_ptr<Type>>
// clang-format on
inline bool is(const T& type1, const T& type2) {
    return type1 && type2 && type1->kind == type2->kind;
}

template <typename T>
// clang-format off
    requires std::same_as<std::remove_cv_t<T>, Type*>
        || std::same_as<std::remove_cv_t<T>, const Type*>
        || std::same_as<std::remove_cv_t<T>, std::shared_ptr<Type>>
// clang-format on
inline bool is(const T& type, std::function<bool(TypeKind)> predicate) {
    return type && predicate(type->kind);
}


inline bool isInteger(const Type* type) {
    return is(type, [](TypeKind kind) {
        return kind == BOOL || kind == CHAR || kind == SHORT || kind == INT || kind == LONG || kind == ENUM;
    });
}

inline bool isTypeName(const Token* token) {
    if (!token) {
        return false;
    }
    using enum Keyword;
    static const std::unordered_set<std::string_view> TYPE_NAMES = {
        to_string_view(VOID),
        to_string_view(BOOL),
        to_string_view(CHAR),
        to_string_view(SHORT),
        to_string_view(INT),
        to_string_view(LONG),
        to_string_view(STRUCT),
        to_string_view(UNION),
        to_string_view(ENUM),
        to_string_view(TYPEDEF),
    };
    return TYPE_NAMES.contains(token->originalValue);
}

std::shared_ptr<Type> pointerTo(const std::shared_ptr<Type>& base);
std::shared_ptr<Type> functionType(const std::shared_ptr<Type>& returnType);
std::shared_ptr<Type> arrayOf(const std::shared_ptr<Type>& base, int size);
void addType(Node* node);
} // namespace type

} // namespace yoctocc
