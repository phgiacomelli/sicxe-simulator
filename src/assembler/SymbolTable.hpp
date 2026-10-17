#pragma once

#include "AssemblyDiagnostic.hpp"
#include "AssemblyResult.hpp"
#include "SourceLine.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace assembler {

enum class SymbolDiagnosticCode {
    InvalidSymbolName,
    ReservedSymbolName,
    DuplicateSymbol,
    UndefinedSymbol,
};

struct SymbolDiagnostic {
    SymbolDiagnosticCode code{};
    std::string symbol;
    SourcePosition position;
    std::optional<SourcePosition> previousDefinition;
    std::string message;

    bool operator==(const SymbolDiagnostic&) const = default;
};

struct SymbolReference {
    std::string name;
    std::string source;
    SourcePosition position;

    bool operator==(const SymbolReference&) const = default;
};

using SymbolResolutionResult = std::variant<std::uint32_t, SymbolDiagnostic>;

class SymbolTable {
public:
    static std::string normalizeName(std::string_view name);
    static bool isValidIdentifier(std::string_view name);
    static bool isReservedName(std::string_view name);

    std::optional<SymbolDiagnostic>
    define(std::string_view label, std::uint32_t address, SourcePosition position);
    std::optional<SymbolDiagnostic> define(const SourceIdentifier& label, std::uint32_t address);

    std::optional<SymbolDiagnostic> reference(std::string_view label, SourcePosition position);
    std::optional<SymbolDiagnostic> reference(const SourceIdentifier& label);

    SymbolResolutionResult resolve(std::string_view label, SourcePosition position) const;
    SymbolResolutionResult resolve(const SourceIdentifier& label) const;

    std::optional<AssemblySymbol> find(std::string_view label) const;
    bool isDefined(std::string_view label) const;

    const std::vector<AssemblySymbol>& symbols() const noexcept;
    const std::vector<SymbolReference>& references() const noexcept;
    std::vector<SymbolReference> pendingReferences() const;
    bool hasPendingReferences() const;
    std::vector<SymbolDiagnostic> unresolvedDiagnostics() const;

    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    std::vector<AssemblySymbol> symbols_;
    std::unordered_map<std::string, std::size_t> indexByName_;
    std::vector<SymbolReference> references_;
};

} // namespace assembler
