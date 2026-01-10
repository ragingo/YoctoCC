#pragma once
#include <memory>

namespace yoctocc {

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
        ADDRESS,       // unary &
        DEREFERENCE,   // unary *
        RETURN,        // return
        IF,            // if
        FOR,           // for or while
        BLOCK,         // { ... }
        FUNCTION_CALL,
        VARIABLE,
        EXPRESSION_STATEMENT,
        NUMBER,
    };

    struct Object {
        // local or global variable/function
        bool isLocal;
        // global variable or function
        bool isFunction;
        // global variable
        std::string initialData;
        // local variable
        int offset;
        std::string name;
        std::shared_ptr<Type> type;

        // function
        std::shared_ptr<Object> parameters;
        std::shared_ptr<Node> body;
        std::shared_ptr<Object> locals;
        int stackSize;

        std::shared_ptr<Object> next;
    };

    inline std::shared_ptr<Object> makeVariable(const std::string& name, const std::shared_ptr<Type>& type, bool isLocal) {
        auto var = std::make_shared<Object>();
        var->isLocal = isLocal;
        var->isFunction = false;
        var->name = name;
        var->type = type;
        var->stackSize = 0;
        return var;
    }

    inline std::shared_ptr<Object> makeFunction(const std::string& name, const std::shared_ptr<Type>& returnType) {
        auto func = std::make_shared<Object>();
        func->isLocal = false;
        func->isFunction = true;
        func->name = name;
        func->type = returnType;
        func->stackSize = 0;
        return func;
    }

    struct Node {
        NodeType nodeType;
        int value;
        std::shared_ptr<Type> type;
        std::shared_ptr<Token> token;
        std::shared_ptr<Node> left;
        std::shared_ptr<Node> right;
        std::shared_ptr<Node> next;
        // if or for
        std::shared_ptr<Node> condition;
        std::shared_ptr<Node> then;
        std::shared_ptr<Node> els;
        std::shared_ptr<Node> init;
        std::shared_ptr<Node> inc;
        // block
        std::shared_ptr<Node> body;
        std::shared_ptr<Object> variable;
        // function call
        std::string functionName;
        std::shared_ptr<Node> arguments;

        Node(NodeType type = NodeType::UNKNOWN): nodeType(type), value(0) {}
    };

} // namespace yoctocc
