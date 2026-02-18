#include "Node/NodeUtil.hpp"

#include "Logger.hpp"
#include "Node/NodeTypes.hpp"
#include "Token.hpp"
#include "Type.hpp"

using namespace std::literals;

namespace {
    using namespace yoctocc;

    std::unique_ptr<Member> findStructMember(const std::shared_ptr<Type>& structType, const Token* memberName) {
        for (auto member = structType->members.get(); member; member = member->next.get()) {
            if (member->name->originalValue == memberName->originalValue) {
                auto found = std::make_unique<Member>();
                found->name = member->name;
                found->type = member->type;
                found->offset = member->offset;
                return found;
            }
        }
        return nullptr;
    }
}

namespace yoctocc {

using enum TypeKind;

std::unique_ptr<Node> createNumberNode(const Token* token, int value) {
    auto node = std::make_unique<Node>(NodeType::NUMBER, token);
    node->value = value;
    return node;
}

std::unique_ptr<Node> createUnaryNode(NodeType type, const Token* token, std::unique_ptr<Node> operand) {
    auto node = std::make_unique<Node>(type, token);
    node->left = std::move(operand);
    return node;
}

std::unique_ptr<Node> createBinaryNode(NodeType type, const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
    auto node = std::make_unique<Node>(type, token);
    node->left = std::move(left);
    node->right = std::move(right);
    return node;
}

std::unique_ptr<Node> createVariableNode(const Token* token, Object* variable) {
    auto node = std::make_unique<Node>(NodeType::VARIABLE, token);
    node->variable = variable;
    return node;
}

std::unique_ptr<Node> createBlockNode(const Token* token, std::unique_ptr<Node> body) {
    auto node = std::make_unique<Node>(NodeType::BLOCK, token);
    node->body = std::move(body);
    return node;
}

std::unique_ptr<Node> createAddNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
    type::addType(left.get());
    type::addType(right.get());

    // number + number
    if (type::isInteger(left->type) && type::isInteger(right->type)) {
        return createBinaryNode(NodeType::ADD, token, std::move(left), std::move(right));
    }

    // pointer + pointer (error)
    if (left->type->base && right->type->base) {
        Log::error("Invalid addition of two pointers"sv, token);
        return nullptr;
    }

    // not pointer + pointer (swap)
    if (!left->type->base && right->type->base) {
        std::swap(left, right);
    }

    // pointer + number
    auto newRight = createBinaryNode(NodeType::MUL, token, std::move(right), createNumberNode(token, left->type->base->size));
    return createBinaryNode(NodeType::ADD, token, std::move(left), std::move(newRight));
}

std::unique_ptr<Node> createSubNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
    type::addType(left.get());
    type::addType(right.get());

    // number - number
    if (type::isInteger(left->type) && type::isInteger(right->type)) {
        return createBinaryNode(NodeType::SUB, token, std::move(left), std::move(right));
    }

    // pointer - number
    if (left->type->base && type::isInteger(right->type)) {
        auto resultType = left->type;
        auto newRight = createBinaryNode(NodeType::MUL, token, std::move(right), createNumberNode(token, left->type->base->size));
        type::addType(newRight.get());
        auto node = createBinaryNode(NodeType::SUB, token, std::move(left), std::move(newRight));
        node->type = resultType;
        return node;
    }

    // pointer - pointer
    if (left->type->base && right->type->base) {
        int baseSize = left->type->base->size;
        auto node = createBinaryNode(NodeType::SUB, token, std::move(left), std::move(right));
        node->type = std::make_shared<Type>(TypeKind::INT);
        return createBinaryNode(NodeType::DIV, token, std::move(node), createNumberNode(token, baseSize));
    }

    Log::error("Invalid subtraction involving pointers"sv, token);
    return nullptr;
}

std::unique_ptr<Node> createStructRefNode(const Token* token, std::unique_ptr<Node> left) {
    type::addType(left.get());

    if (!type::is(left->type, STRUCT) && !type::is(left->type, UNION)) {
        Log::error("Left operand is not a struct or union type"sv, token);
        return nullptr;
    }

    auto node = createUnaryNode(NodeType::MEMBER, token, std::move(left));
    node->member = findStructMember(node->left->type, token);

    return node;
}

} // namespace yoctocc
