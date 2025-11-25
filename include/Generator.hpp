#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace yoctocc {

struct Function;
struct Node;

class Generator final {
public:
    std::vector<std::string> run(const std::shared_ptr<Function>& func);

private:
    static constexpr size_t alignTo(size_t n, size_t align) {
        return (n + align - 1) / align * align;
    }
    void assignLocalVariableOffsets(const std::shared_ptr<Function>& func);
    void generateAddress(const std::shared_ptr<Node>& node);
    void generateStatement(const std::shared_ptr<Node>& node);
    void generateExpression(const std::shared_ptr<Node>& node);
    void generateFunction(const std::shared_ptr<Function>& func);

private:
    std::vector<std::string> lines{};
    std::shared_ptr<Function> currentFunction;
    uint64_t labelCount = 0UL;
};

} // namespace yoctocc
