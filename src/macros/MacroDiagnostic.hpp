#pragma once

#include "SourceOrigin.hpp"
#include "SourcePosition.hpp"

#include <string>
#include <vector>

namespace macro {

// Shared categories, not scanner validation. Owners add semantic diagnostics
// when implementing definitions, arguments and the expansion engine.
enum class MacroDiagnosticCode {
    MissingPrototype,
    InvalidPrototype,
    UnexpectedEnd,
    UnterminatedDefinition,
    ReservedName,
    InvalidArgument,
    RecursiveExpansion,
    ExpansionLimitExceeded,
    InvalidLabel,
    DuplicateLabel,
};

struct MacroDiagnostic {
    MacroDiagnosticCode code{};
    SourcePosition position;
    std::string message;
    std::vector<ExpansionFrame> calls;

    bool operator==(const MacroDiagnostic&) const = default;
};

} // namespace macro
