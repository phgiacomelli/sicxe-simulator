#pragma once

#include "AssemblyDiagnostic.hpp"
#include "AssemblyResult.hpp"
#include "InstructionCatalog.hpp"
#include "InstructionOperand.hpp"
#include "LocatedLine.hpp"
#include "ObjectOperand.hpp"
#include "SourceLine.hpp"
#include "SourceParser.hpp"
#include "SymbolTable.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace assembler {

enum class ObjectDirectiveKind {
    Start,
    End,
    Base,
    NoBase,
    ExtDef,
    ExtRef,
    Byte,
    Word,
    Resb,
    Resw,
};

struct ObjectSectionHeader {
    std::string name;
    std::string source;
    SourcePosition position;
    std::uint32_t startAddress{};
    std::uint32_t length{};

    bool operator==(const ObjectSectionHeader&) const = default;
};

struct ExportedSymbol {
    std::string name;
    std::string source;
    SourcePosition declaration;
    std::uint32_t address{};
    std::uint32_t offset{};
    SourcePosition definition;

    bool operator==(const ExportedSymbol&) const = default;
};

struct ExternalSymbol {
    std::string name;
    std::string source;
    SourcePosition declaration;

    bool operator==(const ExternalSymbol&) const = default;
};

struct ObjectEntryPoint {
    std::string name;
    std::string source;
    SourcePosition position;
    std::uint32_t address{};
    std::uint32_t offset{};
    SourcePosition definition;

    bool operator==(const ObjectEntryPoint&) const = default;
};

struct ObjectMemoryOperand {
    InstructionAddressing mode{InstructionAddressing::Simple};
    bool indexed{false};
    ClassifiedOperandValue target;

    constexpr ObjectValueKind targetKind() const noexcept {
        return classifyOperandValue(target);
    }

    bool operator==(const ObjectMemoryOperand&) const = default;
};

using ObjectBaseOperand = std::variant<IntegerOperand, LocalSymbolReference>;

struct ObjectLocatedLine {
    LocatedLine located{};
    std::uint32_t offset{};
    std::optional<ObjectDirectiveKind> directive;
    std::optional<ObjectMemoryOperand> memoryOperand;
    std::optional<ObjectWordExpression> wordExpression;
    std::optional<std::vector<std::uint8_t>> byteData;
    std::optional<ObjectBaseOperand> baseOperand;
    std::vector<ExportedSymbol> exportedSymbols;
    std::vector<ExternalSymbol> externalSymbols;

    constexpr const SourceLine& source() const noexcept {
        return located.source;
    }

    constexpr LocatedLineCategory category() const noexcept {
        return located.category;
    }

    constexpr std::uint32_t address() const noexcept {
        return located.address;
    }

    constexpr std::size_t sizeBytes() const noexcept {
        return located.sizeBytes;
    }

    bool operator==(const ObjectLocatedLine&) const = default;
};

enum class ObjectPassOneDiagnosticCode {
    InvalidSourceLine,
    MissingStart,
    DuplicateStart,
    MisplacedStart,
    MissingSectionName,
    InvalidSectionName,
    SectionNameTooLong,
    InvalidStartOperand,
    MissingEnd,
    DuplicateEnd,
    StatementAfterEnd,
    InvalidEndOperand,
    ExternalInEnd,
    InvalidLabel,
    DuplicateSymbol,
    UndefinedSymbol,
    SymbolNameTooLong,
    DuplicateExternalDeclaration,
    ExternalDeclarationConflict,
    LocalExternalConflict,
    UndefinedExportedSymbol,
    UnknownOperation,
    ExtendedFormatNotAllowed,
    InvalidDirectiveOperand,
    ExternalInBase,
    InvalidInstructionOperand,
    ExternalInFormat3,
    AddressOutOfRange,
};

struct ObjectPassOneDiagnostic {
    ObjectPassOneDiagnosticCode code{};
    SourcePosition position;
    std::string message;
    std::optional<SourcePosition> previousDefinition;
    std::optional<OperandDiagnostic> operandDiagnostic;

    bool operator==(const ObjectPassOneDiagnostic&) const = default;
};

AssemblyDiagnostic toAssemblyDiagnostic(const ObjectPassOneDiagnostic& diagnostic);

struct ObjectPassOneResult {
    ObjectSectionHeader section{};
    std::uint32_t startAddress{};
    std::uint32_t initialPc{};
    std::uint32_t programLength{};
    std::optional<ObjectEntryPoint> entry;
    std::vector<ObjectLocatedLine> lines;
    SymbolTable symbolTable;
    std::vector<AssemblySymbol> symbols;
    std::vector<ExportedSymbol> exports;
    std::vector<ExternalSymbol> externalReferences;
    std::vector<ObjectPassOneDiagnostic> diagnostics;

    bool succeeded() const noexcept {
        return diagnostics.empty();
    }

    constexpr std::uint32_t toSectionOffset(std::uint32_t localAddress) const noexcept {
        return localAddress >= startAddress ? (localAddress - startAddress) : 0U;
    }

    std::optional<AssemblySymbol> findSymbol(std::string_view name) const;
    std::optional<ExportedSymbol> findExport(std::string_view name) const;
    std::optional<ExternalSymbol> findExternalReference(std::string_view name) const;
};

class ObjectPassOne {
public:
    static ObjectPassOneResult run(std::string_view sourceText);
    static ObjectPassOneResult run(const SourceParseResult& parseResult);
    static ObjectPassOneResult run(std::span<const SourceLine> parsedLines);
};

} // namespace assembler
