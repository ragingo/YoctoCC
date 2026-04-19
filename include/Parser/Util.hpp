#pragma once
#include "ParseScope.hpp"
#include "Token.hpp"
#include "Type.hpp"


namespace yoctocc::parser {
inline bool isTypeName(const Token* token, const ParseScope& scope) {
    bool isTypeName = type::isTypeName(token);
    if (isTypeName) {
        return true;
    }
    return scope.findTypeDef(token) != nullptr;
}
} // namespace yoctocc::parser
