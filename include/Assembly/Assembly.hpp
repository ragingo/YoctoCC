#pragma once
#include <cstdint>
#include <string>

namespace yoctocc {

constexpr std::string SYSTEM_ENTRY_POINT = "_start";
constexpr std::string USER_ENTRY_POINT = "main";

} // namespace yoctocc

#include "Address.hpp"
#include "GasDirective.hpp"
#include "Label.hpp"
#include "OpCode.hpp"
#include "Register.hpp"
#include "SystemCall.hpp"

#include "Instructions/Instructions.hpp"
#include "AssemblyWriter.hpp"
