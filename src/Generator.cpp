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

std::vector<std::string> pushf() {
    depth++;
    std::vector<std::string> ret;
    ret.emplace_back(sub(RSP, 8));
    ret.emplace_back(movsd(Address{RSP}, XMM0));
    return ret;
}

std::vector<std::string> popf(Register reg) {
    depth--;
    std::vector<std::string> ret;
    ret.emplace_back(movsd(reg, Address{RSP}));
    ret.emplace_back(add(RSP, 8));
    return ret;
}

enum TypeID {
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
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
        case FLOAT:
            return F32;
        case DOUBLE:
            return F64;
        default:
            return U64;
    }
}

using CastCode = std::span<const std::string>;

const std::array<std::string, 0> empty{};
const std::array i32i8  = {movsbl(EAX, AL)};
const std::array i32u8  = {movzbl(EAX, AL)};
const std::array i32i16 = {movswl(EAX, AX)};
const std::array i32u16 = {movzwl(EAX, AX)};
const std::array i32f32 = {cvtsi2ss(XMM0, EAX)};
const std::array i32i64 = {movsxd(RAX, EAX)};
const std::array i32f64 = {cvtsi2sd(XMM0, EAX)};

const std::array u32f32 = {mov(EAX, EAX), cvtsi2ss(XMM0, RAX)};
const std::array u32i64 = {mov(EAX, EAX)};
const std::array u32f64 = {mov(EAX, EAX), cvtsi2sd(XMM0, RAX)};

const std::array i64f32 = {cvtsi2ss(XMM0, RAX)};
const std::array i64f64 = {cvtsi2sd(XMM0, RAX)};

const std::array u64f32 = {cvtsi2ss(XMM0, RAX)};
const std::array u64f64 = {
    test(RAX, RAX),
    js(labels::label("1").ref(Label::Direction::FORWARD)),
    pxor(XMM0, XMM0),
    cvtsi2sd(XMM0, RAX),
    jmp(labels::label("2").ref(Label::Direction::FORWARD)),
    labels::label("1").def(),
    mov(RDI, RAX),
    and_(RAX, 1),
    pxor(XMM0, XMM0),
    shr(RDI),
    or_(RDI, RAX),
    cvtsi2sd(XMM0, RDI),
    addsd(XMM0, XMM0),
    labels::label("2").def(),
};

const std::array f32i8  = {cvttss2si(EAX, XMM0), movsbl(EAX, AL)};
const std::array f32u8  = {cvttss2si(EAX, XMM0), movzbl(EAX, AL)};
const std::array f32i16 = {cvttss2si(EAX, XMM0), movswl(EAX, AX)};
const std::array f32u16 = {cvttss2si(EAX, XMM0), movzwl(EAX, AX)};
const std::array f32i32 = {cvttss2si(EAX, XMM0)};
const std::array f32u32 = {cvttss2si(RAX, XMM0)};
const std::array f32i64 = {cvttss2si(RAX, XMM0)};
const std::array f32u64 = {cvttss2si(RAX, XMM0)};
const std::array f32f64 = {cvtss2sd(XMM0, XMM0)};

const std::array f64i8  = {cvttsd2si(EAX, XMM0), movsbl(EAX, AL)};
const std::array f64u8  = {cvttsd2si(EAX, XMM0), movzbl(EAX, AL)};
const std::array f64i16 = {cvttsd2si(EAX, XMM0), movswl(EAX, AX)};
const std::array f64u16 = {cvttsd2si(EAX, XMM0), movzwl(EAX, AX)};
const std::array f64i32 = {cvttsd2si(EAX, XMM0)};
const std::array f64u32 = {cvttsd2si(RAX, XMM0)};
const std::array f64f32 = {cvtsd2ss(XMM0, XMM0)};
const std::array f64i64 = {cvttsd2si(RAX, XMM0)};
const std::array f64u64 = {cvttsd2si(RAX, XMM0)};

// castTable[from][to]
// clang-format off
constexpr std::array<std::array<CastCode, 10>, 10> castTable = {{
    // i8     i16     i32     i64     u8     u16     u32     u64     f32     f64
    {{ empty, empty,  empty,  i32i64, i32u8, i32u16, empty,  i32i64, i32f32, i32f64 }}, // i8
    {{ i32i8, empty,  empty,  i32i64, i32u8, i32u16, empty,  i32i64, i32f32, i32f64 }}, // i16
    {{ i32i8, i32i16, empty,  i32i64, i32u8, i32u16, empty,  i32i64, i32f32, i32f64 }}, // i32
    {{ i32i8, i32i16, empty,  empty,  i32u8, i32u16, empty,  empty,  i64f32, i64f64 }}, // i64

    {{ i32i8, empty,  empty,  i32i64, empty, empty,  empty,  i32i64, i32f32, i32f64 }}, // u8
    {{ i32i8, i32i16, empty,  i32i64, i32u8, empty,  empty,  i32i64, i32f32, i32f64 }}, // u16
    {{ i32i8, i32i16, empty,  u32i64, i32u8, i32u16, empty,  u32i64, u32f32, u32f64 }}, // u32
    {{ i32i8, i32i16, empty,  empty,  i32u8, i32u16, empty,  empty,  u64f32, u64f64 }}, // u64

    {{ f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, empty,  f32f64 }}, // f32
    {{ f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32, empty  }}, // f64
}};
// clang-format on

