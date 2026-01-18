#include "Node/NodeUtil.hpp"

#include "Logger.hpp"
#include "Node/NodeTypes.hpp"
#include "Token.hpp"
#include "Type.hpp"

using namespace std::literals;

namespace yoctocc {

std::shared_ptr<Node> createNumberNode(const std::shared_ptr<Token>& token, int value) {
    auto node = std::make_shared<Node>(NodeType::NUMBER);
    node->value = value;
    node->token = token;
    return node;
}

std::shared_ptr<Node> createUnaryNode(NodeType type, const std::shared_ptr<Token>& token, const std::shared_ptr<Node>& operand) {
    auto node = std::make_shared<Node>(type);
    node->left = operand;
    node->token = token;
    return node;
}

std::shared_ptr<Node> createBinaryNode(NodeType type, const std::shared_ptr<Token>& token, const std::shared_ptr<Node>& left, const std::shared_ptr<Node>& right) {
    auto node = std::make_shared<Node>(type);
    node->left = left;
    node->right = right;
    node->token = token;
    return node;
}

std::shared_ptr<Node> createVariableNode(const std::shared_ptr<Token>& token, const std::shared_ptr<Object>& variable) {
    auto node = std::make_shared<Node>(NodeType::VARIABLE);
    node->variable = variable;
    node->token = token;
    return node;
}

std::shared_ptr<Node> createBlockNode(const std::shared_ptr<Token>& token, const std::shared_ptr<Node>& body) {
    auto node = std::make_shared<Node>(NodeType::BLOCK);
    node->body = body;
    node->token = token;
    return node;
}

std::shared_ptr<Node> createAddNode(const std::shared_ptr<Token>& token, const std::shared_ptr<Node>& left, const std::shared_ptr<Node>& right) {
    type::addType(left);
    type::addType(right);

    // number + number
    if (type::isInteger(left->type) && type::isInteger(right->type)) {
        return createBinaryNode(NodeType::ADD, token, left, right);
    }

    // pointer + pointer (error)
    if (left->type->base && right->type->base) {
        Log::error("Invalid addition of two pointers"sv, token);
        return nullptr;
    }

    // not pointer + pointer (swap)
    auto newLeft = left;
    auto newRight = right;
    if (!left->type->base && right->type->base) {
        std::swap(newLeft, newRight);
    }

    // pointer + number
    newRight = createBinaryNode(NodeType::MUL, token, newRight, createNumberNode(token, newLeft->type->base->size));
    return createBinaryNode(NodeType::ADD, token, newLeft, newRight);
}

std::shared_ptr<Node> createSubNode(const std::shared_ptr<Token>& token, const std::shared_ptr<Node>& left, const std::shared_ptr<Node>& right) {
    type::addType(left);
    type::addType(right);

    // number - number
    if (type::isInteger(left->type) && type::isInteger(right->type)) {
        return createBinaryNode(NodeType::SUB, token, left, right);
    }

    // pointer - number
    if (left->type->base && type::isInteger(right->type)) {
        auto newRight = createBinaryNode(NodeType::MUL, token, right, createNumberNode(token, left->type->base->size));
        type::addType(newRight);
        auto node = createBinaryNode(NodeType::SUB, token, left, newRight);
        node->type = left->type;
        return node;
    }

    // pointer - pointer
    if (left->type->base && right->type->base) {
        auto node = createBinaryNode(NodeType::SUB, token, left, right);
        node->type = std::make_shared<Type>(TypeKind::INT);
        return createBinaryNode(NodeType::DIV, token, node, createNumberNode(token, left->type->base->size));
    }

    Log::error("Invalid subtraction involving pointers"sv, token);
    return nullptr;
}

} // namespace yoctocc
