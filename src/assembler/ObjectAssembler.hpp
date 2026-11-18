#pragma once

#include "ObjectPassTwo.hpp"

#include <string_view>

namespace assembler {

// Independent object mode; absolute assemble() keeps its existing contract.
ObjectAssemblyResult assembleObject(std::string_view sourceText);

} // namespace assembler
