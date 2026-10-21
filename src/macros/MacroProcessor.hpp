#pragma once

#include "MacroResult.hpp"

#include <string_view>

namespace macro {

// Contract only in package 1; definition belongs to the engine in package 6.
// No I/O, Qt, VM, assembly or shared state between invocations.
MacroResult expand(std::string_view sourceText);

} // namespace macro
