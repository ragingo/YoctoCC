#include "Generator.hpp"

#include "Assembly/Assembly.hpp"
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include "Utility.hpp"
#include <cassert>

namespace {
using namespace yoctocc;

constexpr size_t STACK_ALIGNMENT = 16;

int depth = 0;

std::string push_rax() {
    depth++;
    return push(RAX);
}

std::string pop_reg(Register reg) {
    depth--;
    return pop(reg);
}

enum TypeID {
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64
};

TypeID getTypeID(const Type* type) {
    using enum TypeKind;
    switch (type->kind) {
        case CHAR:
            return type->isUnsigned ? U8 : I8;
        case SHORT:
            return type->isUnsigned ? U16 : I16;
        case INT:
            return type->isUnsigned ? U32 : I32;
        case LONG:
            return type->isUnsigned ? U64 : I64;
        default:
            return U64;
    }
}

const std::string i32i8 = movsbl(EAX, AL);
const std::string i32u8 = movzbl(EAX, AL);
const std::string i32i16 = movswl(EAX, AX);
const std::string i32u16 = movzwl(EAX, AX);
const std::string i32i64 = movsxd(RAX, EAX);
const std::string u32i64 = mov(EAX, EAX);

// cast_table[from][to]
// clang-format off
const std::string castTable[8][8] = {
    // i8   i16     i32 i64     u8     u16     u32 u64
    {"",    "",     "", i32i64, i32u8, i32u16, "", i32i64}, // i8
    {i32i8, "",     "", i32i64, i32u8, i32u16, "", i32i64}, // i16
    {i32i8, i32i16, "", i32i64, i32u8, i32u16, "", i32i64}, // i32
    {i32i8, i32i16, "", "",     i32u8, i32u16, "", ""},     // i64
    {i32i8, "",     "", i32i64, "",    "",     "", i32i64}, // u8
    {i32i8, i32i16, "", i32i64, i32u8, "",     "", i32i64}, // u16
    {i32i8, i32i16, "", u32i64, i32u8, i32u16, "", u32i64}, // u32
    {i32i8, i32i16, "", "",     i32u8, i32u16, "", ""},     // u64
};
// clang-format on

std::string compareZero(const Type* type) {
    if (type::isInteger(type) && type->size <= 4) {
        return cmp(EAX, 0);
    } else {
        return cmp(RAX, 0);
    }
}
} // namespace

