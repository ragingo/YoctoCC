#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace yoctocc {

struct Node;
struct Object;
struct Type;

class Generator final {
public:
    std::vector<std::string> run(const std::shared_ptr<Object>& obj);

private:
    static constexpr size_t alignTo(size_t n, size_t align) {
        return (n + align - 1) / align * align;
    }
    void load(const std::shared_ptr<Type>& type);
    void store();
    void assignLocalVariableOffsets(const std::shared_ptr<Object>& obj);
    void generateAddress(const std::shared_ptr<Node>& node);
    void generateStatement(const std::shared_ptr<Node>& node);
    void generateExpression(const std::shared_ptr<Node>& node);
    void generateFunction(const std::shared_ptr<Object>& obj);
    void emitData(std::shared_ptr<Object> obj);
    void emitText(std::shared_ptr<Object> obj);

    inline void addCode(std::string&& line) {
        lines.emplace_back(std::move(line));
    }

    template<typename... Args>
    void addCode(Args&&... args) {
        (lines.emplace_back(std::forward<Args>(args)), ...);
    }

private:
    std::vector<std::string> lines{};
    std::shared_ptr<Object> currentFunction;
    uint64_t labelCount = 0UL;
};

} // namespace yoctocc
