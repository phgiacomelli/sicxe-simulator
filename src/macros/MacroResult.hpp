#pragma once

#include "MacroDiagnostic.hpp"
#include "SourceOrigin.hpp"

#include <string>
#include <variant>
#include <vector>

namespace macro {

struct MacroSuccess {
    // LF output, one SourceOrigin per emitted physical line, in output order.
    std::string expandedText;
    std::vector<SourceOrigin> origins;

    bool operator==(const MacroSuccess&) const = default;
};

struct MacroFailure {
    std::vector<MacroDiagnostic> diagnostics;

    bool operator==(const MacroFailure&) const = default;
};

// Failure exposes diagnostics only, never partially expanded source text.
using MacroResult = std::variant<MacroSuccess, MacroFailure>;

} // namespace macro
