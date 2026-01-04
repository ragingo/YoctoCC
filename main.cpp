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

    AssemblyWriter writer{};
    writer.section(TEXT);
    writer.section_text_symbol(GLOBAL, SYSTEM_ENTRY_POINT);

    std::println("Tokenizing...");
    auto token = tokenize(ifs);

    std::println("Parsing...");
    Parser parser{};
    auto program = parser.parse(token);

    std::println("Generating...");
    Generator generator{};
    auto lines = generator.run(program);

    std::vector<std::string> startupCode{};
    startupCode.emplace_back(call(USER_ENTRY_POINT));
    startupCode.emplace_back(jmp(".L.return"));
    for (const auto& line : lines) {
        startupCode.emplace_back(line);
    }
    startupCode.emplace_back(".L.return:");
    startupCode.emplace_back(mov(RDI, RAX));
    startupCode.emplace_back(mov(RAX, std::to_underlying(EXIT)));
    startupCode.emplace_back(syscall());
    writer.func(SYSTEM_ENTRY_POINT, startupCode);

    writer.compile();

    for (const auto &line : writer.get_code()) {
        ofs << line;
    }
    ofs.close();

    return EXIT_SUCCESS;
}
