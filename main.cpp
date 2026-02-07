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
    std::string sourceFile = argv[1];
    std::string outputFile = (argc >= 3) ? argv[2] : "build/program.s";

    std::ifstream ifs(sourceFile);
    if (!ifs) {
        Log::error("Failed to open source file");
        return EXIT_FAILURE;
    }
    std::ofstream ofs(outputFile);
    if (!ofs) {
        Log::error("Failed to open output file");
        return EXIT_FAILURE;
    }

    std::println("Tokenizing...");
    Log::sourceFileName = sourceFile;
    auto tokenChain = tokenize(ifs);

    std::println("Parsing...");
    Parser parser{};
    auto program = parser.parse(tokenChain.get());

    std::println("Generating...");
    Generator generator{};
    AssemblyWriter writer{};
    writer.addLine(directive::file(1, sourceFile));
    writer.compile(generator.run(program.get()));

    std::println("Writing...");
    for (const auto& line : writer.getCode()) {
        ofs << line;
    }
    ofs.close();

    return EXIT_SUCCESS;
}
