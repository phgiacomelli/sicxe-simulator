#pragma once

#include "AssemblyDiagnostic.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace assembler {

struct SourceField {
    std::string text;
    SourcePosition position;

    bool operator==(const SourceField&) const = default;
};

struct SourceIdentifier {
    SourceField source;
    std::string key;

    bool operator==(const SourceIdentifier&) const = default;
};

struct ParsedStatement {
    std::optional<SourceIdentifier> label;
    SourceIdentifier operation;
    std::optional<SourceField> operand;

    bool operator==(const ParsedStatement&) const = default;
};

enum class SourceLineKind {
    Blank,
    Comment,
    Statement,
    Invalid,
};

struct SourceLine {
    std::size_t number{};
    std::string text;
    SourceLineKind kind{SourceLineKind::Blank};
    // Present only for a syntactically valid Statement.
    std::optional<ParsedStatement> statement;
    // Includes the '.' or ';' marker and preserves the rest of the line.
    std::optional<SourceField> comment;

    bool operator==(const SourceLine&) const = default;
};

} // namespace assembler
