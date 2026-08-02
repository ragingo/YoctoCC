#include "Parser/Common.hpp"

#include <memory>
#include <vector>
#include "Node/NodeTypes.hpp"
#include "Type.hpp"

namespace yoctocc {

std::unique_ptr<Initializer> createInitializer(const std::shared_ptr<Type>& type) {
    auto initializer = std::make_unique<Initializer>();
    initializer->type = type;

    if (type->kind == TypeKind::ARRAY) {
        initializer->children.reserve(type->arraySize);
        for (int i = 0; i < type->arraySize; i++) {
            initializer->children.emplace_back(createInitializer(type->base));
        }
    }

    return initializer;
}

} // namespace yoctocc
