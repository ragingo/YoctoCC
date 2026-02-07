#pragma once
#include <memory>
#include "Node/NodeTypes.hpp"

namespace yoctocc {

struct Token;

std::unique_ptr<Node> createNumberNode(const Token* token, int value);
std::unique_ptr<Node> createUnaryNode(NodeType type, const Token* token, std::unique_ptr<Node> operand);
std::unique_ptr<Node> createBinaryNode(NodeType type, const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right);
std::unique_ptr<Node> createVariableNode(const Token* token, Object* variable);
std::unique_ptr<Node> createBlockNode(const Token* token, std::unique_ptr<Node> body = nullptr);
std::unique_ptr<Node> createAddNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right);
std::unique_ptr<Node> createSubNode(const Token* token, std::unique_ptr<Node> left, std::unique_ptr<Node> right);

} // namespace yoctocc
