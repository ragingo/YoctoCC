#include "Type.hpp"

#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"

using namespace std::literals;

namespace yoctocc::type {

    std::shared_ptr<Type> pointerTo(const std::shared_ptr<Type>& base) {
        auto type = std::make_shared<Type>(TypeKind::POINTER, 8, 8);
        type->base = base;
        return type;
    }

    std::shared_ptr<Type> functionType(const std::shared_ptr<Type>& returnType) {
        auto type = std::make_shared<Type>(TypeKind::FUNCTION);
        type->returnType = returnType;
        return type;
    }

    std::shared_ptr<Type> arrayOf(const std::shared_ptr<Type>& base, int size) {
        auto type = std::make_shared<Type>(TypeKind::ARRAY, base->size * size, base->alignment);
        type->base = base;
        type->arraySize = size;
        return type;
    }

    void addType(Node* node) {
        if (!node || node->type) {
            return;
        }

        addType(node->left.get());
        addType(node->right.get());
        addType(node->condition.get());
        addType(node->then.get());
        addType(node->els.get());
        addType(node->init.get());
        addType(node->inc.get());

        for (Node* body = node->body.get(); body; body = body->next.get()) {
            addType(body);
        }

        for (Node* arguments = node->arguments.get(); arguments; arguments = arguments->next.get()) {
            addType(arguments);
        }

        switch (node->nodeType) {
            case NodeType::ADD:
            case NodeType::SUB:
            case NodeType::MUL:
            case NodeType::DIV:
            case NodeType::NEGATE:
                node->type = node->left->type;
                return;
            case NodeType::ASSIGN:
                if (node->left->type->kind == TypeKind::ARRAY) {
                    Log::error("not an lvalue"sv, node->token);
                    return;
                }
                node->type = node->left->type;
                return;
            case NodeType::EQUAL:
            case NodeType::NOT_EQUAL:
            case NodeType::LESS:
            case NodeType::LESS_EQUAL:
            case NodeType::GREATER:
            case NodeType::GREATER_EQUAL:
            case NodeType::NUMBER:
            case NodeType::FUNCTION_CALL:
                node->type = std::make_shared<Type>(TypeKind::INT);
                return;
            case NodeType::VARIABLE:
                node->type = node->variable->type;
                return;
            case NodeType::COMMA:
                node->type = node->right->type;
                return;
            case NodeType::MEMBER:
                node->type = node->member->type;
                return;
            case NodeType::ADDRESS:
                if (node->left->type->kind == TypeKind::ARRAY) {
                    node->type = pointerTo(node->left->type->base);
                } else {
                    node->type = pointerTo(node->left->type);
                }
                return;
            case NodeType::DEREFERENCE:
                if (!node->left->type || !node->left->type->base) {
                    Log::error("Invalid pointer dereference"sv, node->token);
                    return;
                }
                node->type = node->left->type->base;
                return;
            case NodeType::STATEMENT_EXPRESSION:
                if (node->body) {
                    Node* stmt = node->body.get();
                    while (stmt->next) {
                        stmt = stmt->next.get();
                    }
                    if (stmt->nodeType == NodeType::EXPRESSION_STATEMENT) {
                        node->type = stmt->left->type;
                        return;
                    }
                }
                Log::error("statement expression returning void is not supported"sv, node->token);
                return;
            case NodeType::BLOCK:
            case NodeType::IF:
            case NodeType::FOR:
            case NodeType::EXPRESSION_STATEMENT:
            case NodeType::RETURN:
            case NodeType::UNKNOWN:
                // skip
                return;
        }
    }

} // namespace yoctocc::type
