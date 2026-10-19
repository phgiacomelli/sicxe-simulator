#pragma once

#include "AddressLimits.hpp"
#include "AssemblyDiagnostic.hpp"
#include "SourceLine.hpp"
#include "SymbolTable.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace assembler {

enum class IntegerBase {
    Decimal,
    Hexadecimal,
};

struct IntegerOperand {
    std::int64_t value{};
    IntegerBase base{IntegerBase::Decimal};
    bool negative{false};
    std::string text;
    SourcePosition position;

    bool operator==(const IntegerOperand&) const = default;
};

struct SymbolOperand {
    std::string name;
    std::string source;
    SourcePosition position;

    SourceIdentifier identifier() const {
        return SourceIdentifier{SourceField{source, position}, name};
    }

    bool operator==(const SymbolOperand&) const = default;
};

using OperandValue = std::variant<IntegerOperand, SymbolOperand>;

enum class OperandDiagnosticCode {
    EmptyOperand,
    UnsupportedExpression,
    InvalidInteger,
    IntegerOverflow,
    NegativeValueNotAllowed,
    ValueOutOfRange,
    InvalidSymbolName,
    ReservedSymbolName,
    DuplicateSymbol,
    SymbolNotAllowed,
    UndefinedSymbol,
};

struct OperandDiagnostic {
    OperandDiagnosticCode code{};
    std::string token;
    SourcePosition position;
    std::string message;

    bool operator==(const OperandDiagnostic&) const = default;
};

AssemblyDiagnostic toAssemblyDiagnostic(const OperandDiagnostic& diagnostic);

struct IntegerRange {
    std::int64_t min{};
    std::int64_t max{};
    bool allowNegative{false};

    bool operator==(const IntegerRange&) const = default;
};

inline constexpr IntegerRange Address12BitRange{0, 0xFFF, false};
inline constexpr IntegerRange Address20BitRange{0, MaxAddressableByte, false};
inline constexpr IntegerRange Word24BitRange{-0x800000, 0xFFFFFF, true};
inline constexpr IntegerRange NonNegativeIntegerRange{
    0, std::numeric_limits<std::int64_t>::max(), false};

using OperandParseResult = std::variant<OperandValue, OperandDiagnostic>;
using OperandNumberResult = std::variant<std::int64_t, OperandDiagnostic>;

OperandParseResult parseValue(std::string_view token, SourcePosition position);
OperandParseResult parseValue(const SourceField& field);

OperandNumberResult validateInteger(const OperandValue& operand, IntegerRange range);
OperandNumberResult
parseKnownInteger(std::string_view token, SourcePosition position, IntegerRange range);
OperandNumberResult parseKnownInteger(const SourceField& field, IntegerRange range);

std::optional<OperandDiagnostic>
referenceOperand(const OperandValue& operand, SymbolTable& symbols, IntegerRange integerRange);
OperandNumberResult
resolveOperand(const OperandValue& operand, const SymbolTable& symbols, IntegerRange integerRange);

} // namespace assembler
