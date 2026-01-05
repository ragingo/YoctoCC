#include "Assembly/AssemblyWriter.hpp"

#include <cassert>
#include <format>
#include "Assembly/Assembly.hpp"

namespace yoctocc {

void AssemblyWriter::section(Section section) noexcept {
    _sections.emplace(section, std::vector<std::string>{});
}

void AssemblyWriter::section_text_symbol(LinkerDirective directive, const std::string& symbol) noexcept {
    auto it = _sections.find(Section::TEXT);
    assert(it != _sections.end());
    if (it != _sections.end()) {
        it->second.emplace_back(std::format("    {} {}\n", to_string(directive), symbol));
    }
}

void AssemblyWriter::func(const std::string& label, std::vector<std::string> body) noexcept {
    _code.emplace_back(std::format("{}:\n", label));
    for (const auto& line : body) {
        _code.emplace_back("    " + line + "\n");
    }
}

void AssemblyWriter::func(const std::string& label, std::function<std::vector<std::string>()> body) noexcept {
    func(label, body());
}

void AssemblyWriter::compile() noexcept {
    std::vector<std::string> final_code{};
    final_code.emplace_back(".intel_syntax noprefix\n");

    for (const auto& [section, lines] : _sections) {
        final_code.emplace_back(std::format("{}\n", to_string(section)));
        for (const auto& line : lines) {
            final_code.emplace_back(line);
        }
    }

    for (const auto& line : _code) {
        final_code.emplace_back(line);
    }

    // 実行可能スタックが不要であることを示すセクション（警告を抑制）
    final_code.emplace_back(".section .note.GNU-stack,\"\",%progbits\n");

    _code = std::move(final_code);
}

void AssemblyWriter::clear() noexcept {
    _code.clear();
    _sections.clear();
}

const std::vector<std::string>& AssemblyWriter::get_code() const noexcept {
    return _code;
}

} // namespace yoctocc
