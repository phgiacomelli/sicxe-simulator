#pragma once

#include "MacroResult.hpp"

#include <string_view>

namespace macro {

// Limits per source: 128 active calls and 100000 instantiated body lines.
// No I/O, Qt, VM, assembly or shared state between invocations.
MacroResult expand(std::string_view sourceText);

} // namespace macro
