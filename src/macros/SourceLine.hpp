#pragma once

#include "SourcePosition.hpp"

#include <optional>
#include <string>

namespace macro {

struct SourceField {
    std::string text;
    SourcePosition position;

    bool operator==(const SourceField&) const = default;
};

enum class SourceLineKind {
    Blank,
    Comment,
    Code,
};

enum class MacroDirective {
    None,
    DefinitionBegin,
    DefinitionEnd,
};

struct SourceLine {
    std::size_t number{1};
    std::string text;
    SourceLineKind kind{SourceLineKind::Blank};
    std::optional<SourceField> label;
    std::optional<SourceField> operation;
    std::optional<SourceField> operand;
    std::optional<SourceField> comment;
    // Lexical information only: the caller decides whether it is a macro error.
    std::optional<SourcePosition> openQuote;
    bool terminated{};

    MacroDirective directive() const;

    bool operator==(const SourceLine&) const = default;
};

} // namespace macro
