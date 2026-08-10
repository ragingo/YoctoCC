#include "Node/NodeUtil.hpp"

#include <utility>
#include "Logger.hpp"
#include "Node/NodeTypes.hpp"
#include "Parser/Common.hpp"
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

    Log::error("No such member"sv, memberName);
    return nullptr;
}
} // namespace

namespace yoctocc {

using enum TypeKind;

std::unique_ptr<Node> createNumberNode(const Token* token, int64_t value) {
    auto node = std::make_unique<Node>(NodeType::NUMBER, token);
    node->value = value;
    return node;
}

std::unique_ptr<Node> createLongNode(const Token* token, int64_t value) {
    auto node = std::make_unique<Node>(NodeType::NUMBER, token);
    node->value = value;
    node->type = type::longType();
    return node;
}

std::unique_ptr<Node> createUnaryNode(NodeType type, const Token* token, std::unique_ptr<Node> operand) {
    auto node = std::make_unique<Node>(type, token);
    node->left = std::move(operand);
    return node;
}

std::unique_ptr<Node> createBinaryNode(NodeType type,
                                       const Token* token,
                                       std::unique_ptr<Node> left,
                                       std::unique_ptr<Node> right) {
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
    if (type::isInteger(left->type.get()) && type::isInteger(right->type.get())) {
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
    auto newRight =
        createBinaryNode(NodeType::MUL, token, std::move(right), createLongNode(token, left->type->base->size));
    return createBinaryNode(NodeType::ADD, token, std::move(left), std::move(newRight));
}

std::unique_ptr<Node> createSubNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
    type::addType(left.get());
    type::addType(right.get());

    // number - number
    if (type::isInteger(left->type.get()) && type::isInteger(right->type.get())) {
        return createBinaryNode(NodeType::SUB, token, std::move(left), std::move(right));
    }

    // pointer - number
    if (left->type->base && type::isInteger(right->type.get())) {
        auto resultType = left->type;
        auto newRight =
            createBinaryNode(NodeType::MUL, token, std::move(right), createLongNode(token, left->type->base->size));
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

std::unique_ptr<Node> createCastNode(std::unique_ptr<Node> expression, const std::shared_ptr<Type>& targetType) {
    type::addType(expression.get());

    auto token = expression->token;
    auto node = createUnaryNode(NodeType::CAST, token, std::move(expression));
    node->type = targetType;
    return node;
}

std::unique_ptr<Node> createInitDesignetorExpressionNode(const Token* token, const InitDesignator* initDesignator) {
    if (initDesignator->variable) {
        return createVariableNode(token, initDesignator->variable);
    }

    if (initDesignator->member) {
        auto initDesgExprNode = createInitDesignetorExpressionNode(token, initDesignator->next);
        auto memberNode = createUnaryNode(NodeType::MEMBER, token, std::move(initDesgExprNode));
        memberNode->member = std::make_unique<Member>();
        memberNode->member->name = initDesignator->member->name;
        memberNode->member->type = initDesignator->member->type;
        memberNode->member->offset = initDesignator->member->offset;
        memberNode->member->index = initDesignator->member->index;
        return memberNode;
    }

    auto left = createInitDesignetorExpressionNode(token, initDesignator->next);
    auto right = createNumberNode(token, initDesignator->index);
    auto addNode = createAddNode(token, std::move(left), std::move(right));
    auto unaryNode = createUnaryNode(NodeType::DEREFERENCE, token, std::move(addNode));
    return unaryNode;
}

std::unique_ptr<Node> createVariableInitializerNode(
    const Token* token,
    Initializer* initializer,
    const InitDesignator* initDesignator,
    const std::shared_ptr<Type>& type
) {
    if (type->kind == TypeKind::ARRAY) {
        auto node = std::make_unique<Node>(NodeType::NULL_EXPRESSION, token);
        for (int i = 0; i < type->arraySize; i++) {
            InitDesignator initDesignator2{initDesignator, i, nullptr, nullptr};
            auto right = createVariableInitializerNode(token, initializer->children[i].get(), &initDesignator2, type->base);
            node = createBinaryNode(NodeType::COMMA, token, std::move(node), std::move(right));
        }
        return node;
    }

    if (type->kind == TypeKind::STRUCT && !initializer->expression) {
        auto node = std::make_unique<Node>(NodeType::NULL_EXPRESSION, token);
        for (auto member = type->members.get(); member; member = member->next.get()) {
            InitDesignator initDesignator2{initDesignator, 0, member, nullptr};
            auto right = createVariableInitializerNode(token, initializer->children[member->index].get(), &initDesignator2, member->type);
            node = createBinaryNode(NodeType::COMMA, token, std::move(node), std::move(right));
        }
        return node;
    }

    if (type->kind == TypeKind::UNION) {
        InitDesignator initDesignator2{initDesignator, 0, type->members.get(), nullptr};
        return createVariableInitializerNode(token, initializer->children[0].get(), &initDesignator2, type->members->type);
    }

    if (!initializer->expression) {
        return std::make_unique<Node>(NodeType::NULL_EXPRESSION, token);
    }

    auto left = createInitDesignetorExpressionNode(token, initDesignator);
    auto right = std::move(initializer->expression);
    return createBinaryNode(NodeType::ASSIGN, token, std::move(left), std::move(right));
}

int64_t eval(Node* node) {
    std::string dummy;
    return eval2(node, dummy);
}

int64_t eval2(Node* node, std::string& label) {
    using enum NodeType;
    assert(node);
    type::addType(node);

    switch (node->nodeType) {
    case ADD:
        return eval2(node->left.get(), label) + eval(node->right.get());
    case SUB:
        return eval2(node->left.get(), label) - eval(node->right.get());
    case MUL:
        return eval(node->left.get()) * eval(node->right.get());
    case DIV:
        return eval(node->left.get()) / eval(node->right.get());
    case NEGATE:
        return -eval(node->left.get());
    case MOD:
        return eval(node->left.get()) % eval(node->right.get());
    case BIT_AND:
        return eval(node->left.get()) & eval(node->right.get());
    case BIT_OR:
        return eval(node->left.get()) | eval(node->right.get());
    case BIT_XOR:
        return eval(node->left.get()) ^ eval(node->right.get());
    case SHL:
        return eval(node->left.get()) << eval(node->right.get());
    case SHR:
        return eval(node->left.get()) >> eval(node->right.get());
    case EQUAL:
        return eval(node->left.get()) == eval(node->right.get());
    case NOT_EQUAL:
        return eval(node->left.get()) != eval(node->right.get());
    case LESS:
        return eval(node->left.get()) < eval(node->right.get());
    case LESS_EQUAL:
        return eval(node->left.get()) <= eval(node->right.get());
    case GREATER:
        return eval(node->left.get()) > eval(node->right.get());
    case GREATER_EQUAL:
        return eval(node->left.get()) >= eval(node->right.get());
    case CONDITIONAL:
        return eval(node->condition.get()) ? eval2(node->then.get(), label) : eval2(node->els.get(), label);
    case COMMA:
        return eval2(node->right.get(), label);
    case NOT:
        return !eval(node->left.get());
    case BIT_NOT:
        return ~eval(node->left.get());
    case LOGICAL_AND:
        return eval(node->left.get()) && eval(node->right.get());
    case LOGICAL_OR:
        return eval(node->left.get()) || eval(node->right.get());
    case CAST: {
        int64_t value = eval2(node->left.get(), label);
        if (type::isInteger(node->type.get())) {
            if (node->type->size == 1) {
                return static_cast<int64_t>(static_cast<uint8_t>(value));
            } else if (node->type->size == 2) {
                return static_cast<int64_t>(static_cast<uint16_t>(value));
            } else if (node->type->size == 4) {
                return static_cast<int64_t>(static_cast<uint32_t>(value));
            } else if (node->type->size == 8) {
                return static_cast<int64_t>(static_cast<uint64_t>(value));
            }
        }
        return eval2(node->left.get(), label);
    }
    case ADDRESS:
        return eval_rvalue(node->left.get(), label);
    case MEMBER:
        if (node->type->kind != TypeKind::ARRAY) {
            Log::error("eval2: member node is not an array type"sv, node->token);
            return 0;
        }
        return eval_rvalue(node->left.get(), label) + node->member->offset;
    case VARIABLE:
        if (node->variable->type->kind != TypeKind::ARRAY && node->variable->type->kind != TypeKind::FUNCTION) {
            Log::error("eval2: variable node is not an array or function type"sv, node->token);
            return 0;
        }
        label = node->variable->name;
        return 0;
    case NUMBER:
        return node->value;
    default:
        // TODO: 列挙体の文字列表現
        Log::error(std::format("token::eval: unsupported node type: {}", std::to_underlying(node->nodeType)));
        return 0;
    }
}

int64_t eval_rvalue(Node* node, std::string& label) {
    switch (node->nodeType) {
    case NodeType::VARIABLE:
        if (node->variable->isLocal) {
            Log::error("eval_rvalue: local variable is not supported");
            return 0;
        }
        label = node->variable->name;
        return 0;
    case NodeType::DEREFERENCE:
        return eval2(node->left.get(), label);
    case NodeType::MEMBER:
        return eval_rvalue(node->left.get(), label) + node->member->offset;
    default:
        Log::error(std::format("eval_rvalue: unsupported node type: {}", std::to_underlying(node->nodeType)));
        return 0;
    }
}

Relocation* writeGlobalVariableData(Relocation* relocations, const Initializer* initializer, const std::shared_ptr<Type>& type, std::vector<char>& buf, size_t offset) {
    if (type->kind == TypeKind::ARRAY) {
        for (int i = 0; i < type->arraySize; i++) {
            relocations = writeGlobalVariableData(relocations, initializer->children[i].get(), type->base, buf, offset + i * type->base->size);
        }
        return relocations;
    }

    if (type->kind == TypeKind::STRUCT) {
        for (auto member = type->members.get(); member; member = member->next.get()) {
            relocations = writeGlobalVariableData(relocations, initializer->children[member->index].get(), member->type, buf, offset + member->offset);
        }
        return relocations;
    }

    if (type->kind == TypeKind::UNION) {
        relocations = writeGlobalVariableData(relocations, initializer->children[0].get(), type->members->type, buf, offset);
        return relocations;
    }

    if (!initializer->expression) {
        return relocations;
    }

    std::string label;
    int64_t value = eval2(initializer->expression.get(), label);

    if (label.empty()) {
        switch (type->size) {
            case 1:
                buf[offset] = static_cast<char>(value);
                break;
            case 2:
                *reinterpret_cast<int16_t*>(&buf[offset]) = static_cast<int16_t>(value);
                break;
            case 4:
                *reinterpret_cast<int32_t*>(&buf[offset]) = static_cast<int32_t>(value);
                break;
            case 8:
                *reinterpret_cast<int64_t*>(&buf[offset]) = static_cast<int64_t>(value);
                break;
            default:
                std::unreachable();
        }
        return relocations;
    }

    auto relocation = std::make_unique<Relocation>();
    relocation->offset = offset;
    relocation->label = label;
    relocation->addend = value;

    relocations->next = std::move(relocation);
    return relocations->next.get();
}

} // namespace yoctocc
