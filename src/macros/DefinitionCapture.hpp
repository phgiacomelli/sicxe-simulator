#pragma once

#include "MacroDefinition.hpp"
#include "MacroDiagnostic.hpp"

#include <cstddef>
#include <span>
#include <variant>

namespace macro {

struct CapturedDefinition {
    MacroDefinition definition;
    // Zero-based index after the matching MCEND in the supplied span.
    std::size_t nextLine;
};

using DefinitionCaptureResult = std::variant<CapturedDefinition, MacroDiagnostic>;

class DefinitionCapture {
public:
    // beginIndex selects MCDEFN. Also diagnoses an isolated MCEND.
    // Does not install anything; the engine decides when to capture and define.
    static DefinitionCaptureResult capture(std::span<const SourceLine> lines,
                                           std::size_t beginIndex);
};

} // namespace macro
