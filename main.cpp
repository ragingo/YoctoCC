#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>
#include "Assembly/Assembly.hpp"
#include "Generator.hpp"
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Tokenizer.hpp"
#include "Parser.hpp"

using namespace yoctocc;
using enum LinkerDirective;
using enum OpCode;
using enum Register;
using enum Section;
using enum SystemCall;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        Log::error("Usage: yoctocc <source_file> [output_file]");
        return EXIT_FAILURE;
    }
    std::string source_file = argv[1];
    std::string output_file = (argc >= 3) ? argv[2] : "build/program.s";

    std::ifstream ifs(source_file);
    if (!ifs) {
        Log::error("Failed to open source file");
        return EXIT_FAILURE;
    }
    std::ofstream ofs(output_file);
    if (!ofs) {
        Log::error("Failed to open output file");
        return EXIT_FAILURE;
    }

    std::println("Tokenizing...");
    auto token = tokenize(ifs);

    std::println("Parsing...");
    Parser parser{};
    auto program = parser.parse(token);

    std::println("Generating...");
    Generator generator{};
    auto lines = generator.run(program);

    ofs << ".intel_syntax noprefix\n";
    ofs << to_string(Section::TEXT) << "\n";
    ofs << "    " << to_string(LinkerDirective::GLOBAL) << " " << SYSTEM_ENTRY_POINT << "\n";
    ofs << SYSTEM_ENTRY_POINT << ":\n";
    ofs << "    " << call(USER_ENTRY_POINT) << "\n";
    ofs << "    " << jmp(".L.return") << "\n";

    // Generator の出力（.data セクション、.text セクションの関数定義）
    for (const auto& line : lines) {
        ofs << line << "\n";
    }

    // _start の終了処理
    ofs << ".L.return:\n";
    ofs << "    " << mov(RDI, RAX) << "\n";
    ofs << "    " << mov(RAX, std::to_underlying(EXIT)) << "\n";
    ofs << "    " << syscall_() << "\n";

    ofs << ".section .note.GNU-stack,\"\",\%progbits\n";

    ofs.close();

    return EXIT_SUCCESS;
}
