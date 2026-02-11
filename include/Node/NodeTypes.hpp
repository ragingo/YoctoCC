#pragma once
#include <memory>

namespace yoctocc {

    struct Member;
    struct Node;
    struct Object;
    struct Token;
    struct Type;

    enum class NodeType {
        UNKNOWN,
        ADD,           // +
        SUB,           // -
        MUL,           // *
        DIV,           // /
        NEGATE,        // unary -
        EQUAL,         // ==
        NOT_EQUAL,     // !=
        LESS,          // <
        LESS_EQUAL,    // <=
        GREATER,       // >
        GREATER_EQUAL, // >=
        ASSIGN,        // =
        COMMA,         // ,
        MEMBER,        // .
        ADDRESS,       // unary &
        DEREFERENCE,   // unary *
        RETURN,        // return
        IF,            // if
        FOR,           // for or while
        BLOCK,         // { ... }
        FUNCTION_CALL,
        VARIABLE,
        EXPRESSION_STATEMENT,
        STATEMENT_EXPRESSION,
        NUMBER,
    };

    struct Node {
        NodeType nodeType;
        int value = 0;
        std::shared_ptr<Type> type;
        const Token* token = nullptr;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        std::unique_ptr<Node> next;
        // if or for
        std::unique_ptr<Node> condition;
        std::unique_ptr<Node> then;
        std::unique_ptr<Node> els;
        std::unique_ptr<Node> init;
        std::unique_ptr<Node> inc;
        // block or statement expression
        std::unique_ptr<Node> body;
        Object* variable = nullptr;
        // struct
        std::unique_ptr<Member> member;
        // function call
        std::string functionName;
        std::unique_ptr<Node> arguments;

        Node(NodeType type, const Token* token): nodeType(type), token(token) {}
    };

    struct Object {
        // local or global variable/function
        bool isLocal = false;
        // global variable or function
        bool isFunction = false;
        // global variable
        std::string initialData;
        // local variable
        int offset = 0;
        std::string name;
        std::shared_ptr<Type> type;

        // function
        Object* parameters = nullptr;
        std::unique_ptr<Node> body;
        std::unique_ptr<Object> locals;
        int stackSize = 0;

        std::unique_ptr<Object> next;
    };

    struct Member {
        const Token* name = nullptr;
        std::shared_ptr<Type> type;
        int offset = 0;
        std::unique_ptr<Member> next;
    };

    inline std::unique_ptr<Object> makeVariable(const std::string& name, const std::shared_ptr<Type>& type, bool isLocal) {
        auto var = std::make_unique<Object>();
        var->isLocal = isLocal;
        var->name = name;
        var->type = type;
        return var;
    }

    inline std::unique_ptr<Object> makeFunction(const std::string& name, const std::shared_ptr<Type>& returnType) {
        auto func = std::make_unique<Object>();
        func->isFunction = true;
        func->name = name;
        func->type = returnType;
        return func;
    };

} // namespace yoctocc
