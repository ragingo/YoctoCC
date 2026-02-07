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
    std::vector<std::string> run(Object* obj);

private:
    static constexpr size_t alignTo(size_t n, size_t align) {
        return (n + align - 1) / align * align;
    }
    void load(const Type* type);
    void store(const Type* type);
    void assignLocalVariableOffsets(Object* obj);
    void generateAddress(const Node* node);
    void generateStatement(const Node* node);
    void generateExpression(const Node* node);
    void generateFunction(const Object* obj);
    void emitData(const Object* obj);
    void emitText(const Object* obj);

    inline void addCode(std::string&& line) {
        lines.emplace_back(std::move(line));
    }

    template<typename... Args>
    void addCode(Args&&... args) {
        (lines.emplace_back(std::forward<Args>(args)), ...);
    }

private:
    std::vector<std::string> lines{};
    const Object* currentFunction = nullptr;
    uint64_t labelCount = 0UL;
};

} // namespace yoctocc
