#pragma once

#include "AssemblyDiagnostic.hpp"
#include "SourceLine.hpp"

#include <string_view>
#include <vector>

namespace assembler {

struct SourceParseResult {
    std::vector<SourceLine> lines;
    std::vector<AssemblyDiagnostic> diagnostics;

    bool succeeded() const {
        return diagnostics.empty();
    }
};

class SourceParser {
public:
    static SourceParseResult parse(std::string_view sourceText);
};

} // namespace assembler
