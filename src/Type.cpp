#include "Type.hpp"

#include <tuple>
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"

using namespace std::literals;

namespace {
    using namespace yoctocc;

    std::shared_ptr<Type> getCommonType(const std::shared_ptr<Type>& type1, const std::shared_ptr<Type>& type2) {
        if (type1->base) {
            return type::pointerTo(type1->base);
        }
        if (type1->size == 8 || type2->size == 8) {
            return type::longType();
        }
        return type::intType();
    }

    std::tuple<std::unique_ptr<Node>, std::unique_ptr<Node>> convertUsualArithmetic(std::unique_ptr<Node> lhs, std::unique_ptr<Node> rhs) {
        auto type = getCommonType(lhs->type, rhs->type);
        auto newLhsNode = createCastNode(std::move(lhs), type);
        auto newRhsNode = createCastNode(std::move(rhs), type);
        return {std::move(newLhsNode), std::move(newRhsNode)};
    }
}

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
            case NodeType::NUMBER:
                node->type = (node->value == static_cast<int64_t>(static_cast<int32_t>(node->value))) ? type::intType() : type::longType();
                return;
            case NodeType::ADD:
            case NodeType::SUB:
            case NodeType::MUL:
            case NodeType::DIV:
                {
                    auto [newLhsNode, newRhsNode] = convertUsualArithmetic(std::move(node->left), std::move(node->right));
                    node->left = std::move(newLhsNode);
                    node->right = std::move(newRhsNode);
                    node->type = node->left->type;
                }
                return;
            case NodeType::NEGATE:
                {
                    auto type = getCommonType(type::intType(), node->left->type);
                    node->left = createCastNode(std::move(node->left), type);
                    node->type = type;
                }
                return;
            case NodeType::ASSIGN:
                if (node->left->type->kind == TypeKind::ARRAY) {
                    Log::error("not an lvalue"sv, node->token);
                    return;
                }
                if (node->left->type->kind != TypeKind::STRUCT) {
                    node->right = createCastNode(std::move(node->right), node->left->type);
                }
                node->type = node->left->type;
                return;
            case NodeType::EQUAL:
            case NodeType::NOT_EQUAL:
            case NodeType::LESS:
            case NodeType::LESS_EQUAL:
            case NodeType::GREATER:
            case NodeType::GREATER_EQUAL:
                {
                    auto [newLhsNode, newRhsNode] = convertUsualArithmetic(std::move(node->left), std::move(node->right));
                    node->left = std::move(newLhsNode);
                    node->right = std::move(newRhsNode);
                }
                node->type = type::intType();
                break;
            case NodeType::FUNCTION_CALL:
                node->type = type::longType();
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
                if (node->left->type->kind == TypeKind::VOID) {
                    Log::error("cannot dereference void pointer"sv, node->token);
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
            case NodeType::CAST:
                // skip
                return;
        }
    }

} // namespace yoctocc::type
