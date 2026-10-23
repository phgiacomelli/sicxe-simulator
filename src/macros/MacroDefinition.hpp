#pragma once

#include "SourceLine.hpp"

#include <vector>

namespace macro {

// Owns the lexical skeleton. Formal parsing and scoped substitution are separate.
class MacroDefinition {
public:
    const SourceLine& begin() const noexcept;
    const SourceLine& prototype() const noexcept;
    const std::vector<SourceLine>& body() const noexcept;
    const SourceLine& end() const noexcept;
    const SourceField& name() const noexcept;

private:
    friend class DefinitionCapture;
    MacroDefinition(SourceLine begin,
                    SourceLine prototype,
                    std::vector<SourceLine> body,
                    SourceLine end);

    SourceLine begin_;
    SourceLine prototype_;
    std::vector<SourceLine> body_;
    SourceLine end_;
};

} // namespace macro
