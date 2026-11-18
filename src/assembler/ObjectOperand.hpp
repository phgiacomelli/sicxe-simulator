#pragma once

#include "AssemblyDiagnostic.hpp"
#include "OperandValue.hpp"
#include "SourceLine.hpp"
#include "SymbolTable.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace assembler {

inline constexpr std::size_t MaxObjectSymbolLength = 6;

bool isObjectReservedName(std::string_view name);
bool isValidObjectGlobalName(std::string_view name);

struct LocalSymbolReference {
    std::string name;
    std::string source;
    SourcePosition position;
    std::uint32_t address{};
    std::uint32_t offset{};
    SourcePosition definition;

    bool operator==(const LocalSymbolReference&) const = default;
};

struct ExternalSymbolReference {
    std::string name;
    std::string source;
    std::string declarationSource;
    SourcePosition position;
    SourcePosition declaration;

    bool operator==(const ExternalSymbolReference&) const = default;
};

enum class ObjectValueKind {
    AbsoluteInteger,
    LocalSymbol,
    ExternalSymbol,
};

using ClassifiedOperandValue =
    std::variant<IntegerOperand, LocalSymbolReference, ExternalSymbolReference>;

constexpr ObjectValueKind classifyOperandValue(const ClassifiedOperandValue& value) noexcept {
    if (std::holds_alternative<IntegerOperand>(value)) {
        return ObjectValueKind::AbsoluteInteger;
    }
    if (std::holds_alternative<LocalSymbolReference>(value)) {
        return ObjectValueKind::LocalSymbol;
    }
    return ObjectValueKind::ExternalSymbol;
}

enum class ObjectTermSign {
    Add,
    Subtract,
};

struct ParsedWordTerm {
    ObjectTermSign sign{ObjectTermSign::Add};
    SourcePosition position;
    OperandValue value;

    bool operator==(const ParsedWordTerm&) const = default;
};

struct ParsedObjectWordExpression {
    std::vector<ParsedWordTerm> terms;

    bool operator==(const ParsedObjectWordExpression&) const = default;
};

struct ObjectWordTerm {
    ObjectTermSign sign{ObjectTermSign::Add};
    SourcePosition position;
    ClassifiedOperandValue value;

    constexpr ObjectValueKind kind() const noexcept {
        return classifyOperandValue(value);
    }

    constexpr std::int64_t initialContribution() const noexcept {
        if (const auto* integer = std::get_if<IntegerOperand>(&value)) {
            return sign == ObjectTermSign::Subtract ? -integer->value : integer->value;
        }
        if (const auto* local = std::get_if<LocalSymbolReference>(&value)) {
            const auto offset = static_cast<std::int64_t>(local->offset);
            return sign == ObjectTermSign::Subtract ? -offset : offset;
        }
        return 0;
    }

    bool operator==(const ObjectWordTerm&) const = default;
};

struct ObjectWordExpression {
    std::vector<ObjectWordTerm> terms;

    std::int64_t evaluateInitialAddend() const noexcept;
    std::int64_t localRelocationBalance() const noexcept;

    bool operator==(const ObjectWordExpression&) const = default;
};

using ObjectWordParseResult = std::variant<ParsedObjectWordExpression, OperandDiagnostic>;

ObjectWordParseResult parseObjectWordExpression(std::string_view text, SourcePosition position);
ObjectWordParseResult parseObjectWordExpression(const SourceField& field);

} // namespace assembler
