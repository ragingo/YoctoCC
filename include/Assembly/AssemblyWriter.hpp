#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Section.hpp"
#include "LinkerDirective.hpp"

namespace yoctocc {

class AssemblyWriter final {
public:
    void compile(const std::vector<std::string>& code) noexcept;
    void clear() noexcept;
    const std::vector<std::string>& getCode() const noexcept;

private:
    std::vector<std::string> _code{};
};

} // namespace yoctocc
