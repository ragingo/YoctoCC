#pragma once
#include <fstream>
#include <memory>

namespace yoctocc {

struct Token;

std::unique_ptr<Token> tokenize(std::ifstream& ifs);

} // namespace yoctocc
