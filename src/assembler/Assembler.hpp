#pragma once

#include "AssemblyResult.hpp"

#include <string_view>

namespace assembler {

AssemblyResult assemble(std::string_view sourceText);

} // namespace assembler
