#pragma once

#include "SourceLine.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace macro {

struct SourceScanResult {
    std::vector<SourceLine> lines;

    // A lexical copy, not expansion: keeps macro directives and all comments.
    std::string normalizedText() const;
};

// Lexical scanning only: no opcode, label, operand or definition validation.
class SourceScanner {
public:
    static SourceScanResult scan(std::string_view sourceText);
};

} // namespace macro
