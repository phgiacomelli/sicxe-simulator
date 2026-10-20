#pragma once

#include "InstructionCatalog.hpp"
#include "LocatedLine.hpp"
#include "OperandValue.hpp"
#include "SymbolTable.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace assembler {

// nullopt disables base addressing; an engaged zero is a valid active base.
using BaseState = std::optional<std::uint32_t>;

enum class InstructionEncodingCode {
    InvalidSourceLine,
    InvalidInstruction,
    InvalidOperand,
    LayoutMismatch,
    InvalidBaseState,
    AddressOutOfRange,
    DisplacementOutOfRange,
};

struct InstructionEncodingDiagnostic {
    InstructionEncodingCode code{};
    SourcePosition position;
    std::string message;
    std::variant<std::monostate, InstructionDiagnostic, OperandDiagnostic> cause;
};

struct EncodedInstruction {
    std::vector<std::uint8_t> bytes;
};

using InstructionEncodingResult = std::variant<EncodedInstruction, InstructionEncodingDiagnostic>;

InstructionEncodingResult
encodeInstruction(const LocatedLine& line, const SymbolTable& symbols, BaseState baseState);
AssemblyDiagnostic toAssemblyDiagnostic(const InstructionEncodingDiagnostic& diagnostic);

} // namespace assembler
