#include "Generator.hpp"

#include <cassert>
#include "Assembly/Assembly.hpp"
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include "Utility.hpp"

namespace {
    constexpr size_t STACK_ALIGNMENT = 16;
}

namespace yoctocc {

using enum GasDirective;
using enum Register;
using namespace std::string_view_literals;

std::vector<std::string> Generator::run(Object* obj) {
    assert(obj);
    assignLocalVariableOffsets(obj);
    emitData(obj);
    emitText(obj);
    return lines;
}

void Generator::load(const Type* type) {
    assert(type);
    if (type->kind == TypeKind::ARRAY) {
        // 何もしない
        return;
    }
    if (type->size == 1) {
        addCode(movsbq(RAX, Address{RAX}));
    } else {
        addCode(mov(RAX, Address{RAX}));
    }
}

void Generator::store(const Type* type) {
    addCode(pop(RDI));
    if (type->size == 1) {
        addCode(mov(Address{RDI}, AL));
    } else {
        addCode(mov(Address{RDI}, RAX));
    }
}

void Generator::assignLocalVariableOffsets(Object* obj) {
    assert(obj);

    for (Object* fn = obj; fn; fn = fn->next.get()) {
        if (!fn->isFunction) {
            continue;
        }
        int offset = 0;
        for (Object* local = fn->locals.get(); local; local = local->next.get()) {
            offset += local->type->size;
            local->offset = -offset;
        }
        fn->stackSize = alignTo(offset, STACK_ALIGNMENT);
    }
}

void Generator::generateAddress(const Node* node) {
    assert(node);

    switch (node->nodeType) {
        case NodeType::VARIABLE:
            if (node->variable->isLocal) {
                addCode(lea(RAX, Address{RBP, node->variable->offset}));
            } else {
                addCode(lea(RAX, RipRelativeAddress{node->variable->name}));
            }
            return;
        case NodeType::DEREFERENCE:
            generateExpression(node->left.get());
            return;
        case NodeType::MEMBER:
            generateAddress(node->left.get());
            addCode(add(RAX, node->member->offset));
            return;
        case NodeType::COMMA:
            generateExpression(node->left.get());
            generateAddress(node->right.get());
            return;
        default:
            break;
    }

    Log::error("Not an lvalue"sv, node->token);
}

void Generator::generateStatement(const Node* node) {
    assert(node);
    addCode(directive::loc(1, node->token->line));

    if (node->nodeType == NodeType::IF) {
        uint64_t count = labelCount++;
        auto elseLabel = makeElseLabel(count);
        auto endLabel = makeEndLabel(count);

        generateExpression(node->condition.get());
        // if
        addCode(cmp(RAX, 0));
        addCode(je(elseLabel.ref()));
        // then
        generateStatement(node->then.get());
        addCode(jmp(endLabel.ref()));
        // else
        addCode(elseLabel.def());
        if (node->els) {
            generateStatement(node->els.get());
        }
        addCode(endLabel.def());
        return;
    }
    if (node->nodeType == NodeType::FOR) {
        uint64_t count = labelCount++;
        auto beginLabel = makeBeginLabel(count);
        auto endLabel = makeEndLabel(count);

        if (node->init) {
            generateStatement(node->init.get());
        }
        addCode(beginLabel.def());
        if (node->condition) {
            generateExpression(node->condition.get());
            addCode(cmp(RAX, 0));
            addCode(je(endLabel.ref()));
        }
        generateStatement(node->then.get());
        if (node->inc) {
            generateExpression(node->inc.get());
        }
        addCode(jmp(beginLabel.ref()));
        addCode(endLabel.def());
        return;
    }
    if (node->nodeType == NodeType::BLOCK) {
        for (const Node* statement = node->body.get(); statement; statement = statement->next.get()) {
            generateStatement(statement);
        }
        return;
    }
    if (node->nodeType == NodeType::RETURN) {
        generateExpression(node->left.get());
        addCode(jmp(makeLabel("return", currentFunction->name).ref()));
        return;
    }
    if (node->nodeType == NodeType::EXPRESSION_STATEMENT) {
        generateExpression(node->left.get());
        return;
    }

    using namespace std::literals;
    Log::error("Invalid statement"sv, node->token);
}

void Generator::generateExpression(const Node* node) {
    assert(node);
    addCode(directive::loc(1, node->token->line));

    switch (node->nodeType) {
        case NodeType::NUMBER:
            addCode(mov(RAX, node->value));
            return;
        case NodeType::NEGATE:
            generateExpression(node->left.get());
            addCode(neg(RAX));
            return;
        case NodeType::VARIABLE:
        case NodeType::MEMBER:
            generateAddress(node);
            load(node->type.get());
            return;
        case NodeType::ADDRESS:
            generateAddress(node->left.get());
            return;
        case NodeType::DEREFERENCE:
            generateExpression(node->left.get());
            load(node->type.get());
            return;
        case NodeType::ASSIGN:
            generateAddress(node->left.get());
            addCode(push(RAX));
            generateExpression(node->right.get());
            store(node->type.get());
            return;
        case NodeType::STATEMENT_EXPRESSION:
            for (const Node* stmt = node->body.get(); stmt; stmt = stmt->next.get()) {
                generateStatement(stmt);
            }
            return;
        case NodeType::COMMA:
            generateExpression(node->left.get());
            generateExpression(node->right.get());
            return;
        case NodeType::FUNCTION_CALL:
            {
                int argCount = 0;
                for (const Node* arg = node->arguments.get(); arg; arg = arg->next.get()) {
                    generateExpression(arg);
                    addCode(push(RAX));
                    argCount++;
                }
                assert(argCount <= static_cast<int>(ARG_REGISTERS64.size()));
                for (int i = argCount - 1; i >= 0; i--) {
                    addCode(pop(ARG_REGISTERS64[i]));
                }
                addCode(mov(RAX, 0));
                addCode(call(node->functionName));
            }
            return;
        default:
            break;
    }

    generateExpression(node->right.get());
    addCode(push(RAX));

    generateExpression(node->left.get());
    addCode(pop(RDI));

    switch (node->nodeType) {
        case NodeType::ADD:
            addCode(add(RAX, RDI));
            return;
        case NodeType::SUB:
            addCode(sub(RAX, RDI));
            return;
        case NodeType::MUL:
            addCode(imul(RAX, RDI));
            return;
        case NodeType::DIV:
            addCode(
                cqo(),
                idiv(RDI)
            );
            return;
        case NodeType::EQUAL:
            addCode(
                cmp(RAX, RDI),
                sete(AL),
                movzx(RAX, AL)
            );
            return;
        case NodeType::NOT_EQUAL:
            addCode(
                 cmp(RAX, RDI),
                 setne(AL),
                 movzx(RAX, AL)
            );
            return;
        case NodeType::LESS:
            addCode(
                cmp(RAX, RDI),
                setl(AL),
                movzx(RAX, AL)
            );
            return;
        case NodeType::LESS_EQUAL:
            addCode(
                cmp(RAX, RDI),
                setle(AL),
                movzx(RAX, AL)
            );
            return;
        case NodeType::GREATER:
            addCode(
                cmp(RAX, RDI),
                setg(AL),
                movzx(RAX, AL)
            );
            return;
        case NodeType::GREATER_EQUAL:
            addCode(
                cmp(RAX, RDI),
                setge(AL),
                movzx(RAX, AL)
            );
            return;
        default:
            break;
    }

    using namespace std::literals;
    Log::error("Invalid expression"sv, node->token);
}

void Generator::generateFunction(const Object* obj) {
    assert(obj);
    currentFunction = obj;

    addCode(
        directive::global(obj->name),
        makeLabel(obj->name).def(),
        // Prologue
        push(RBP),
        mov(RBP, RSP)
    );
    if (obj->stackSize > 0) {
        addCode(sub(RSP, obj->stackSize));
    }

    int i = 0;
    for (const Object* param = obj->parameters; param; param = param->next.get()) {
        if (param->type->size == 1) {
            addCode(mov(Address{RBP, param->offset}, ARG_REGISTERS8[i++]));
        } else {
            addCode(mov(Address{RBP, param->offset}, ARG_REGISTERS64[i++]));
        }
    }

    generateStatement(obj->body.get());
    // Epilogue
    addCode(
        makeLabel("return", obj->name).def(),
        mov(RSP, RBP),
        pop(RBP),
        ret()
    );
}

void Generator::emitData(const Object* obj) {
    assert(obj);

    for (const Object* var = obj; var; var = var->next.get()) {
        if (var->isFunction) {
            continue;
        }
        addCode(
            to_string(DATA),
            directive::global(var->name),
            makeLabel(var->name).def()
        );
        if (var->initialData.empty()) {
            addCode(directive::zero(var->type->size));
        } else {
            assert((var->initialData.size() + 1uz) == static_cast<size_t>(var->type->size));
            for (int i = 0; i < var->type->size; i++) {
                addCode(directive::byte(var->initialData[i]));
            }
        }
    }
}

void Generator::emitText(const Object* obj) {
    assert(obj);

    addCode(to_string(TEXT));

    for (const Object* fn = obj; fn; fn = fn->next.get()) {
        if (!fn->isFunction) {
            continue;
        }
        generateFunction(fn);
    }
}

} // namespace yoctocc
