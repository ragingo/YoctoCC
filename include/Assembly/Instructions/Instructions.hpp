#pragma once
#include <concepts>
#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Assembly/Assembly.hpp"

namespace yoctocc {

    namespace {
        using enum OpCode;
        using enum Register;
    }

    template <typename T>
    concept OperandType =
        (std::is_enum_v<std::remove_cvref_t<T>> && std::is_integral_v<std::underlying_type_t<std::remove_cvref_t<T>>>) ||
        std::is_integral_v<std::remove_cvref_t<T>> ||
        std::is_same_v<std::remove_cvref_t<T>, std::string> ||
        std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
        std::is_convertible_v<T, const char*> ||
        std::is_same_v<std::remove_cvref_t<T>, Register> ||
        std::is_same_v<std::remove_cvref_t<T>, Address<Register>> ||
        std::is_same_v<std::remove_cvref_t<T>, RipRelativeAddress>;

    template<OperandType T>
    inline constexpr std::string operand_to_string(T&& operand) {
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, Register>) {
            return to_string(operand);
        } else if constexpr (std::is_same_v<U, Address<Register>>) {
            return to_string(std::move(operand));
        } else if constexpr (std::is_same_v<U, RipRelativeAddress>) {
            return to_string(std::move(operand));
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
        std::vector<std::string> operandStrings = {
            operand_to_string(std::forward<Operands>(operands))...
        };
        std::string result = to_string(opCode) + " ";
        for (size_t i = 0; i < operandStrings.size(); ++i) {
            result += operandStrings[i];
            if (i + 1 < operandStrings.size()) {
                result += ", ";
            }
        }
        return result;
    }
    static_assert(instruction(MOV, RAX, 42) == "mov rax, 42");

    template <OpCode Op>
    struct Instruction {
        constexpr std::string operator()(OperandType auto&&... operands) const {
            return instruction(Op, std::forward<decltype(operands)>(operands)...);
        }
    };

    inline constexpr Instruction<MOV> mov;
    inline constexpr Instruction<MOVZX> movzx;
    inline constexpr Instruction<MOVSBQ> movsbq;
    inline constexpr Instruction<LEA> lea;
    inline constexpr Instruction<ADD> add;
    inline constexpr Instruction<SUB> sub;
    inline constexpr Instruction<INC> inc;
    inline constexpr Instruction<DEC> dec;
    inline constexpr Instruction<IMUL> imul;
    inline constexpr Instruction<MUL> mul;
    inline constexpr Instruction<IDIV> idiv;
    inline constexpr Instruction<CQO> cqo;
    inline constexpr Instruction<NEG> neg;
    inline constexpr Instruction<CMP> cmp;
    inline constexpr Instruction<SETE> sete;
    inline constexpr Instruction<SETNE> setne;
    inline constexpr Instruction<SETL> setl;
    inline constexpr Instruction<SETLE> setle;
    inline constexpr Instruction<SETG> setg;
    inline constexpr Instruction<SETGE> setge;
    inline constexpr Instruction<SYSCALL> syscall_;
    inline constexpr Instruction<PUSH> push;
    inline constexpr Instruction<POP> pop;
    inline constexpr Instruction<RET> ret;
    inline constexpr Instruction<JMP> jmp;
    inline constexpr Instruction<JE> je;
    inline constexpr Instruction<CALL> call;

    static_assert(mov(RAX, 42) == "mov rax, 42");
    static_assert(add(Address{RAX}, 42) == "add [rax], 42");
    static_assert(sub(Address{RAX}, R8) == "sub [rax], r8");

} // namespace yoctocc
