#pragma once

#include "AssemblyDiagnostic.hpp"
#include "AssemblyResult.hpp"
#include "LocatedLine.hpp"
#include "SourceLine.hpp"
#include "SourceParser.hpp"
#include "SymbolTable.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace assembler {

enum class PassOneDiagnosticCode {
    InvalidSourceLine,
    DuplicateSymbol,
    UndefinedSymbol,
    InvalidLabel,
    UnknownOperation,
    ExtendedFormatNotAllowed,
    DuplicateStart,
    MisplacedStart,
    InvalidStartOperand,
    MissingEnd,
    DuplicateEnd,
    StatementAfterEnd,
    InvalidEndOperand,
    InvalidDirectiveOperand,
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

AssemblyDiagnostic toAssemblyDiagnostic(const PassOneDiagnostic& diagnostic);

struct PassOneResult {
    std::uint32_t startAddress{};
    std::uint32_t initialPc{};
    std::uint32_t programLength{};
    std::vector<LocatedLine> lines;
    SymbolTable symbolTable;
    std::vector<AssemblySymbol> symbols;
    std::vector<PassOneDiagnostic> diagnostics;

    bool succeeded() const noexcept {
        return diagnostics.empty();
    }

    std::optional<AssemblySymbol> findSymbol(std::string_view name) const;
};

class PassOne {
public:
    static PassOneResult run(const SourceParseResult& parseResult);
    static PassOneResult run(std::span<const SourceLine> parsedLines);
};

} // namespace assembler
