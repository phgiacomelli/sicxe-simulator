#pragma once

#include "MacroArguments.hpp"
#include "MacroTemplate.hpp"
#include "SourceLine.hpp"

#include <vector>

namespace macro {

struct PreparedMacroDefinition {
    // Zero-based range [beginLine, nextLine) in SubstitutionResult::lines.
    std::size_t beginLine;
    std::size_t nextLine;
    MacroTemplate macroTemplate;
};

struct SubstitutionResult {
    std::vector<SourceLine> lines;
    // Direct nested definitions only. Their children are prepared when called.
    std::vector<PreparedMacroDefinition> definitions;
    // Same indexing as lines; identities survive substitution and captured bindings.
    std::vector<std::vector<GeneratedNameUse>> generatedNames;
};

class ParameterSubstitution {
public:
    // Used by the engine: preserves nested references and captured ancestors.
    // Does not install definitions or expand calls.
    static SubstitutionResult instantiate(const MacroTemplate& macroTemplate,
                                          const BoundMacroArguments& arguments);

    // Substitui os parâmetros da macro expandida.
    //
    // A definição principal não é emitida. Somente seu corpo é
    // produzido, incluindo as definições internas preservadas.
    static std::vector<SourceLine> substitute(const MacroTemplate& macroTemplate,
                                              const BoundMacroArguments& arguments);
};

} // namespace macro
