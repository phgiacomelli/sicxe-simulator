#pragma once

#include "AssemblyDiagnostic.hpp"
#include "LocatedLine.hpp"
#include "OperandValue.hpp"
#include "SourceLine.hpp"
#include "SymbolTable.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace assembler {

enum class DataDiagnosticCode {
    InvalidSourceLine,
    InvalidDirective,
    MissingOperand,
    InvalidByteLiteral,
    NonAsciiCharacter,
    OddHexLength,
    InvalidHexDigit,
    InvalidOperand,
    SizeOutOfRange,
    LayoutMismatch,
};

struct DataDiagnostic {
    DataDiagnosticCode code{};
    SourcePosition position;
    std::string message;
    // Keep package 3's exact diagnostic for numeric/symbolic failures.
    std::optional<OperandDiagnostic> operandDiagnostic;
};

AssemblyDiagnostic toAssemblyDiagnostic(const DataDiagnostic& diagnostic);

struct DataMeasurement {
    std::size_t sizeBytes{};
    bool reservation{};
};

struct DataBytes {
    std::vector<std::uint8_t> bytes;
};

struct DataReservation {
    std::size_t sizeBytes{};
};

using EncodedData = std::variant<DataBytes, DataReservation>;
using DataMeasureResult = std::variant<DataMeasurement, DataDiagnostic>;
using DataEncodeResult = std::variant<EncodedData, DataDiagnostic>;

// Records WORD references, including forward references; never emits bytes.
DataMeasureResult measureData(const SourceLine& line, SymbolTable& symbols);
// Resolves WORD references and verifies the fixed layout; never fills reservations.
DataEncodeResult encodeData(const LocatedLine& line, const SymbolTable& symbols);

} // namespace assembler
