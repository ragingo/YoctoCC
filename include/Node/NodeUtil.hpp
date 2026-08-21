#pragma once
#include "Node/NodeTypes.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace yoctocc {

struct Token;
struct Initializer;
struct InitDesignator;

std::unique_ptr<Node> createNumberNode(const Token* token, int64_t value);
std::unique_ptr<Node> createNumberNode(const Token* token, double value);
std::unique_ptr<Node> createLongNode(const Token* token, int64_t value);
std::unique_ptr<Node> createULongNode(const Token* token, int64_t value);
std::unique_ptr<Node> createUnaryNode(NodeType type, const Token* token, std::unique_ptr<Node> operand);
std::unique_ptr<Node> createBinaryNode(NodeType type,
                                       const Token* token,
                                       std::unique_ptr<Node> left,
                                       std::unique_ptr<Node> right);
std::unique_ptr<Node> createVariableNode(const Token* token, Object* variable);
std::unique_ptr<Node> createBlockNode(const Token* token, std::unique_ptr<Node> body = nullptr);
std::unique_ptr<Node> createAddNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right);
std::unique_ptr<Node> createSubNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right);
std::unique_ptr<Node> createStructRefNode(const Token* token, std::unique_ptr<Node> left);
std::unique_ptr<Node> createCastNode(std::unique_ptr<Node> expression, const std::shared_ptr<Type>& targetType);
std::unique_ptr<Node> createInitDesignetorExpressionNode(const Token* token, const InitDesignator* initDesignator);
std::unique_ptr<Node> createVariableInitializerNode(
    const Token* token,
    Initializer* initializer,
    const InitDesignator* initDesignator,
    const std::shared_ptr<Type>& type
);
int64_t eval(Node* node);
int64_t eval2(Node* node, std::string& label);
int64_t eval_rvalue(Node* node, std::string& label)
#if defined(__clang__)
#elif defined(__GNUC__)
    post(ret: ret >= 0)
#else
#endif
;
Relocation* writeGlobalVariableData(
    Relocation* relocations,
    const Initializer* initializer,
    const std::shared_ptr<Type>& type,
    std::vector<char>& buf,
    size_t offset
);

inline bool isComparison(const Node* node) {
    using enum NodeType;
    auto type = node->nodeType;
    return type == EQUAL || type == NOT_EQUAL || type == LESS || type == LESS_EQUAL;
}

} // namespace yoctocc
