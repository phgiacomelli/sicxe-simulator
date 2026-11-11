
#pragma once

#include "MacroArguments.hpp"
#include "MacroTemplate.hpp"
#include "SourceLine.hpp"

#include <vector>

namespace macro {

class ParameterSubstitution {
public:
    // Substitui os parâmetros da macro expandida.
    //
    // A definição principal não é emitida. Somente seu corpo é
    // produzido, incluindo as definições internas preservadas.
    static std::vector<SourceLine> substitute(
        const MacroTemplate& macroTemplate,
        const BoundMacroArguments& arguments);
};

} // namespace macro
