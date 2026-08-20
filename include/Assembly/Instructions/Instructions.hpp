#pragma once
#include "Assembly/Assembly.hpp"
#include <concepts>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace yoctocc {

namespace {
using enum OpCode;
using enum Register;
} // namespace

template <typename T>
// clang-format off
concept OperandType =
    (std::is_enum_v<std::remove_cvref_t<T>> && std::is_integral_v<std::underlying_type_t<std::remove_cvref_t<T>>>) ||
    std::is_integral_v<std::remove_cvref_t<T>> ||
    std::is_same_v<std::remove_cvref_t<T>, std::string> ||
    std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
    std::is_convertible_v<T, const char*> ||
    std::is_same_v<std::remove_cvref_t<T>, Register> ||
    std::is_same_v<std::remove_cvref_t<T>, Address<Register>> ||
    std::is_same_v<std::remove_cvref_t<T>, RipRelativeAddress>;
// clang-format on

template <OperandType T>
inline constexpr std::string operand_to_string(T&& operand) {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, Register>) {
        return to_string(operand);
    } else if constexpr (std::is_same_v<U, Address<Register>>) {
        return to_string(std::forward<T>(operand));
    } else if constexpr (std::is_same_v<U, RipRelativeAddress>) {
        return to_string(std::forward<T>(operand));
    } else if constexpr (std::is_integral_v<U>) {
        return to_string(operand);
    } else if constexpr (std::is_enum_v<U>) {
        return to_string(std::to_underlying(operand));
    } else if constexpr (std::is_same_v<U, std::string>) {
        return operand;
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        return std::string(operand);
    } else if constexpr (std::is_convertible_v<T, const char*>) {
        return std::string(operand);
    } else {
        static_assert(false, "Unsupported operand type");
        return "";
    }
}

template <typename... Operands>
inline constexpr std::string instruction(OpCode opCode, Operands&&... operands) {
    std::vector<std::string> operandStrings = {operand_to_string(std::forward<Operands>(operands))...};
    std::string result = to_string(opCode);
    for (size_t i = 0; i < operandStrings.size(); ++i) {
        result += (i == 0 ? " " : ", ");
        result += operandStrings[i];
    }
    return result;
}
static_assert(instruction(MOV, RAX, 42) == "mov rax, 42");
static_assert(instruction(RET) == "ret");

template <OpCode Op>
struct Instruction {
    constexpr std::string operator()(OperandType auto&&... operands) const {
        return instruction(Op, std::forward<decltype(operands)>(operands)...);
    }
};

inline constexpr Instruction<MOV> mov;
inline constexpr Instruction<MOVZX> movzx;
inline constexpr Instruction<MOVSBQ> movsbq;
inline constexpr Instruction<MOVSWQ> movswq;
inline constexpr Instruction<MOVSXD> movsxd;
inline constexpr Instruction<MOVSBL> movsbl;
inline constexpr Instruction<MOVZBL> movzbl;
inline constexpr Instruction<MOVSWL> movswl;
inline constexpr Instruction<MOVZWL> movzwl;
inline constexpr Instruction<MOVQ> movq;
inline constexpr Instruction<MOVSS> movss;
inline constexpr Instruction<MOVSD> movsd;
inline constexpr Instruction<MOVL> movl;
inline constexpr Instruction<LEA> lea;
inline constexpr Instruction<ADD> add;
inline constexpr Instruction<ADDQ> addq;
inline constexpr Instruction<SUB> sub;
inline constexpr Instruction<INC> inc;
inline constexpr Instruction<DEC> dec;
inline constexpr Instruction<IMUL> imul;
inline constexpr Instruction<MUL> mul;
inline constexpr Instruction<DIV> div;
inline constexpr Instruction<IDIV> idiv;
inline constexpr Instruction<CQO> cqo;
inline constexpr Instruction<CDQ> cdq;
inline constexpr Instruction<NEG> neg;
inline constexpr Instruction<NOT> not_;
inline constexpr Instruction<AND> and_;
inline constexpr Instruction<OR> or_;
inline constexpr Instruction<XOR> xor_;
inline constexpr Instruction<SHL> shl;
inline constexpr Instruction<SHR> shr;
inline constexpr Instruction<SAR> sar;
inline constexpr Instruction<CMP> cmp;
inline constexpr Instruction<SETE> sete;
inline constexpr Instruction<SETNE> setne;
inline constexpr Instruction<SETL> setl;
inline constexpr Instruction<SETB> setb;
inline constexpr Instruction<SETLE> setle;
inline constexpr Instruction<SETBE> setbe;
inline constexpr Instruction<SETG> setg;
inline constexpr Instruction<SETA> seta;
inline constexpr Instruction<SETGE> setge;
inline constexpr Instruction<SETAE> setae;
inline constexpr Instruction<SYSCALL> syscall_;
inline constexpr Instruction<PUSH> push;
inline constexpr Instruction<POP> pop;
inline constexpr Instruction<RET> ret;
inline constexpr Instruction<JMP> jmp;
inline constexpr Instruction<JE> je;
inline constexpr Instruction<JNE> jne;
inline constexpr Instruction<CALL> call;
inline constexpr Instruction<REP_STOSB> rep_stosb;
inline constexpr Instruction<CVTSD2SS> cvtsd2ss;
inline constexpr Instruction<CVTSI2SD> cvtsi2sd;
inline constexpr Instruction<CVTSI2SS> cvtsi2ss;
inline constexpr Instruction<CVTSS2SD> cvtss2sd;
inline constexpr Instruction<CVTTSD2SI> cvttsd2si;
inline constexpr Instruction<CVTTSS2SI> cvttss2si;

static_assert(mov(RAX, 42) == "mov rax, 42");
static_assert(add(Address{RAX}, 42) == "add [rax], 42");
static_assert(sub(Address{RAX}, R8) == "sub [rax], r8");

} // namespace yoctocc
