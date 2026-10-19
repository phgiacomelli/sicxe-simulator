#include "SymbolTable.hpp"

#include "InstructionCatalog.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace assembler {
namespace {

constexpr std::array<std::string_view, 8> ReservedDirectives = {
    "BASE",
    "BYTE",
    "END",
    "NOBASE",
    "RESB",
    "RESW",
    "START",
    "WORD",
};

bool isAsciiLetter(char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

bool isAsciiDigit(char character) {
    return character >= '0' && character <= '9';
}

SymbolDiagnostic makeInvalidNameDiagnostic(std::string_view label, SourcePosition position) {
    return SymbolDiagnostic{
        SymbolDiagnosticCode::InvalidSymbolName,
        std::string{label},
        position,
        std::nullopt,
        "Rotulo '" + std::string{label} +
            "' invalido; use uma letra seguida de letras, digitos ou '_'.",
    };
}

SymbolDiagnostic makeReservedNameDiagnostic(std::string_view label, SourcePosition position) {
    return SymbolDiagnostic{
        SymbolDiagnosticCode::ReservedSymbolName,
        std::string{label},
        position,
        std::nullopt,
        "Rotulo '" + std::string{label} + "' colide com mnemonico ou diretiva reservada.",
    };
}

SymbolDiagnostic makeDuplicateDiagnostic(std::string_view label,
                                         SourcePosition position,
                                         std::string_view previousLabel,
                                         SourcePosition previousPosition) {
    return SymbolDiagnostic{
        SymbolDiagnosticCode::DuplicateSymbol,
        std::string{label},
        position,
        previousPosition,
        "Rotulo '" + std::string{label} + "' redefinido; '" + std::string{previousLabel} +
            "' foi definido antes na linha " + std::to_string(previousPosition.line) + ", coluna " +
            std::to_string(previousPosition.column) + ".",
    };
}

SymbolDiagnostic makeUndefinedDiagnostic(std::string_view label, SourcePosition position) {
    return SymbolDiagnostic{
        SymbolDiagnosticCode::UndefinedSymbol,
        std::string{label},
        position,
        std::nullopt,
        "Simbolo '" + std::string{label} + "' nao foi definido.",
    };
}

std::optional<SymbolDiagnostic> validateIdentifier(const SourceIdentifier& identifier,
                                                   std::string_view& rawNameOut) {
    rawNameOut = identifier.source.text.empty() ? std::string_view{identifier.key}
                                                : std::string_view{identifier.source.text};
    if (!SymbolTable::isValidIdentifier(rawNameOut)) {
        return makeInvalidNameDiagnostic(rawNameOut, identifier.source.position);
    }
    if (!identifier.key.empty() && SymbolTable::normalizeName(rawNameOut) != identifier.key) {
        return makeInvalidNameDiagnostic(rawNameOut, identifier.source.position);
    }
    return std::nullopt;
}

} // namespace

AssemblyDiagnostic toAssemblyDiagnostic(const SymbolDiagnostic& diagnostic) {
    AssemblyDiagnosticCode code{};
    switch (diagnostic.code) {
        case SymbolDiagnosticCode::InvalidSymbolName:
            code = AssemblyDiagnosticCode::InvalidSymbolName;
            break;
        case SymbolDiagnosticCode::ReservedSymbolName:
            code = AssemblyDiagnosticCode::ReservedSymbolName;
            break;
        case SymbolDiagnosticCode::DuplicateSymbol:
            code = AssemblyDiagnosticCode::DuplicateSymbol;
            break;
        case SymbolDiagnosticCode::UndefinedSymbol:
            code = AssemblyDiagnosticCode::UndefinedSymbol;
            break;
    }
    return AssemblyDiagnostic{code, diagnostic.position, diagnostic.message};
}

std::string SymbolTable::normalizeName(std::string_view name) {
    std::string normalized{name};
    for (char& character : normalized) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return normalized;
}

bool SymbolTable::isValidIdentifier(std::string_view name) {
    if (name.empty() || !isAsciiLetter(name.front())) {
        return false;
    }
    for (std::size_t index = 1; index < name.size(); ++index) {
        const char character = name[index];
        if (!isAsciiLetter(character) && !isAsciiDigit(character) && character != '_') {
            return false;
        }
    }
    return true;
}

bool SymbolTable::isReservedName(std::string_view name) {
    if (!isValidIdentifier(name)) {
        return false;
    }
    const std::string normalized = normalizeName(name);
    return std::binary_search(ReservedDirectives.begin(), ReservedDirectives.end(), normalized) ||
           std::holds_alternative<std::reference_wrapper<const InstructionSpec>>(
               findInstruction(normalized));
}

std::optional<SymbolDiagnostic>
SymbolTable::define(std::string_view label, std::uint32_t address, SourcePosition position) {
    if (!isValidIdentifier(label)) {
        return makeInvalidNameDiagnostic(label, position);
    }
    std::string key = normalizeName(label);
    if (isReservedName(key)) {
        return makeReservedNameDiagnostic(label, position);
    }
    if (const auto existing = indexByName_.find(key); existing != indexByName_.end()) {
        return makeDuplicateDiagnostic(label,
                                       position,
                                       definitionSources_[existing->second],
                                       symbols_[existing->second].definition);
    }
    indexByName_.emplace(key, symbols_.size());
    symbols_.push_back(AssemblySymbol{std::move(key), address, position});
    definitionSources_.emplace_back(label);
    return std::nullopt;
}

std::optional<SymbolDiagnostic> SymbolTable::define(const SourceIdentifier& label,
                                                    std::uint32_t address) {
    std::string_view rawName;
    if (const auto invalid = validateIdentifier(label, rawName)) {
        return invalid;
    }
    return define(rawName, address, label.source.position);
}

std::optional<SymbolDiagnostic> SymbolTable::reference(std::string_view label,
                                                       SourcePosition position) {
    if (!isValidIdentifier(label)) {
        return makeInvalidNameDiagnostic(label, position);
    }
    std::string key = normalizeName(label);
    if (isReservedName(key)) {
        return makeReservedNameDiagnostic(label, position);
    }
    references_.push_back(SymbolReference{std::move(key), std::string{label}, position});
    return std::nullopt;
}

std::optional<SymbolDiagnostic> SymbolTable::reference(const SourceIdentifier& label) {
    std::string_view rawName;
    if (const auto invalid = validateIdentifier(label, rawName)) {
        return invalid;
    }
    return reference(rawName, label.source.position);
}

SymbolResolutionResult SymbolTable::resolve(std::string_view label, SourcePosition position) const {
    if (!isValidIdentifier(label)) {
        return makeInvalidNameDiagnostic(label, position);
    }
    const std::string key = normalizeName(label);
    if (isReservedName(key)) {
        return makeReservedNameDiagnostic(label, position);
    }
    const auto existing = indexByName_.find(key);
    if (existing == indexByName_.end()) {
        return makeUndefinedDiagnostic(label, position);
    }
    return symbols_[existing->second].address;
}

SymbolResolutionResult SymbolTable::resolve(const SourceIdentifier& label) const {
    std::string_view rawName;
    if (const auto invalid = validateIdentifier(label, rawName)) {
        return *invalid;
    }
    return resolve(rawName, label.source.position);
}

std::optional<AssemblySymbol> SymbolTable::find(std::string_view label) const {
    if (!isValidIdentifier(label)) {
        return std::nullopt;
    }
    const std::string key = normalizeName(label);
    const auto existing = indexByName_.find(key);
    if (existing == indexByName_.end()) {
        return std::nullopt;
    }
    return symbols_[existing->second];
}

bool SymbolTable::isDefined(std::string_view label) const {
    return find(label).has_value();
}

const std::vector<AssemblySymbol>& SymbolTable::symbols() const noexcept {
    return symbols_;
}

const std::vector<SymbolReference>& SymbolTable::references() const noexcept {
    return references_;
}

std::vector<SymbolReference> SymbolTable::pendingReferences() const {
    std::vector<SymbolReference> pending;
    for (const auto& entry : references_) {
        if (!indexByName_.contains(entry.name)) {
            pending.push_back(entry);
        }
    }
    return pending;
}

bool SymbolTable::hasPendingReferences() const {
    for (const auto& entry : references_) {
        if (!indexByName_.contains(entry.name)) {
            return true;
        }
    }
    return false;
}

std::vector<SymbolDiagnostic> SymbolTable::unresolvedDiagnostics() const {
    std::vector<SymbolDiagnostic> diagnostics;
    for (const auto& entry : references_) {
        if (!indexByName_.contains(entry.name)) {
            diagnostics.push_back(makeUndefinedDiagnostic(entry.source, entry.position));
        }
    }
    return diagnostics;
}

std::size_t SymbolTable::size() const noexcept {
    return symbols_.size();
}

bool SymbolTable::empty() const noexcept {
    return symbols_.empty();
}

} // namespace assembler
