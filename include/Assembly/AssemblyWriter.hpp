#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace yoctocc {

class AssemblyWriter final {
public:
    inline void addLine(const std::string& line) noexcept {
        _code.emplace_back(line + "\n");
    }
    void compile(const std::vector<std::string>& code) noexcept;
    void clear() noexcept;
    const std::vector<std::string>& getCode() const noexcept;

private:
    std::vector<std::string> _code{};
};

} // namespace yoctocc
