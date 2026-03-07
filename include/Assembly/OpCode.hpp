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
    MOVSWL,
    LEA,
    ADD,
    SUB,
    MUL,
    IMUL,
    IDIV,
    INC,
    DEC,
    CQO,
    CDQ,
    NEG,
    CMP,
    SETE,
    SETNE,
    SETL,
    SETLE,
    SETG,
    SETGE,
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
    SYSCALL
};

constexpr std::string to_string(OpCode op) {
    using enum OpCode;
    switch (op) {
        case MOV: return "mov";
        case MOVZX: return "movzx";
        case MOVSBQ: return "movsbq";
        case MOVSWQ: return "movswq";
        case MOVSXD: return "movsxd";
        case MOVSBL: return "movsx";
        case MOVSWL: return "movsx";
        case LEA: return "lea";
        case ADD: return "add";
        case SUB: return "sub";
        case MUL: return "mul";
        case IMUL: return "imul";
        case IDIV: return "idiv";
        case INC: return "inc";
        case DEC: return "dec";
        case CQO: return "cqo";
        case CDQ: return "cdq";
        case NEG: return "neg";
        case CMP: return "cmp";
        case SETE: return "sete";
        case SETNE: return "setne";
        case SETL: return "setl";
        case SETLE: return "setle";
        case SETG: return "setg";
        case SETGE: return "setge";
        case PUSH: return "push";
        case POP: return "pop";
        case CALL: return "call";
        case RET: return "ret";
        case JMP: return "jmp";
        case JE: return "je";
        case JNE: return "jne";
        case JL: return "jl";
        case JLE: return "jle";
        case JG: return "jg";
        case JGE: return "jge";
        case SYSCALL: return "syscall";
        default: return "???";
    }
}

} // namespace yoctocc
