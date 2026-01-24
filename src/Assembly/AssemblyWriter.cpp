#include "Assembly/AssemblyWriter.hpp"

#include <cassert>
#include <format>
#include "Assembly/Assembly.hpp"

namespace yoctocc {

using enum GasDirective;
using enum Register;
using enum SystemCall;

void AssemblyWriter::compile(const std::vector<std::string>& code) noexcept {
    std::string RETURN_LABEL = ".L.return";
    std::vector<std::string> result{};
    result.emplace_back(".intel_syntax noprefix\n");
    result.emplace_back(std::format("{}\n", to_string(TEXT)));
    result.emplace_back(std::format("    {} {}\n", to_string(GLOBAL), SYSTEM_ENTRY_POINT));
    result.emplace_back(std::format("{}:\n", SYSTEM_ENTRY_POINT));
    result.emplace_back(std::format("    {}\n", call(USER_ENTRY_POINT)));
    result.emplace_back(std::format("    {}\n", jmp(RETURN_LABEL)));

    result.emplace_back("\n");
    result.emplace_back("# ===== Generated Code Start =====\n");
    result.emplace_back("\n");

    for (auto&& line : code) {
        result.emplace_back(std::format("{}\n", line));
    }

    result.emplace_back("\n");
    result.emplace_back("# ===== Generated Code End =====\n");
    result.emplace_back("\n");

    result.emplace_back(std::format("{}:\n", RETURN_LABEL));
    result.emplace_back(std::format("    {}\n", mov(RDI, RAX)));
    result.emplace_back(std::format("    {}\n", mov(RAX, std::to_underlying(EXIT))));
    result.emplace_back(std::format("    {}\n", syscall_()));

    // 実行可能スタックが不要であることを示すセクション（警告を抑制）
    result.emplace_back(".section .note.GNU-stack,\"\",%progbits\n");

    _code = std::move(result);
}

void AssemblyWriter::clear() noexcept {
    _code.clear();
}

const std::vector<std::string>& AssemblyWriter::getCode() const noexcept {
    return _code;
}

} // namespace yoctocc
