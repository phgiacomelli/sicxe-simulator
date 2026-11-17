#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace linker {

struct ObjectPosition {
    std::string file;
    // One-based physical record and byte column.
    std::size_t record{1};
    std::size_t column{1};

    bool operator==(const ObjectPosition&) const = default;
};

enum class ObjectDiagnosticCode {
    InvalidRecordType,
    InvalidRecordLength,
    InvalidHex,
    InvalidName,
    MissingRecord,
    UnexpectedRecord,
    NonZeroOrigin,
    ValueOutOfRange,
    DuplicateName,
    InvalidTextLength,
    OverlappingText,
    InvalidModification,
    MissingText,
    UndeclaredReference,
};

struct ObjectDiagnostic {
    ObjectDiagnosticCode code{};
    ObjectPosition position;
    std::string message;

    bool operator==(const ObjectDiagnostic&) const = default;
};

struct ObjectFailure {
    std::vector<ObjectDiagnostic> diagnostics;

    bool operator==(const ObjectFailure&) const = default;
};

} // namespace linker
