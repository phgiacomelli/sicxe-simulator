#pragma once

#include "SourcePosition.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace macro {

struct ExpansionFrame {
    std::string macroName;
    // Positions in the original source, including calls within macro bodies.
    SourcePosition definition;
    SourcePosition invocation;

    bool operator==(const ExpansionFrame&) const = default;
};

struct SourceOrigin {
    std::size_t expandedLine{1};
    SourcePosition source;
    // Outermost to innermost; empty for a directly copied source line.
    std::vector<ExpansionFrame> calls;

    bool operator==(const SourceOrigin&) const = default;
};

} // namespace macro