std::vector<std::string> compareZero(const Type* type) {
    if (type->kind == TypeKind::FLOAT) {
        return {xorps(XMM1, XMM1), ucomiss(XMM0, XMM1)};
    } else if (type->kind == TypeKind::DOUBLE) {
        return {xorpd(XMM1, XMM1), ucomisd(XMM0, XMM1)};
    }

    if (type::isInteger(type) && type->size <= 4) {
        return {cmp(EAX, 0)};
    } else {
        return {cmp(RAX, 0)};
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

    if (type::is(to, BOOL)) {
        addCode(compareZero(from));
        addCode(setne(AL), movzx(EAX, AL));
        return;
    }

    const CastCode& code = castTable[getTypeID(from)][getTypeID(to)];
    for (const auto& line : code) {
        addCode(line);
    }
}

void Generator::load(const Type* type) {
    using enum TypeKind;
    assert(type);

    switch (type->kind) {
        case ARRAY:
        case STRUCT:
        case UNION:
            // 何もしない
            return;
        case FLOAT:
            addCode(movss(XMM0, Address{RAX}));
            return;
        case DOUBLE:
            addCode(movsd(XMM0, Address{RAX}));
            return;
        default:
          break;
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

    switch (type->kind) {
        case STRUCT:
        case UNION: {
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
        case FLOAT:
            addCode(movss(Address{RDI}, XMM0));
            return;
        case DOUBLE:
            addCode(movsd(Address{RDI}, XMM0));
            return;
        default:
          break;
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
        addCode(compareZero(node->condition->type.get()));
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
            addCode(compareZero(node->condition->type.get()));
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
        addCode(compareZero(node->condition->type.get()));
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
                    addCode(mov(RAX, u64));
                    addCode(movq(XMM0, RAX));
                    return;
                }
                default:
                    addCode(mov(RAX, node->integerValue));
                    return;
            }
        case NodeType::NEGATE:
            generateExpression(node->left.get());
            if (type::isFloat(node->type.get())) {
                if (node->type->kind== TypeKind::FLOAT) {
                    addCode(mov(RAX, 1));
                    addCode(shl(RAX, 31));
                    addCode(movq(XMM1, RAX));
                    addCode(xorps(XMM0, XMM1));
                } else if (node->type->kind == TypeKind::DOUBLE) {
                    addCode(mov(RAX, 1));
                    addCode(shl(RAX, 63));
                    addCode(movq(XMM1, RAX));
                    addCode(xorpd(XMM0, XMM1));
                }
            }
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
            addCode(compareZero(node->condition->type.get()));
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
            addCode(compareZero(node->left->type.get()));
            addCode(je(falseLabel.ref()));
            generateExpression(node->right.get());
            addCode(compareZero(node->right->type.get()));
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
            addCode(compareZero(node->left->type.get()));
            addCode(jne(trueLabel.ref()));
            generateExpression(node->right.get());
            addCode(compareZero(node->right->type.get()));
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

    if (type::isFloat(node->left->type.get())) {
        generateExpression(node->right.get());
        addCode(pushf());
        generateExpression(node->left.get());
        addCode(popf(XMM1));

        switch (node->nodeType) {
            case NodeType::ADD:
                if (node->left->type->kind == TypeKind::FLOAT) {
                    addCode(addss(XMM0, XMM1));
                } else {
                    addCode(addsd(XMM0, XMM1));
                }
                return;
            case NodeType::SUB:
                if (node->left->type->kind == TypeKind::FLOAT) {
                    addCode(subss(XMM0, XMM1));
                } else {
                    addCode(subsd(XMM0, XMM1));
                }
                return;
            case NodeType::MUL:
                if (node->left->type->kind == TypeKind::FLOAT) {
                    addCode(mulss(XMM0, XMM1));
                } else {
                    addCode(mulsd(XMM0, XMM1));
                }
                return;
            case NodeType::DIV:
                if (node->left->type->kind == TypeKind::FLOAT) {
                    addCode(divss(XMM0, XMM1));
                } else {
                    addCode(divsd(XMM0, XMM1));
                }
                return;
            case NodeType::EQUAL:
            case NodeType::NOT_EQUAL:
            case NodeType::LESS:
            case NodeType::LESS_EQUAL:
                if (node->left->type->kind == TypeKind::FLOAT) {
                    addCode(ucomiss(XMM1, XMM0));
                } else {
                    addCode(ucomisd(XMM1, XMM0));
                }
                switch (node->nodeType) {
                    case NodeType::EQUAL:
                        addCode(sete(AL), setnp(DL), and_(AL, DL));
                        break;
                    case NodeType::NOT_EQUAL:
                        addCode(setne(AL), setp(DL), or_(AL, DL));
                        break;
                    case NodeType::LESS:
                        addCode(seta(AL));
                        break;
                    case NodeType::LESS_EQUAL:
                        addCode(setae(AL));
                        break;
                    default:
                    std::unreachable();
                }
                addCode(and_(AL, 1), movzx(RAX, AL));
                return;
            default:
                Log::error("invalid expression", node->token);
                std::unreachable();
        }
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
            addCode(and_(ax, di));
            return;
        case NodeType::BIT_OR:
            addCode(or_(ax, di));
            return;
        case NodeType::BIT_XOR:
            addCode(xor_(ax, di));
            return;
        case NodeType::EQUAL:
        case NodeType::NOT_EQUAL:
        case NodeType::LESS:
        case NodeType::LESS_EQUAL:
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
