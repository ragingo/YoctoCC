#include "Generator.hpp"

#include <cassert>
#include "Assembly/Assembly.hpp"
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Type.hpp"

namespace {
    constexpr size_t STACK_ALIGNMENT = 16;
}

namespace yoctocc {

std::vector<std::string> Generator::run(const std::shared_ptr<Function>& func) {
    assert(func);

    assignLocalVariableOffsets(func);

    for (auto fn = func; fn; fn = fn->next) {
        generateFunction(fn);
    }

    return lines;
}

void Generator::load(const std::shared_ptr<Type>& type) {
    assert(type);
    if (type->kind == TypeKind::ARRAY) {
        // 何もしない
        return;
    }
    lines.emplace_back(mov(Register::RAX, Address<Register>{Register::RAX}));
}

void Generator::store() {
    lines.emplace_back(pop(Register::RDI));
    lines.emplace_back(mov(Address<Register>{Register::RDI}, Register::RAX));
}

void Generator::assignLocalVariableOffsets(const std::shared_ptr<Function>& func) {
    assert(func);

    for (auto fn = func; fn; fn = fn->next) {
        int offset = 0;
        for (auto obj = fn->locals; obj; obj = obj->next) {
            offset += obj->type->size;
            obj->offset = -offset;
        }
        fn->stackSize = alignTo(offset, STACK_ALIGNMENT);
    }
}

void Generator::generateAddress(const std::shared_ptr<Node>& node) {
    assert(node);
    if (node->nodeType == NodeType::VARIABLE) {
        int offset = node->variable->offset;
        lines.emplace_back(lea(Register::RAX, Address<Register>{Register::RBP, offset}));
        return;
    }
    if (node->nodeType == NodeType::DEREFERENCE) {
        generateExpression(node->left);
        return;
    }
    using namespace std::literals;
    Log::error(node->token->location, "Not an lvalue"sv);
}

void Generator::generateStatement(const std::shared_ptr<Node>& node) {
    assert(node);
    if (node->nodeType == NodeType::IF) {
        uint64_t count = labelCount++;
        auto elseLabel = makeElseLabel(count);
        auto endLabel = makeEndLabel(count);

        generateExpression(node->condition);
        // if
        lines.emplace_back(cmp(Register::RAX, 0));
        lines.emplace_back(je(elseLabel.ref()));
        // then
        generateStatement(node->then);
        lines.emplace_back(jmp(endLabel.ref()));
        // else
        lines.emplace_back(elseLabel.def());
        if (node->els) {
            generateStatement(node->els);
        }
        lines.emplace_back(endLabel.def());
        return;
    }
    if (node->nodeType == NodeType::FOR) {
        uint64_t count = labelCount++;
        auto beginLabel = makeBeginLabel(count);
        auto endLabel = makeEndLabel(count);

        if (node->init) {
            generateStatement(node->init);
        }
        lines.emplace_back(beginLabel.def());
        if (node->condition) {
            generateExpression(node->condition);
            lines.emplace_back(cmp(Register::RAX, 0));
            lines.emplace_back(je(endLabel.ref()));
        }
        generateStatement(node->then);
        if (node->inc) {
            generateExpression(node->inc);
        }
        lines.emplace_back(jmp(beginLabel.ref()));
        lines.emplace_back(endLabel.def());
        return;
    }
    if (node->nodeType == NodeType::BLOCK) {
        auto statement = node->body;
        while (statement) {
            generateStatement(statement);
            statement = statement->next;
        }
        return;
    }
    if (node->nodeType == NodeType::RETURN) {
        generateExpression(node->left);
        lines.emplace_back(jmp(std::format(".L.return.{}", currentFunction->name)));
        return;
    }
    if (node->nodeType == NodeType::EXPRESSION_STATEMENT) {
        generateExpression(node->left);
        return;
    }

    using namespace std::literals;
    Log::error(node->token->location, "Invalid statement"sv);
}

void Generator::generateExpression(const std::shared_ptr<Node>& node) {
    using enum Register;

    assert(node);

    switch (node->nodeType) {
        case NodeType::NUMBER:
            lines.emplace_back(mov(RAX, node->value));
            return;
        case NodeType::NEGATE:
            generateExpression(node->left);
            lines.emplace_back(neg(RAX));
            return;
        case NodeType::VARIABLE:
            generateAddress(node);
            load(node->type);
            return;
        case NodeType::ADDRESS:
            generateAddress(node->left);
            return;
        case NodeType::DEREFERENCE:
            generateExpression(node->left);
            load(node->type);
            return;
        case NodeType::ASSIGN:
            generateAddress(node->left);
            lines.emplace_back(push(RAX));
            generateExpression(node->right);
            store();
            return;
        case NodeType::FUNCTION_CALL:
            {
                int argCount = 0;
                for (auto arg = node->arguments; arg; arg = arg->next) {
                    generateExpression(arg);
                    lines.emplace_back(push(RAX));
                    argCount++;
                }
                assert(argCount <= static_cast<int>(ARG_REGISTERS.size()));
                for (int i = argCount - 1; i >= 0; i--) {
                    lines.emplace_back(pop(ARG_REGISTERS[i]));
                }
                lines.emplace_back(mov(RAX, 0));
                lines.emplace_back(call(node->functionName));
            }
            return;
        default:
            break;
    }

    generateExpression(node->right);
    lines.emplace_back(push(RAX));

    generateExpression(node->left);
    lines.emplace_back(pop(RDI));

    switch (node->nodeType) {
        case NodeType::ADD:
            lines.emplace_back(add(RAX, RDI));
            return;
        case NodeType::SUB:
            lines.emplace_back(sub(RAX, RDI));
            return;
        case NodeType::MUL:
            lines.emplace_back(imul(RAX, RDI));
            return;
        case NodeType::DIV:
            lines.emplace_back(cqo());
            lines.emplace_back(idiv(RDI));
            return;
        case NodeType::EQUAL:
            lines.emplace_back(cmp(RAX, RDI));
            lines.emplace_back(sete(AL));
            lines.emplace_back(movzx(RAX, AL));
            return;
        case NodeType::NOT_EQUAL:
            lines.emplace_back(cmp(RAX, RDI));
            lines.emplace_back(setne(AL));
            lines.emplace_back(movzx(RAX, AL));
            return;
        case NodeType::LESS:
            lines.emplace_back(cmp(RAX, RDI));
            lines.emplace_back(setl(AL));
            lines.emplace_back(movzx(RAX, AL));
            return;
        case NodeType::LESS_EQUAL:
            lines.emplace_back(cmp(RAX, RDI));
            lines.emplace_back(setle(AL));
            lines.emplace_back(movzx(RAX, AL));
            return;
        case NodeType::GREATER:
            lines.emplace_back(cmp(RAX, RDI));
            lines.emplace_back(setg(AL));
            lines.emplace_back(movzx(RAX, AL));
            return;
        case NodeType::GREATER_EQUAL:
            lines.emplace_back(cmp(RAX, RDI));
            lines.emplace_back(setge(AL));
            lines.emplace_back(movzx(RAX, AL));
            return;
        default:
            break;
    }

    using namespace std::literals;
    Log::error(node->token->location, "Invalid expression"sv);
}

void Generator::generateFunction(const std::shared_ptr<Function>& func) {
    using enum Register;
    assert(func);
    currentFunction = func;

    lines.emplace_back(std::format(".global {}", func->name));
    lines.emplace_back(std::format("{}:", func->name));
    // Prologue
    lines.emplace_back(push(RBP));
    lines.emplace_back(mov(RBP, RSP));
    if (func->stackSize > 0) {
        lines.emplace_back(sub(RSP, func->stackSize));
    }

    int i = 0;
    for (auto param = func->parameters; param; param = param->next) {
        lines.emplace_back(mov(Address<Register>{RBP, param->offset}, ARG_REGISTERS[i++]));
    }

    generateStatement(func->body);

    // Epilogue
    lines.emplace_back(std::format(".L.return.{}:", func->name));
    lines.emplace_back(mov(RSP, RBP));
    lines.emplace_back(pop(RBP));
    lines.emplace_back(ret());
}

} // namespace yoctocc
