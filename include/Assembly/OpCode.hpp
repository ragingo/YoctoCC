#pragma once
#include <string>

namespace yoctocc {

enum class OpCode {
    MOV,
    MOVZX,
    MOVSBQ,
    MOVSWQ,
    MOVSXD,
    MOVSBL,
    MOVZBL,
    MOVSWL,
    MOVZWL,
    MOVQ,
    MOVSS,
    MOVSD,
    MOVL,
    LEA,
    ADD,
    ADDQ,
    SUB,
    MUL,
    IMUL,
    DIV,
    IDIV,
    INC,
    DEC,
    CQO,
    CDQ,
    NEG,
    NOT,
    AND,
    OR,
    XOR,
    SHL,
    SHR,
    SAR,
    CMP,
    SETE,
    SETNE,
    SETL,
    SETB,
    SETLE,
    SETBE,
    SETG,
    SETA,
    SETGE,
    SETAE,
    PUSH,
    POP,
    CALL,
    RET,
    JMP,
    JE,
    JNE,
    JL,
    JLE,
    JG,
    JGE,
    SYSCALL,
    REP_STOSB
};

constexpr std::string to_string(OpCode op) {
    using enum OpCode;
    switch (op) {
        case MOV:
            return "mov";
        case MOVZX:
            return "movzx";
        case MOVSBQ:
            return "movsbq";
        case MOVSWQ:
            return "movswq";
        case MOVSXD:
            return "movsxd";
        case MOVSBL:
            return "movsx";
        case MOVZBL:
            return "movzx";
        case MOVSWL:
            return "movsx";
        case MOVZWL:
            return "movzx";
        case MOVQ:
            return "movq";
        case MOVSS:
            return "movss";
        case MOVSD:
            return "movsd";
        case MOVL:
            return "mov";
        case LEA:
            return "lea";
        case ADD:
            return "add";
        case ADDQ:
            return "addq";
        case SUB:
            return "sub";
        case MUL:
            return "mul";
        case IMUL:
            return "imul";
        case DIV:
            return "div";
        case IDIV:
            return "idiv";
        case INC:
            return "inc";
        case DEC:
            return "dec";
        case CQO:
            return "cqo";
        case CDQ:
            return "cdq";
        case NEG:
            return "neg";
        case NOT:
            return "not";
        case AND:
            return "and";
        case OR:
            return "or";
        case XOR:
            return "xor";
        case SHL:
            return "shl";
        case SHR:
            return "shr";
        case SAR:
            return "sar";
        case CMP:
            return "cmp";
        case SETE:
            return "sete";
        case SETNE:
            return "setne";
        case SETL:
            return "setl";
        case SETB:
            return "setb";
        case SETLE:
            return "setle";
        case SETBE:
            return "setbe";
        case SETG:
            return "setg";
        case SETA:
            return "seta";
        case SETGE:
            return "setge";
        case SETAE:
            return "setae";
        case PUSH:
            return "push";
        case POP:
            return "pop";
        case CALL:
            return "call";
        case RET:
            return "ret";
        case JMP:
            return "jmp";
        case JE:
            return "je";
        case JNE:
            return "jne";
        case JL:
            return "jl";
        case JLE:
            return "jle";
        case JG:
            return "jg";
        case JGE:
            return "jge";
        case SYSCALL:
            return "syscall";
        case REP_STOSB:
            return "rep stosb";
        default:
            return "???";
    }
}

} // namespace yoctocc