namespace yoctocc {

using enum GasDirective;
using enum Register;
using namespace directive;
using namespace std::string_view_literals;

std::vector<std::string> Generator::run(Object* obj) {
    assert(obj);
    assignLocalVariableOffsets(obj);
    emitData(obj);
    emitText(obj);
    return lines;
}

void Generator::cast(const Node* node) {
    using enum TypeKind;
    auto from = node->left->type.get();
    auto to = node->type.get();

    if (type::is(to, VOID)) {
        return;
    }

    if (type::is(from, BOOL)) {
        addCode(compareZero(from), setne(AL), movzx(EAX, AL));
        return;
    }

    auto code = castTable[getTypeID(from)][getTypeID(to)];
    if (!code.empty()) {
        addCode(code);
    }
}

void Generator::load(const Type* type) {
    using enum TypeKind;
    assert(type);
    if (type::is(type, ARRAY) || type::is(type, STRUCT) || type::is(type, UNION)) {
        // 何もしない
        return;
    }
    if (type->size == 1) {
        if (type->isUnsigned) {
            addCode(movzbl(EAX, byte_ptr(Address{RAX})));
        } else {
            addCode(movsbl(EAX, byte_ptr(Address{RAX})));
        }
    } else if (type->size == 2) {
        if (type->isUnsigned) {
            addCode(movzwl(EAX, word_ptr(Address{RAX})));
        } else {
            addCode(movswl(EAX, word_ptr(Address{RAX})));
        }
    } else if (type->size == 4) {
        addCode(movsxd(RAX, Address{RAX}));
    } else {
        addCode(mov(RAX, Address{RAX}));
    }
}

void Generator::store(const Type* type) {
    using enum TypeKind;
    assert(type);
    addCode(pop_reg(RDI));

    if (type::is(type, STRUCT) || type::is(type, UNION)) {
        int i = 0;
        // 8 バイトずつコピー
        for (; i + 8 <= type->size; i += 8) {
            addCode(mov(R8, Address{RAX, i}));
            addCode(mov(Address{RDI, i}, R8));
        }
        // 残りのバイトをコピー
        for (; i < type->size; i++) {
            addCode(mov(R8B, Address{RAX, i}));
            addCode(mov(Address{RDI, i}, R8B));
        }
        return;
    }

    if (type->size == 1) {
        addCode(mov(Address{RDI}, AL));
    } else if (type->size == 2) {
        addCode(mov(Address{RDI}, AX));
    } else if (type->size == 4) {
        addCode(mov(Address{RDI}, EAX));
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
            offset = alignTo(offset, local->alignment);
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

void Generator::emitLocation(const Node* node) {
    if (node->token->line != lastEmittedLine) {
        addCode(loc(1, static_cast<int>(node->token->line)));
        lastEmittedLine = node->token->line;
    }
}

void Generator::generateStatement(const Node* node) {
    assert(node);

    emitLocation(node);

    if (node->nodeType == NodeType::IF) {
        uint64_t count = labelCount++;
        auto elseLabel = labels::else_(count);
        auto endLabel = labels::end(count);

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
        auto beginLabel = labels::begin(count);
        auto breakLabel = labels::label(node->breakLabel);
        auto continueLabel = labels::label(node->continueLabel);

        if (node->init) {
            generateStatement(node->init.get());
        }
        addCode(beginLabel.def());
        if (node->condition) {
            generateExpression(node->condition.get());
            addCode(cmp(RAX, 0));
            addCode(je(breakLabel.ref()));
        }
        generateStatement(node->then.get());
        addCode(continueLabel.def());
        if (node->inc) {
            generateExpression(node->inc.get());
        }
        addCode(jmp(beginLabel.ref()));
        addCode(breakLabel.def());
        return;
    }

    if (node->nodeType == NodeType::DO) {
        int count = labelCount++;
        auto beginLabel = labels::begin(count);
        auto breakLabel = labels::label(node->breakLabel);
        auto continueLabel = labels::label(node->continueLabel);

        addCode(beginLabel.def());
        if (node->then) {
            generateStatement(node->then.get());
        }
        addCode(continueLabel.def());
        if (node->condition) {
            generateExpression(node->condition.get());
        }
        addCode(cmp(RAX, 0));
        addCode(jne(beginLabel.ref()));
        addCode(breakLabel.def());
        return;
    }

    if (node->nodeType == NodeType::SWITCH) {
        generateExpression(node->condition.get());

        for (const Node* caseNode = node->cases; caseNode; caseNode = caseNode->cases) {
            if (node->condition->type->size == 8) {
                addCode(cmp(RAX, caseNode->integerValue));
            } else {
                addCode(cmp(EAX, static_cast<int32_t>(caseNode->integerValue)));
            }
            addCode(je(labels::label(caseNode->label).ref()));
        }

        if (node->defaultCase) {
            addCode(jmp(labels::label(node->defaultCase->label).ref()));
        }

        auto breakLabel = labels::label(node->breakLabel);
        addCode(jmp(breakLabel.ref()));
        generateStatement(node->then.get());
        addCode(breakLabel.def());
        return;
    }

    if (node->nodeType == NodeType::CASE) {
        addCode(labels::label(node->label).def());
        generateStatement(node->left.get());
        return;
    }

    if (node->nodeType == NodeType::BLOCK) {
        for (const Node* statement = node->body.get(); statement; statement = statement->next.get()) {
            generateStatement(statement);
        }
        return;
    }

    if (node->nodeType == NodeType::GOTO) {
        addCode(jmp(node->uniqueLabel));
        return;
    }

    if (node->nodeType == NodeType::LABEL) {
        addCode(labels::label(node->uniqueLabel).def());
        generateStatement(node->left.get());
        return;
    }

    if (node->nodeType == NodeType::RETURN) {
        if (node->left) {
            generateExpression(node->left.get());
        }
        addCode(jmp(labels::label("return", currentFunction->name).ref()));
        return;
    }

    if (node->nodeType == NodeType::EXPRESSION_STATEMENT) {
        generateExpression(node->left.get());
        return;
    }

    Log::error("Invalid statement"sv, node->token);
}

void Generator::generateExpression(const Node* node) {
    assert(node);
    emitLocation(node);

    switch (node->nodeType) {
        case NodeType::NULL_EXPRESSION:
            return;
        case NodeType::NUMBER:
            switch (node->type->kind) {
                case TypeKind::FLOAT: {
                    auto u32 = std::bit_cast<uint32_t>(static_cast<float>(node->floatValue));
                    addCode(mov(EAX, u32));
                    addCode(movq(XMM0, RAX));
                    return;
                }
                case TypeKind::DOUBLE: {
                    auto u64 = std::bit_cast<uint64_t>(node->floatValue);
                    addCode(mov(EAX, u64));
                    addCode(movq(XMM0, RAX));
                    return;
                }
                default:
                    addCode(mov(RAX, node->integerValue));
                    return;
            }
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
            addCode(push_rax());
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
        case NodeType::CAST:
            generateExpression(node->left.get());
            cast(node);
            return;
        case NodeType::MEMORY_CLEAR:
            addCode(mov(RCX, node->variable->type->size));
            addCode(lea(RDI, Address{RBP, node->variable->offset}));
            addCode(mov(AL, 0));
            addCode(rep_stosb());
            return;
        case NodeType::FUNCTION_CALL: {
            int argCount = 0;
            for (const Node* arg = node->arguments.get(); arg; arg = arg->next.get()) {
                generateExpression(arg);
                addCode(push_rax());
                argCount++;
            }
            assert(std::cmp_less_equal(argCount, ARG_REGISTERS64.size()));
            for (int i = argCount - 1; i >= 0; i--) {
                addCode(pop_reg(ARG_REGISTERS64[i]));
            }
            addCode(mov(RAX, 0));
            if (depth % 2 == 0) {
                addCode(call(node->functionName));
            } else {
                addCode(sub(RSP, 8));
                addCode(call(node->functionName));
                addCode(add(RSP, 8));
            }

            switch (node->type->kind) {
                case TypeKind::BOOL:
                    addCode(movzx(EAX, AL));
                    return;
                case TypeKind::CHAR:
                    if (node->type->isUnsigned) {
                        addCode(movzbl(EAX, AL));
                    } else {
                        addCode(movsbl(EAX, AL));
                    }
                    return;
                case TypeKind::SHORT:
                    if (node->type->isUnsigned) {
                        addCode(movzwl(EAX, AX));
                    } else {
                        addCode(movswl(EAX, AX));
                    }
                    return;
                default:
                    return;
            }
        }
            return;
        case NodeType::CONDITIONAL: {
            uint64_t count = labelCount++;
            auto elseLabel = labels::else_(count);
            auto endLabel = labels::end(count);
            generateExpression(node->condition.get());
            addCode(cmp(RAX, 0));
            addCode(je(elseLabel.ref()));
            generateExpression(node->then.get());
            addCode(jmp(endLabel.ref()));
            addCode(elseLabel.def());
            generateExpression(node->els.get());
            addCode(endLabel.def());
        }
            return;
        case NodeType::NOT:
            generateExpression(node->left.get());
            addCode(compareZero(node->left->type.get()));
            addCode(sete(AL));
            addCode(movzx(RAX, AL));
            return;
        case NodeType::BIT_NOT:
            generateExpression(node->left.get());
            addCode(not_(RAX));
            return;
        case NodeType::LOGICAL_AND: {
            uint64_t count = labelCount++;
            auto falseLabel = labels::false_(count);
            auto endLabel = labels::end(count);
            generateExpression(node->left.get());
            addCode(cmp(RAX, 0));
            addCode(je(falseLabel.ref()));
            generateExpression(node->right.get());
            addCode(cmp(RAX, 0));
            addCode(je(falseLabel.ref()));
            addCode(mov(RAX, 1));
            addCode(jmp(endLabel.ref()));
            addCode(falseLabel.def());
            addCode(mov(RAX, 0));
            addCode(endLabel.def());
            return;
        }
        case NodeType::LOGICAL_OR: {
            uint64_t count = labelCount++;
            auto trueLabel = labels::true_(count);
            auto endLabel = labels::end(count);
            generateExpression(node->left.get());
            addCode(cmp(RAX, 0));
            addCode(jne(trueLabel.ref()));
            generateExpression(node->right.get());
            addCode(cmp(RAX, 0));
            addCode(jne(trueLabel.ref()));
            addCode(mov(RAX, 0));
            addCode(jmp(endLabel.ref()));
            addCode(trueLabel.def());
            addCode(mov(RAX, 1));
            addCode(endLabel.def());
            return;
        }
        default:
            break;
    }

    generateExpression(node->right.get());
    addCode(push_rax());

    generateExpression(node->left.get());
    addCode(pop_reg(RDI));

    Register ax;
    Register di;
    Register dx;

    if (node->type->kind == TypeKind::LONG || node->left->type->base) {
        ax = RAX;
        di = RDI;
        dx = RDX;
    } else {
        ax = EAX;
        di = EDI;
        dx = EDX;
    }

    switch (node->nodeType) {
        case NodeType::ADD:
            addCode(add(ax, di));
            return;
        case NodeType::SUB:
            addCode(sub(ax, di));
            return;
        case NodeType::MUL:
            addCode(imul(ax, di));
            return;
        case NodeType::DIV:
        case NodeType::MOD: {
            if (node->type->isUnsigned) {
                addCode(mov(dx, 0), div(di));
            } else {
                if (node->left->type->size == 8) {
                    addCode(cqo());
                } else {
                    addCode(cdq());
                }
                addCode(idiv(di));
            }

            if (node->nodeType == NodeType::MOD) {
                addCode(mov(RAX, RDX));
            }
        }
            return;
        case NodeType::BIT_AND:
            addCode(and_(RAX, RDI));
            return;
        case NodeType::BIT_OR:
            addCode(or_(RAX, RDI));
            return;
        case NodeType::BIT_XOR:
            addCode(xor_(RAX, RDI));
            return;
        case NodeType::EQUAL:
        case NodeType::NOT_EQUAL:
        case NodeType::LESS:
        case NodeType::LESS_EQUAL:
        case NodeType::GREATER:
        case NodeType::GREATER_EQUAL:
            addCode(cmp(ax, di));
            switch (node->nodeType) {
                case NodeType::EQUAL:
                    addCode(sete(AL));
                    break;
                case NodeType::NOT_EQUAL:
                    addCode(setne(AL));
                    break;
                case NodeType::LESS:
                    if (node->left->type->isUnsigned) {
                        addCode(setb(AL));
                    } else {
                        addCode(setl(AL));
                    }
                    break;
                case NodeType::LESS_EQUAL:
                    if (node->left->type->isUnsigned) {
                        addCode(setbe(AL));
                    } else {
                        addCode(setle(AL));
                    }
                    break;
                case NodeType::GREATER:
                    if (node->left->type->isUnsigned) {
                        addCode(seta(AL));
                    } else {
                        addCode(setg(AL));
                    }
                    break;
                case NodeType::GREATER_EQUAL:
                    if (node->left->type->isUnsigned) {
                        addCode(setae(AL));
                    } else {
                        addCode(setge(AL));
                    }
                    break;
                default:
                    std::unreachable();
            }
            addCode(movzx(RAX, AL));
            return;
        case NodeType::SHL:
            addCode(mov(RCX, RDI), shl(ax, CL));
            return;
        case NodeType::SHR:
            addCode(mov(RCX, RDI));
            if (node->left->type->isUnsigned) {
                addCode(shr(ax, CL));
            } else {
                addCode(sar(ax, CL));
            }
            return;
        default:
            break;
    }

    Log::error("Invalid expression"sv, node->token);
}

void Generator::generateFunction(const Object* obj) {
    assert(obj);
    currentFunction = obj;
    lastEmittedLine = 0;

    if (obj->isStatic) {
        addCode(local(obj->name));
    } else {
        addCode(global(obj->name));
    }

    addCode(section::text);
    addCode(labels::label(obj->name).def(),
            // Prologue
            push(RBP),
            mov(RBP, RSP));

    if (obj->stackSize > 0) {
        addCode(sub(RSP, obj->stackSize));
    }

    if (obj->vaArea) {
        int argCount = 0;
        for (auto param = obj->parameters; param; param = param->next.get()) {
            argCount++;
        }
        int offset = obj->vaArea->offset;
        addCode(
            // va_elem
            movl(dword_ptr(Address{RBP, offset}), argCount * 8),
            movl(dword_ptr(Address{RBP, offset + 4}), 0),
            movq(Address{RBP, offset + 16}, RBP),
            addq(Address{RBP, offset + 16}, offset + 24),
            // __reg_save_area__
            movq(Address{RBP, offset + 24}, RDI),
            movq(Address{RBP, offset + 32}, RSI),
            movq(Address{RBP, offset + 40}, RDX),
            movq(Address{RBP, offset + 48}, RCX),
            movq(Address{RBP, offset + 56}, R8),
            movq(Address{RBP, offset + 64}, R9),
            movsd(Address{RBP, offset + 72}, XMM0),
            movsd(Address{RBP, offset + 80}, XMM1),
            movsd(Address{RBP, offset + 88}, XMM2),
            movsd(Address{RBP, offset + 96}, XMM3),
            movsd(Address{RBP, offset + 104}, XMM4),
            movsd(Address{RBP, offset + 112}, XMM5),
            movsd(Address{RBP, offset + 120}, XMM6),
            movsd(Address{RBP, offset + 128}, XMM7)
        );
    }

    int i = 0;
    for (const Object* param = obj->parameters; param; param = param->next.get()) {
        switch (param->type->size) {
            case 1:
                addCode(mov(Address{RBP, param->offset}, ARG_REGISTERS8[i++]));
                break;
            case 2:
                addCode(mov(Address{RBP, param->offset}, ARG_REGISTERS16[i++]));
                break;
            case 4:
                addCode(mov(Address{RBP, param->offset}, ARG_REGISTERS32[i++]));
                break;
            case 8:
                addCode(mov(Address{RBP, param->offset}, ARG_REGISTERS64[i++]));
                break;
            default:
                Log::unreachable();
                break;
        }
    }

    generateStatement(obj->body.get());
    // Epilogue
    addCode(labels::label("return", obj->name).def(), mov(RSP, RBP), pop(RBP), ret());
}

void Generator::emitData(const Object* obj) {
    assert(obj);

    for (const Object* var = obj; var; var = var->next.get()) {
        if (var->isFunction || !var->isDefinition) {
            continue;
        }

        if (var->isStatic) {
            addCode(local(var->name));
        } else {
            addCode(global(var->name));
        }
        addCode(align(var->alignment));

        if (!var->initialData.empty()) {
            assert((var->initialData.size()) == static_cast<size_t>(var->type->size));

            addCode(section::data);
            addCode(labels::label(var->name).def());

            auto relocation = var->relocations.get();
            int pos = 0;

            while (pos < var->type->size) {
                if (relocation && relocation->offset == pos) {
                    addCode(allocate(QUAD, relocation->label, relocation->addend));
                    relocation = relocation->next.get();
                    pos += 8;
                } else {
                    addCode(byte(var->initialData[pos]));
                    pos++;
                }
            }

            continue;
        }

        addCode(section::bss);
        addCode(labels::label(var->name).def());
        addCode(zero(var->type->size));
    }
}

void Generator::emitText(const Object* obj) {
    assert(obj);

    for (const Object* fn = obj; fn; fn = fn->next.get()) {
        if (!fn->isFunction || !fn->isDefinition) {
            continue;
        }
        generateFunction(fn);
    }
}

} // namespace yoctocc
