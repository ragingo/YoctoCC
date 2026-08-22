#include "Assembly/AssemblyWriter.hpp"

#include <cassert>
#include <format>
#include "Assembly/Assembly.hpp"

namespace yoctocc {

using enum GasDirective;
using enum Register;
using enum SystemCall;
using namespace directive;

void AssemblyWriter::compile(const std::vector<std::string>& code) noexcept {
    _code.emplace_back(intelSyntax(false) + "\n");
    _code.emplace_back(sections::text + "\n");
    _code.emplace_back("\n");
    _code.emplace_back("# ===== Generated Code Start =====\n");
    _code.emplace_back("\n");

    for (auto&& line : code) {
        _code.emplace_back(line);
        _code.emplace_back("\n");
    }

    _code.emplace_back("\n");
    _code.emplace_back("# ===== Generated Code End =====\n");
    _code.emplace_back("\n");

    _code.emplace_back(labels::label("return").def());
    _code.emplace_back(std::format("    {}\n", mov(RDI, RAX)));
    _code.emplace_back(std::format("    {}\n", mov(RAX, std::to_underlying(EXIT))));
    _code.emplace_back(std::format("    {}\n", syscall_()));

    // 実行可能スタックが不要であることを示すセクション（警告を抑制）
    _code.emplace_back(section(".note.GNU-stack", "\"\"", "@progbits") + "\n");
}

void AssemblyWriter::clear() noexcept {
    _code.clear();
}

const std::vector<std::string>& AssemblyWriter::getCode() const noexcept {
    return _code;
}

} // namespace yoctocc
