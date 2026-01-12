#include <fstream>
#include <memory>
#include <print>
#include <string>
#include "Assembly/Assembly.hpp"
#include "Generator.hpp"
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Tokenizer.hpp"
#include "Parser.hpp"

using namespace yoctocc;

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
    Log::sourceFileName = source_file;
    auto token = tokenize(ifs);

    std::println("Parsing...");
    Parser parser{};
    auto program = parser.parse(token);

    std::println("Generating...");
    Generator generator{};
    AssemblyWriter writer{};
    writer.compile(generator.run(program));

    std::println("Writing...");
    for (const auto& line : writer.getCode()) {
        ofs << line;
    }
    ofs.close();

    return EXIT_SUCCESS;
}
