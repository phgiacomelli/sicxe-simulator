#pragma once

#include "SourceLine.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace assembler {

enum class OperandPattern { None, OneRegister, TwoRegisters, RegisterAndCount, Memory };
enum class InstructionFormats { Format2, Format3, Format34 };
enum class InstructionForm : std::uint8_t { Format2 = 2, Format3 = 3, Format4 = 4 };

struct InstructionSpec {
    std::string_view mnemonic;
    std::uint8_t opcode;
    InstructionFormats formats;
    OperandPattern operands;

    constexpr std::uint8_t size() const {
        return formats == InstructionFormats::Format2 ? 2 : 3;
    }
    constexpr bool supportsExtended() const {
        return formats == InstructionFormats::Format34;
    }
    bool operator==(const InstructionSpec&) const = default;
};

enum class InstructionDiagnosticCode {
    UnknownInstruction,
    InvalidExtendedFormat,
    InvalidSourceLine,
    InstructionMismatch,
    MissingOperand,
    UnexpectedOperand,
    WrongOperandCount,
    InvalidRegister,
    InvalidShiftCount,
    InvalidIndexing,
    MissingValue,
    UnsupportedOperandSyntax,
    InvalidValue,
    NegativeImmediate,
    InvalidFormat2Operand,
};

struct InstructionDiagnostic {
    InstructionDiagnosticCode code;
    SourceField source;
    std::string message;
};

// Converts to existing shared codes without changing the package 1 public enum.
AssemblyDiagnostic toAssemblyDiagnostic(const InstructionDiagnostic& diagnostic);

using InstructionLookupResult =
    std::variant<std::reference_wrapper<const InstructionSpec>, InstructionDiagnostic>;

std::span<const InstructionSpec> instructionCatalog();
InstructionLookupResult findInstruction(std::string_view mnemonic,
                                        SourcePosition position = {1, 1});
InstructionLookupResult findInstruction(const SourceIdentifier& operation);

} // namespace assembler
