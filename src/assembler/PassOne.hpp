#pragma once

#include "AssemblyDiagnostic.hpp"
#include "AssemblyResult.hpp"
#include "LocatedLine.hpp"
#include "SourceLine.hpp"
#include "SourceParser.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace assembler {

inline constexpr std::uint32_t MaxAddressableByte = 0xFFFFFU;
inline constexpr std::uint32_t AddressSpaceBytes = 0x100000U;

enum class PassOneDiagnosticCode {
    InvalidSourceLine,
    InvalidLabel,
    DuplicateSymbol,
    UndefinedSymbol,
    DuplicateStart,
    MisplacedStart,
    InvalidStartOperand,
    DuplicateEnd,
    MissingEnd,
    StatementAfterEnd,
    InvalidEndOperand,
    InvalidDirectiveOperand,
    UnknownOperation,
    ExtendedFormatNotAllowed,
    InvalidInstructionOperand,
    DataDirectiveNotIntegrated,
    AddressOutOfRange,
};

struct PassOneDiagnostic {
    PassOneDiagnosticCode code{};
    SourcePosition position;
    std::string message;
    std::optional<SourcePosition> previousDefinition;

    bool operator==(const PassOneDiagnostic&) const = default;
};

struct PassOneResult {
    std::uint32_t startAddress{};
    std::uint32_t initialPc{};
    std::uint32_t programLength{};
    std::vector<LocatedLine> lines;
    std::vector<AssemblySymbol> symbols;
    std::vector<PassOneDiagnostic> diagnostics;

    bool succeeded() const noexcept {
        return diagnostics.empty();
    }

    std::optional<AssemblySymbol> findSymbol(std::string_view name) const;

    bool operator==(const PassOneResult&) const = default;
};

class PassOne {
public:
    static PassOneResult run(const SourceParseResult& parseResult);
    static PassOneResult run(std::span<const SourceLine> parsedLines);
};

} // namespace assembler
