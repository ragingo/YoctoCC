#include "Parser/Common.hpp"

#include <memory>
#include <vector>
#include "Node/NodeTypes.hpp"
#include "Type.hpp"

namespace yoctocc {

std::unique_ptr<Initializer> createInitializer(const std::shared_ptr<Type>& type, bool isFlexibleArray) {
    auto initializer = std::make_unique<Initializer>();
    initializer->type = type;

    if (type->kind == TypeKind::ARRAY) {
        if (isFlexibleArray && type->size < 0) {
            initializer->isFlexibleArray = true;
        }
        if (type->arraySize > 0) {
            initializer->children.reserve(type->arraySize);

            for (int i = 0; i < type->arraySize; i++) {
                initializer->children.emplace_back(createInitializer(type->base));
            }
        }
        return initializer;
    }

    if (type->kind == TypeKind::STRUCT || type->kind == TypeKind::UNION) {
        for (auto member = type->members.get(); member; member = member->next.get()) {
            initializer->children.emplace_back(createInitializer(member->type));
        }
        return initializer;
    }

    return initializer;
}

} // namespace yoctocc
