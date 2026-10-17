#pragma once

#include <cstddef>
#include <string>

namespace assembler {

struct SourcePosition {
    // One-based physical line and byte column; a tab counts as one byte.
    std::size_t line{};
    std::size_t column{};

    bool operator==(const SourcePosition&) const = default;
};

enum class AssemblyDiagnosticCode {
    InvalidLabel,
    MissingOperation,
    InvalidOperation,
    UnterminatedQuote,
    UnexpectedField,
};

struct AssemblyDiagnostic {
    AssemblyDiagnosticCode code{};
    SourcePosition position;
    std::string message;

    bool operator==(const AssemblyDiagnostic&) const = default;
};

} // namespace assembler
