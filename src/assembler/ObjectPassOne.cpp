#include "ObjectPassOne.hpp"

#include "AddressLimits.hpp"
#include "DataDirective.hpp"
#include "OperandValue.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace assembler {
namespace {

constexpr std::uint64_t MaxMemoryAddress = MaxAddressableByte;
constexpr std::uint64_t MemoryCapacity = AddressSpaceBytes;

bool isHorizontalSpace(char character) {
    return character == ' ' || character == '\t';
}

SourceField trimSourceField(const SourceField& field) {
    std::size_t start = 0;
    while (start < field.text.size() && isHorizontalSpace(field.text[start])) {
        ++start;
    }
    std::size_t end = field.text.size();
    while (end > start && isHorizontalSpace(field.text[end - 1])) {
        --end;
    }
    return SourceField{
        field.text.substr(start, end - start),
        SourcePosition{field.position.line, field.position.column + start},
    };
}

std::optional<ObjectDirectiveKind> findObjectDirectiveKind(std::string_view mnemonic) {
    if (mnemonic == "START") {
        return ObjectDirectiveKind::Start;
    }
    if (mnemonic == "END") {
        return ObjectDirectiveKind::End;
    }
    if (mnemonic == "BASE") {
        return ObjectDirectiveKind::Base;
    }
    if (mnemonic == "NOBASE") {
        return ObjectDirectiveKind::NoBase;
    }
    if (mnemonic == "EXTDEF") {
        return ObjectDirectiveKind::ExtDef;
    }
    if (mnemonic == "EXTREF") {
        return ObjectDirectiveKind::ExtRef;
    }
    if (mnemonic == "BYTE") {
        return ObjectDirectiveKind::Byte;
    }
    if (mnemonic == "WORD") {
        return ObjectDirectiveKind::Word;
    }
    if (mnemonic == "RESB") {
        return ObjectDirectiveKind::Resb;
    }
    if (mnemonic == "RESW") {
        return ObjectDirectiveKind::Resw;
    }
    return std::nullopt;
}

std::optional<LocatedDirectiveKind> toLocatedDirectiveKind(ObjectDirectiveKind directive) {
    switch (directive) {
        case ObjectDirectiveKind::Start:
            return LocatedDirectiveKind::Start;
        case ObjectDirectiveKind::End:
            return LocatedDirectiveKind::End;
        case ObjectDirectiveKind::Base:
            return LocatedDirectiveKind::Base;
        case ObjectDirectiveKind::NoBase:
            return LocatedDirectiveKind::NoBase;
        case ObjectDirectiveKind::Byte:
            return LocatedDirectiveKind::Byte;
        case ObjectDirectiveKind::Word:
            return LocatedDirectiveKind::Word;
        case ObjectDirectiveKind::Resb:
            return LocatedDirectiveKind::Resb;
        case ObjectDirectiveKind::Resw:
            return LocatedDirectiveKind::Resw;
        case ObjectDirectiveKind::ExtDef:
        case ObjectDirectiveKind::ExtRef:
            return std::nullopt;
    }
    return std::nullopt;
}

std::vector<SourceField> splitCommaSeparatedFields(const SourceField& operand) {
    std::vector<SourceField> items;
    std::size_t segStart = 0;
    for (std::size_t index = 0; index <= operand.text.size(); ++index) {
        if (index == operand.text.size() || operand.text[index] == ',') {
            std::size_t begin = segStart;
            while (begin < index && isHorizontalSpace(operand.text[begin])) {
                ++begin;
            }
            std::size_t end = index;
            while (end > begin && isHorizontalSpace(operand.text[end - 1])) {
                --end;
            }
            const std::size_t columnOffset = (begin < index) ? begin : segStart;
            items.push_back(SourceField{
                operand.text.substr(begin, end - begin),
                SourcePosition{operand.position.line, operand.position.column + columnOffset},
            });
            segStart = index + 1;
        }
    }
    return items;
}

struct PendingExport {
    std::string name;
    std::string source;
    SourcePosition declaration;
    std::size_t lineIndex{};
    bool conflicted{false};
};

struct PendingInstructionSymbol {
    std::size_t lineIndex{};
    LocatedInstructionFormat format{LocatedInstructionFormat::Format3};
    InstructionAddressing mode{InstructionAddressing::Simple};
    bool indexed{false};
    SymbolOperand symbol;
};

struct PendingBaseSymbol {
    std::size_t lineIndex{};
    SymbolOperand symbol;
};

struct PendingWordExpression {
    std::size_t lineIndex{};
    ParsedObjectWordExpression expression;
};

enum class PendingResolutionKind {
    Instruction,
    Base,
    Word,
};

struct PendingResolutionEntry {
    PendingResolutionKind kind{};
    std::size_t index{};
};

std::optional<ObjectPassOneDiagnostic>
defineLocalSymbol(SymbolTable& symbols,
                  const SourceIdentifier& label,
                  std::uint32_t address,
                  const std::vector<ExternalSymbol>& externalReferences,
                  const std::unordered_map<std::string, std::size_t>& externalIndexByName) {
    const std::string_view rawName = label.source.text.empty()
                                         ? std::string_view{label.key}
                                         : std::string_view{label.source.text};
    if (!SymbolTable::isValidIdentifier(rawName)) {
        return ObjectPassOneDiagnostic{
            ObjectPassOneDiagnosticCode::InvalidLabel,
            label.source.position,
            "Rotulo '" + std::string{rawName} +
                "' invalido; use uma letra seguida de letras, digitos ou '_'.",
            std::nullopt,
            std::nullopt,
        };
    }
    if (isObjectReservedName(rawName)) {
        return ObjectPassOneDiagnostic{
            ObjectPassOneDiagnosticCode::InvalidLabel,
            label.source.position,
            "Rotulo '" + std::string{rawName} + "' colide com mnemonico ou diretiva reservada.",
            std::nullopt,
            std::nullopt,
        };
    }
    const std::string normalized = SymbolTable::normalizeName(rawName);
    if (const auto extIt = externalIndexByName.find(normalized);
        extIt != externalIndexByName.end()) {
        const auto& ext = externalReferences[extIt->second];
        return ObjectPassOneDiagnostic{
            ObjectPassOneDiagnosticCode::LocalExternalConflict,
            label.source.position,
            "Rotulo local '" + std::string{rawName} + "' conflita com simbolo externo '" +
                ext.source + "' declarado em EXTREF.",
            ext.declaration,
            std::nullopt,
        };
    }
    if (const auto diagnostic = symbols.define(label, address)) {
        if (diagnostic->code == SymbolDiagnosticCode::DuplicateSymbol) {
            return ObjectPassOneDiagnostic{
                ObjectPassOneDiagnosticCode::DuplicateSymbol,
                diagnostic->position,
                diagnostic->message,
                diagnostic->previousDefinition,
                std::nullopt,
            };
        }
        return ObjectPassOneDiagnostic{
            ObjectPassOneDiagnosticCode::InvalidLabel,
            diagnostic->position,
            diagnostic->message,
            std::nullopt,
            std::nullopt,
        };
    }
    return std::nullopt;
}

} // namespace

AssemblyDiagnostic toAssemblyDiagnostic(const ObjectPassOneDiagnostic& diagnostic) {
    if (diagnostic.operandDiagnostic.has_value()) {
        return toAssemblyDiagnostic(*diagnostic.operandDiagnostic);
    }

    AssemblyDiagnosticCode code = AssemblyDiagnosticCode::InvalidOperation;
    switch (diagnostic.code) {
        case ObjectPassOneDiagnosticCode::InvalidSourceLine:
        case ObjectPassOneDiagnosticCode::MissingStart:
        case ObjectPassOneDiagnosticCode::DuplicateStart:
        case ObjectPassOneDiagnosticCode::MisplacedStart:
        case ObjectPassOneDiagnosticCode::MissingEnd:
        case ObjectPassOneDiagnosticCode::DuplicateEnd:
        case ObjectPassOneDiagnosticCode::StatementAfterEnd:
        case ObjectPassOneDiagnosticCode::UnknownOperation:
        case ObjectPassOneDiagnosticCode::ExtendedFormatNotAllowed:
            code = AssemblyDiagnosticCode::InvalidOperation;
            break;
        case ObjectPassOneDiagnosticCode::MissingSectionName:
        case ObjectPassOneDiagnosticCode::InvalidSectionName:
        case ObjectPassOneDiagnosticCode::SectionNameTooLong:
        case ObjectPassOneDiagnosticCode::InvalidLabel:
            code = AssemblyDiagnosticCode::InvalidLabel;
            break;
        case ObjectPassOneDiagnosticCode::SymbolNameTooLong:
            code = AssemblyDiagnosticCode::InvalidSymbolName;
            break;
        case ObjectPassOneDiagnosticCode::DuplicateSymbol:
        case ObjectPassOneDiagnosticCode::DuplicateExternalDeclaration:
        case ObjectPassOneDiagnosticCode::ExternalDeclarationConflict:
        case ObjectPassOneDiagnosticCode::LocalExternalConflict:
            code = AssemblyDiagnosticCode::DuplicateSymbol;
            break;
        case ObjectPassOneDiagnosticCode::UndefinedSymbol:
        case ObjectPassOneDiagnosticCode::UndefinedExportedSymbol:
            code = AssemblyDiagnosticCode::UndefinedSymbol;
            break;
        case ObjectPassOneDiagnosticCode::ExternalInFormat3:
        case ObjectPassOneDiagnosticCode::ExternalInBase:
        case ObjectPassOneDiagnosticCode::ExternalInEnd:
            code = AssemblyDiagnosticCode::SymbolNotAllowed;
            break;
        case ObjectPassOneDiagnosticCode::InvalidStartOperand:
        case ObjectPassOneDiagnosticCode::InvalidEndOperand:
        case ObjectPassOneDiagnosticCode::InvalidDirectiveOperand:
        case ObjectPassOneDiagnosticCode::InvalidInstructionOperand:
            code = AssemblyDiagnosticCode::UnexpectedField;
            break;
        case ObjectPassOneDiagnosticCode::AddressOutOfRange:
            code = AssemblyDiagnosticCode::ValueOutOfRange;
            break;
    }
    return AssemblyDiagnostic{code, diagnostic.position, diagnostic.message};
}

std::optional<AssemblySymbol> ObjectPassOneResult::findSymbol(std::string_view name) const {
    return symbolTable.find(name);
}

std::optional<ExportedSymbol> ObjectPassOneResult::findExport(std::string_view name) const {
    if (!SymbolTable::isValidIdentifier(name)) {
        return std::nullopt;
    }
    const std::string key = SymbolTable::normalizeName(name);
    for (const auto& exported : exports) {
        if (exported.name == key) {
            return exported;
        }
    }
    return std::nullopt;
}

std::optional<ExternalSymbol>
ObjectPassOneResult::findExternalReference(std::string_view name) const {
    if (!SymbolTable::isValidIdentifier(name)) {
        return std::nullopt;
    }
    const std::string key = SymbolTable::normalizeName(name);
    for (const auto& external : externalReferences) {
        if (external.name == key) {
            return external;
        }
    }
    return std::nullopt;
}

ObjectPassOneResult ObjectPassOne::run(std::string_view sourceText) {
    const auto parsed = SourceParser::parse(sourceText);
    return run(parsed);
}

ObjectPassOneResult ObjectPassOne::run(const SourceParseResult& parseResult) {
    if (!parseResult.succeeded()) {
        ObjectPassOneResult failed;
        failed.diagnostics.reserve(parseResult.diagnostics.size());
        for (const auto& diagnostic : parseResult.diagnostics) {
            failed.diagnostics.push_back(ObjectPassOneDiagnostic{
                ObjectPassOneDiagnosticCode::InvalidSourceLine,
                diagnostic.position,
                diagnostic.message,
                std::nullopt,
                std::nullopt,
            });
        }
        return failed;
    }
    return run(std::span<const SourceLine>{parseResult.lines});
}

ObjectPassOneResult ObjectPassOne::run(std::span<const SourceLine> parsedLines) {
    ObjectPassOneResult result;
    for (const auto& line : parsedLines) {
        if (line.kind == SourceLineKind::Invalid ||
            (line.kind == SourceLineKind::Statement && !line.statement.has_value())) {
            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                ObjectPassOneDiagnosticCode::InvalidSourceLine,
                SourcePosition{line.number, 1},
                "Linha invalida nao pode ser consumida pela passagem 1.",
                std::nullopt,
                std::nullopt,
            });
        }
    }
    if (!result.diagnostics.empty()) {
        return result;
    }

    SymbolTable& symbols = result.symbolTable;
    std::uint64_t startAddress = 0;
    std::uint64_t locationCounter = 0;
    std::uint64_t initialPc = 0;
    bool hasSeenActiveStatement = false;
    bool hasSeenStart = false;
    bool hasSeenMisplacedStart = false;
    bool hasSeenEnd = false;
    bool layoutValid = true;
    std::optional<SourcePosition> firstActivePosition;
    std::optional<ObjectSectionHeader> sectionHeader;
    std::optional<SymbolOperand> endEntrySymbol;

    std::vector<PendingExport> pendingExports;
    std::unordered_map<std::string, std::size_t> exportIndexByName;
    std::vector<ExternalSymbol> externalReferences;
    std::unordered_map<std::string, std::size_t> externalIndexByName;

    std::vector<PendingInstructionSymbol> pendingInstructions;
    std::vector<PendingBaseSymbol> pendingBases;
    std::vector<PendingWordExpression> pendingWords;
    std::vector<PendingResolutionEntry> pendingOrder;

    for (const auto& line : parsedLines) {
        if (line.kind == SourceLineKind::Blank || line.kind == SourceLineKind::Comment) {
            result.lines.push_back(ObjectLocatedLine{
                LocatedLine{
                    line,
                    LocatedLineCategory::Comment,
                    static_cast<std::uint32_t>(locationCounter),
                    0,
                    {},
                    false,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                },
                0,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                {},
                {},
            });
            continue;
        }

        const ParsedStatement& statement = *line.statement;
        const std::string_view operationKey = statement.operation.key;
        const bool extended = !operationKey.empty() && operationKey.front() == '+';
        const std::string_view mnemonic = extended ? operationKey.substr(1) : operationKey;
        const SourcePosition operationPosition = statement.operation.source.position;

        if (!firstActivePosition.has_value()) {
            firstActivePosition = operationPosition;
        }

        if (hasSeenEnd) {
            if (!extended && mnemonic == "END") {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::DuplicateEnd,
                    operationPosition,
                    "Diretiva END duplicada.",
                    std::nullopt,
                    std::nullopt,
                });
            } else if (!extended && mnemonic == "START" && hasSeenStart) {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::DuplicateStart,
                    operationPosition,
                    "Diretiva START duplicada.",
                    std::nullopt,
                    std::nullopt,
                });
            } else {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::StatementAfterEnd,
                    operationPosition,
                    "Declaracao ativa apos a diretiva END.",
                    std::nullopt,
                    std::nullopt,
                });
            }
            continue;
        }

        if (const auto directive = findObjectDirectiveKind(mnemonic)) {
            if (extended) {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::ExtendedFormatNotAllowed,
                    operationPosition,
                    "Diretiva '" + std::string{mnemonic} + "' nao aceita prefixo '+'.",
                    std::nullopt,
                    std::nullopt,
                });
                hasSeenActiveStatement = true;
                layoutValid = false;
                continue;
            }

            if (*directive == ObjectDirectiveKind::Start) {
                if (hasSeenStart) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::DuplicateStart,
                        operationPosition,
                        "Diretiva START duplicada.",
                        std::nullopt,
                        std::nullopt,
                    });
                    continue;
                }
                if (hasSeenActiveStatement) {
                    hasSeenMisplacedStart = true;
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::MisplacedStart,
                        operationPosition,
                        "Diretiva START permitida apenas na primeira linha ativa.",
                        std::nullopt,
                        std::nullopt,
                    });
                    continue;
                }

                hasSeenStart = true;
                hasSeenActiveStatement = true;

                bool sectionLabelValid = false;
                if (!statement.label.has_value()) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::MissingSectionName,
                        operationPosition,
                        "Diretiva START exige um rotulo com o nome da secao no modo objeto.",
                        std::nullopt,
                        std::nullopt,
                    });
                } else {
                    const SourceIdentifier& secLabel = *statement.label;
                    const std::string_view rawSec = secLabel.source.text.empty()
                                                        ? std::string_view{secLabel.key}
                                                        : std::string_view{secLabel.source.text};
                    if (!SymbolTable::isValidIdentifier(rawSec) || isObjectReservedName(rawSec)) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidSectionName,
                            secLabel.source.position,
                            "Nome de secao '" + std::string{rawSec} + "' invalido ou reservado.",
                            std::nullopt,
                            std::nullopt,
                        });
                    } else if (rawSec.size() > MaxObjectSymbolLength) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::SectionNameTooLong,
                            secLabel.source.position,
                            "Nome de secao '" + std::string{rawSec} +
                                "' excede o limite de 6 caracteres.",
                            std::nullopt,
                            std::nullopt,
                        });
                    } else {
                        sectionLabelValid = true;
                    }
                }

                if (!statement.operand.has_value()) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidStartOperand,
                        operationPosition,
                        "Diretiva START exige um endereco inicial.",
                        std::nullopt,
                        std::nullopt,
                    });
                    layoutValid = false;
                    continue;
                }

                const SourceField trimmedStart = trimSourceField(*statement.operand);
                const auto parsedStart = parseKnownInteger(trimmedStart, Address20BitRange);
                if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedStart)) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidStartOperand,
                        diagnostic->position,
                        diagnostic->message,
                        std::nullopt,
                        *diagnostic,
                    });
                    layoutValid = false;
                    continue;
                }

                const auto resolvedStart =
                    static_cast<std::uint64_t>(std::get<std::int64_t>(parsedStart));
                startAddress = resolvedStart;
                locationCounter = resolvedStart;
                initialPc = resolvedStart;

                if (sectionLabelValid && statement.label.has_value()) {
                    const auto startAddr32 = static_cast<std::uint32_t>(startAddress);
                    if (const auto diag = defineLocalSymbol(symbols,
                                                            *statement.label,
                                                            startAddr32,
                                                            externalReferences,
                                                            externalIndexByName)) {
                        result.diagnostics.push_back(*diag);
                    } else {
                        const std::string secKey =
                            SymbolTable::normalizeName(statement.label->source.text);
                        sectionHeader = ObjectSectionHeader{
                            secKey,
                            statement.label->source.text,
                            statement.label->source.position,
                            startAddr32,
                            0,
                        };
                    }
                }

                result.lines.push_back(ObjectLocatedLine{
                    LocatedLine{
                        line,
                        LocatedLineCategory::Directive,
                        static_cast<std::uint32_t>(startAddress),
                        0,
                        std::string{mnemonic},
                        false,
                        std::nullopt,
                        std::nullopt,
                        LocatedDirectiveKind::Start,
                    },
                    0,
                    ObjectDirectiveKind::Start,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                });
                continue;
            }

            hasSeenActiveStatement = true;

            if (*directive == ObjectDirectiveKind::ExtDef ||
                *directive == ObjectDirectiveKind::ExtRef) {
                if (!layoutValid) {
                    continue;
                }
                if (statement.label.has_value()) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidLabel,
                        statement.label->source.position,
                        "Diretiva " + std::string{mnemonic} + " nao aceita rotulo.",
                        std::nullopt,
                        std::nullopt,
                    });
                }

                const std::size_t currentLineIndex = result.lines.size();
                std::vector<ExternalSymbol> lineExternals;

                if (!statement.operand.has_value()) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                        operationPosition,
                        "Diretiva " + std::string{mnemonic} + " exige ao menos um simbolo.",
                        std::nullopt,
                        OperandDiagnostic{
                            OperandDiagnosticCode::EmptyOperand,
                            {},
                            operationPosition,
                            "Diretiva " + std::string{mnemonic} + " exige ao menos um simbolo.",
                        },
                    });
                } else {
                    const auto items = splitCommaSeparatedFields(*statement.operand);
                    for (const auto& item : items) {
                        if (item.text.empty()) {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                                item.position,
                                "Simbolo vazio na lista de " + std::string{mnemonic} + ".",
                                std::nullopt,
                                OperandDiagnostic{
                                    OperandDiagnosticCode::EmptyOperand,
                                    {},
                                    item.position,
                                    "Simbolo vazio na lista de " + std::string{mnemonic} + ".",
                                },
                            });
                            continue;
                        }
                        if (!SymbolTable::isValidIdentifier(item.text)) {
                            const OperandDiagnostic opDiag{
                                OperandDiagnosticCode::InvalidSymbolName,
                                item.text,
                                item.position,
                                "Simbolo '" + item.text + "' invalido em " + std::string{mnemonic} +
                                    ".",
                            };
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                                item.position,
                                opDiag.message,
                                std::nullopt,
                                opDiag,
                            });
                            continue;
                        }
                        if (isObjectReservedName(item.text)) {
                            const OperandDiagnostic opDiag{
                                OperandDiagnosticCode::ReservedSymbolName,
                                item.text,
                                item.position,
                                "Simbolo '" + item.text +
                                    "' colide com mnemonico ou diretiva reservada.",
                            };
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                                item.position,
                                opDiag.message,
                                std::nullopt,
                                opDiag,
                            });
                            continue;
                        }
                        if (item.text.size() > MaxObjectSymbolLength) {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::SymbolNameTooLong,
                                item.position,
                                "Simbolo global '" + item.text +
                                    "' excede o limite de 6 caracteres.",
                                std::nullopt,
                                std::nullopt,
                            });
                            continue;
                        }

                        std::string key = SymbolTable::normalizeName(item.text);
                        if (*directive == ObjectDirectiveKind::ExtDef) {
                            if (const auto dupIt = exportIndexByName.find(key);
                                dupIt != exportIndexByName.end()) {
                                const auto& prev = pendingExports[dupIt->second];
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::DuplicateExternalDeclaration,
                                    item.position,
                                    "Simbolo '" + item.text + "' repetido em EXTDEF.",
                                    prev.declaration,
                                    std::nullopt,
                                });
                                continue;
                            }
                            if (const auto extIt = externalIndexByName.find(key);
                                extIt != externalIndexByName.end()) {
                                const auto& prev = externalReferences[extIt->second];
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::ExternalDeclarationConflict,
                                    item.position,
                                    "Simbolo '" + item.text + "' conflita entre EXTDEF e EXTREF.",
                                    prev.declaration,
                                    std::nullopt,
                                });
                                continue;
                            }
                            if (sectionHeader.has_value() && key == sectionHeader->name) {
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::ExternalDeclarationConflict,
                                    item.position,
                                    "Simbolo '" + item.text +
                                        "' em EXTDEF conflita com o nome da secao.",
                                    sectionHeader->position,
                                    std::nullopt,
                                });
                                continue;
                            }
                            exportIndexByName.emplace(key, pendingExports.size());
                            pendingExports.push_back(PendingExport{
                                std::move(key),
                                item.text,
                                item.position,
                                currentLineIndex,
                                false,
                            });
                        } else {
                            if (const auto dupIt = externalIndexByName.find(key);
                                dupIt != externalIndexByName.end()) {
                                const auto& prev = externalReferences[dupIt->second];
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::DuplicateExternalDeclaration,
                                    item.position,
                                    "Simbolo '" + item.text + "' repetido em EXTREF.",
                                    prev.declaration,
                                    std::nullopt,
                                });
                                continue;
                            }
                            if (const auto expIt = exportIndexByName.find(key);
                                expIt != exportIndexByName.end()) {
                                auto& prev = pendingExports[expIt->second];
                                prev.conflicted = true;
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::ExternalDeclarationConflict,
                                    item.position,
                                    "Simbolo '" + item.text + "' conflita entre EXTDEF e EXTREF.",
                                    prev.declaration,
                                    std::nullopt,
                                });
                                continue;
                            }
                            if (const auto localSym = symbols.find(key)) {
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::LocalExternalConflict,
                                    item.position,
                                    "Simbolo externo '" + item.text +
                                        "' conflita com simbolo local ja definido.",
                                    localSym->definition,
                                    std::nullopt,
                                });
                                continue;
                            }
                            ExternalSymbol extSym{std::move(key), item.text, item.position};
                            externalIndexByName.emplace(extSym.name, externalReferences.size());
                            lineExternals.push_back(extSym);
                            externalReferences.push_back(std::move(extSym));
                        }
                    }
                }

                result.lines.push_back(ObjectLocatedLine{
                    LocatedLine{
                        line,
                        LocatedLineCategory::Directive,
                        static_cast<std::uint32_t>(locationCounter),
                        0,
                        std::string{mnemonic},
                        false,
                        std::nullopt,
                        std::nullopt,
                        std::nullopt,
                    },
                    0,
                    *directive,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    {},
                    std::move(lineExternals),
                });
                continue;
            }

            if (*directive == ObjectDirectiveKind::End) {
                hasSeenEnd = true;

                if (statement.label.has_value() && layoutValid) {
                    if (locationCounter > MaxMemoryAddress) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::AddressOutOfRange,
                            statement.label->source.position,
                            "Contador de localizacao excede 0xFFFFF no rotulo de END.",
                            std::nullopt,
                            std::nullopt,
                        });
                        layoutValid = false;
                    } else {
                        const auto endAddr32 = static_cast<std::uint32_t>(locationCounter);
                        if (const auto diag = defineLocalSymbol(symbols,
                                                                *statement.label,
                                                                endAddr32,
                                                                externalReferences,
                                                                externalIndexByName)) {
                            result.diagnostics.push_back(*diag);
                        }
                    }
                }

                if (statement.operand.has_value()) {
                    const SourceField trimmedEnd = trimSourceField(*statement.operand);
                    const auto parsedEnd = parseValue(trimmedEnd);
                    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedEnd)) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidEndOperand,
                            diagnostic->position,
                            diagnostic->message,
                            std::nullopt,
                            *diagnostic,
                        });
                    } else if (isObjectReservedName(trimmedEnd.text)) {
                        const OperandDiagnostic reservedDiag{
                            OperandDiagnosticCode::ReservedSymbolName,
                            trimmedEnd.text,
                            trimmedEnd.position,
                            "Simbolo '" + trimmedEnd.text +
                                "' colide com mnemonico ou diretiva reservada.",
                        };
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidEndOperand,
                            trimmedEnd.position,
                            reservedDiag.message,
                            std::nullopt,
                            reservedDiag,
                        });
                    } else if (const auto* symbol =
                                   std::get_if<SymbolOperand>(&std::get<OperandValue>(parsedEnd))) {
                        endEntrySymbol = *symbol;
                    } else {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidEndOperand,
                            trimmedEnd.position,
                            "Diretiva END aceita apenas um rotulo de entrada local valido.",
                            std::nullopt,
                            std::nullopt,
                        });
                    }
                }

                result.lines.push_back(ObjectLocatedLine{
                    LocatedLine{
                        line,
                        LocatedLineCategory::Directive,
                        static_cast<std::uint32_t>(locationCounter),
                        0,
                        std::string{mnemonic},
                        false,
                        std::nullopt,
                        std::nullopt,
                        LocatedDirectiveKind::End,
                    },
                    0,
                    ObjectDirectiveKind::End,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                });
                continue;
            }

            if (*directive == ObjectDirectiveKind::Base ||
                *directive == ObjectDirectiveKind::NoBase) {
                if (!layoutValid) {
                    continue;
                }
                if (statement.label.has_value()) {
                    if (locationCounter > MaxMemoryAddress) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::AddressOutOfRange,
                            statement.label->source.position,
                            "Contador de localizacao fora de 0x00000..0xFFFFF.",
                            std::nullopt,
                            std::nullopt,
                        });
                        layoutValid = false;
                        continue;
                    }
                    const auto baseAddr32 = static_cast<std::uint32_t>(locationCounter);
                    if (const auto diag = defineLocalSymbol(symbols,
                                                            *statement.label,
                                                            baseAddr32,
                                                            externalReferences,
                                                            externalIndexByName)) {
                        result.diagnostics.push_back(*diag);
                    }
                }

                const std::size_t currentLineIndex = result.lines.size();

                if (*directive == ObjectDirectiveKind::NoBase) {
                    if (statement.operand.has_value()) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                            statement.operand->position,
                            "Diretiva NOBASE nao aceita operando.",
                            std::nullopt,
                            std::nullopt,
                        });
                    }
                } else {
                    if (!statement.operand.has_value()) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                            operationPosition,
                            "Diretiva BASE exige operando.",
                            std::nullopt,
                            std::nullopt,
                        });
                    } else {
                        const SourceField trimmedBase = trimSourceField(*statement.operand);
                        const auto parsedBase = parseValue(trimmedBase);
                        if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedBase)) {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                                diagnostic->position,
                                diagnostic->message,
                                std::nullopt,
                                *diagnostic,
                            });
                        } else if (isObjectReservedName(trimmedBase.text)) {
                            const OperandDiagnostic reservedDiag{
                                OperandDiagnosticCode::ReservedSymbolName,
                                trimmedBase.text,
                                trimmedBase.position,
                                "Simbolo '" + trimmedBase.text +
                                    "' colide com mnemonico ou diretiva reservada.",
                            };
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                                trimmedBase.position,
                                reservedDiag.message,
                                std::nullopt,
                                reservedDiag,
                            });
                        } else {
                            const auto& baseValue = std::get<OperandValue>(parsedBase);
                            if (const auto* symbol = std::get_if<SymbolOperand>(&baseValue)) {
                                pendingOrder.push_back(PendingResolutionEntry{
                                    PendingResolutionKind::Base, pendingBases.size()});
                                pendingBases.push_back(
                                    PendingBaseSymbol{currentLineIndex, *symbol});
                            } else {
                                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                    ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                                    trimmedBase.position,
                                    "Diretiva BASE exige um simbolo local no modo objeto.",
                                    std::nullopt,
                                    std::nullopt,
                                });
                            }
                        }
                    }
                }

                result.lines.push_back(ObjectLocatedLine{
                    LocatedLine{
                        line,
                        LocatedLineCategory::Directive,
                        static_cast<std::uint32_t>(locationCounter),
                        0,
                        std::string{mnemonic},
                        false,
                        std::nullopt,
                        std::nullopt,
                        toLocatedDirectiveKind(*directive),
                    },
                    0,
                    *directive,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                });
                continue;
            }

            if (!layoutValid) {
                continue;
            }

            if (*directive == ObjectDirectiveKind::Word) {
                if (!statement.operand.has_value()) {
                    const OperandDiagnostic emptyDiag{
                        OperandDiagnosticCode::EmptyOperand,
                        {},
                        operationPosition,
                        "Diretiva WORD exige operando.",
                    };
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                        operationPosition,
                        emptyDiag.message,
                        std::nullopt,
                        emptyDiag,
                    });
                    layoutValid = false;
                    continue;
                }

                const auto parsedWord = parseObjectWordExpression(*statement.operand);
                if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedWord)) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                        diagnostic->position,
                        diagnostic->message,
                        std::nullopt,
                        *diagnostic,
                    });
                    layoutValid = false;
                    continue;
                }

                constexpr std::size_t WordSizeBytes = 3;
                const auto nextLocationCounter = locationCounter + WordSizeBytes;
                if (locationCounter > MaxMemoryAddress || nextLocationCounter > MemoryCapacity) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::AddressOutOfRange,
                        operationPosition,
                        "Dados/reserva excedem o espaco enderecavel 0x00000..0xFFFFF.",
                        std::nullopt,
                        std::nullopt,
                    });
                    layoutValid = false;
                    continue;
                }

                if (statement.label.has_value()) {
                    if (const auto diag =
                            defineLocalSymbol(symbols,
                                              *statement.label,
                                              static_cast<std::uint32_t>(locationCounter),
                                              externalReferences,
                                              externalIndexByName)) {
                        result.diagnostics.push_back(*diag);
                    }
                }

                const std::size_t currentLineIndex = result.lines.size();
                pendingOrder.push_back(
                    PendingResolutionEntry{PendingResolutionKind::Word, pendingWords.size()});
                pendingWords.push_back(PendingWordExpression{
                    currentLineIndex,
                    std::get<ParsedObjectWordExpression>(parsedWord),
                });

                result.lines.push_back(ObjectLocatedLine{
                    LocatedLine{
                        line,
                        LocatedLineCategory::Data,
                        static_cast<std::uint32_t>(locationCounter),
                        WordSizeBytes,
                        std::string{mnemonic},
                        false,
                        std::nullopt,
                        std::nullopt,
                        LocatedDirectiveKind::Word,
                    },
                    0,
                    ObjectDirectiveKind::Word,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                });
                locationCounter = nextLocationCounter;
                continue;
            }

            const auto measured = measureData(line, symbols);
            if (const auto* diagnostic = std::get_if<DataDiagnostic>(&measured)) {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::InvalidDirectiveOperand,
                    diagnostic->position,
                    diagnostic->message,
                    std::nullopt,
                    diagnostic->operandDiagnostic,
                });
                layoutValid = false;
                continue;
            }
            const auto& data = std::get<DataMeasurement>(measured);
            const auto nextLocationCounter = locationCounter + data.sizeBytes;
            if (locationCounter > MaxMemoryAddress || nextLocationCounter > MemoryCapacity) {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::AddressOutOfRange,
                    operationPosition,
                    "Dados/reserva excedem o espaco enderecavel 0x00000..0xFFFFF.",
                    std::nullopt,
                    std::nullopt,
                });
                layoutValid = false;
                continue;
            }
            if (statement.label.has_value()) {
                if (const auto diag = defineLocalSymbol(symbols,
                                                        *statement.label,
                                                        static_cast<std::uint32_t>(locationCounter),
                                                        externalReferences,
                                                        externalIndexByName)) {
                    result.diagnostics.push_back(*diag);
                }
            }

            LocatedLine locatedDataLine{
                line,
                data.reservation ? LocatedLineCategory::Reservation : LocatedLineCategory::Data,
                static_cast<std::uint32_t>(locationCounter),
                data.sizeBytes,
                std::string{mnemonic},
                false,
                std::nullopt,
                std::nullopt,
                toLocatedDirectiveKind(*directive),
            };

            std::optional<std::vector<std::uint8_t>> byteData;
            if (*directive == ObjectDirectiveKind::Byte) {
                const auto encoded = encodeData(locatedDataLine, symbols);
                if (const auto* encodedData = std::get_if<EncodedData>(&encoded)) {
                    if (const auto* bytes = std::get_if<DataBytes>(encodedData)) {
                        byteData = bytes->bytes;
                    }
                }
            }

            result.lines.push_back(ObjectLocatedLine{
                std::move(locatedDataLine),
                0,
                *directive,
                std::nullopt,
                std::nullopt,
                std::move(byteData),
                std::nullopt,
                {},
                {},
            });
            locationCounter = nextLocationCounter;
            continue;
        }

        hasSeenActiveStatement = true;

        const auto lookup = findInstruction(statement.operation);
        if (const auto* lookupError = std::get_if<InstructionDiagnostic>(&lookup)) {
            const ObjectPassOneDiagnosticCode code =
                lookupError->code == InstructionDiagnosticCode::InvalidExtendedFormat
                    ? ObjectPassOneDiagnosticCode::ExtendedFormatNotAllowed
                    : ObjectPassOneDiagnosticCode::UnknownOperation;
            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                code,
                lookupError->source.position,
                lookupError->message,
                std::nullopt,
                std::nullopt,
            });
            layoutValid = false;
            continue;
        }

        const InstructionSpec& spec =
            std::get<std::reference_wrapper<const InstructionSpec>>(lookup).get();

        LocatedInstructionFormat format = LocatedInstructionFormat::Format3;
        std::size_t instructionSize = spec.size();
        if (spec.formats == InstructionFormats::Format2) {
            format = LocatedInstructionFormat::Format2;
            instructionSize = 2;
        } else if (extended) {
            format = LocatedInstructionFormat::Format4;
            instructionSize = 4;
        }

        if (!layoutValid) {
            continue;
        }

        const std::uint64_t nextLocationCounter = locationCounter + instructionSize;
        if (locationCounter > MaxMemoryAddress || nextLocationCounter > MemoryCapacity) {
            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                ObjectPassOneDiagnosticCode::AddressOutOfRange,
                operationPosition,
                "Instrucao excede o espaco enderecavel 0x00000..0xFFFFF.",
                std::nullopt,
                std::nullopt,
            });
            layoutValid = false;
            continue;
        }

        if (statement.label.has_value()) {
            const auto instrAddr32 = static_cast<std::uint32_t>(locationCounter);
            if (const auto diag = defineLocalSymbol(symbols,
                                                    *statement.label,
                                                    instrAddr32,
                                                    externalReferences,
                                                    externalIndexByName)) {
                result.diagnostics.push_back(*diag);
            }
        }

        std::optional<ObjectMemoryOperand> resolvedMemoryOperand;
        const std::size_t currentLineIndex = result.lines.size();

        const auto parsedOperand = parseInstructionOperand(spec, line);
        if (const auto* diagnostic = std::get_if<InstructionDiagnostic>(&parsedOperand)) {
            const SourcePosition position =
                diagnostic->code == InstructionDiagnosticCode::MissingOperand
                    ? statement.operation.source.position
                    : diagnostic->source.position;
            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                ObjectPassOneDiagnosticCode::InvalidInstructionOperand,
                position,
                diagnostic->message,
                std::nullopt,
                std::nullopt,
            });
        } else {
            const auto& operand = std::get<InstructionOperand>(parsedOperand);
            if (const auto* memory = std::get_if<MemoryInstructionOperand>(&operand.data)) {
                const auto parsedValue = parseValue(memory->value);
                if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedValue)) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidInstructionOperand,
                        diagnostic->position,
                        diagnostic->message,
                        std::nullopt,
                        *diagnostic,
                    });
                } else if (isObjectReservedName(memory->value.text)) {
                    const OperandDiagnostic reservedDiag{
                        OperandDiagnosticCode::ReservedSymbolName,
                        memory->value.text,
                        memory->value.position,
                        "Simbolo '" + memory->value.text +
                            "' colide com mnemonico ou diretiva reservada.",
                    };
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::InvalidInstructionOperand,
                        memory->value.position,
                        reservedDiag.message,
                        std::nullopt,
                        reservedDiag,
                    });
                } else {
                    const auto& value = std::get<OperandValue>(parsedValue);
                    if (const auto* integer = std::get_if<IntegerOperand>(&value)) {
                        const auto validated = validateInteger(value, Address20BitRange);
                        if (const auto* diag = std::get_if<OperandDiagnostic>(&validated)) {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::InvalidInstructionOperand,
                                diag->position,
                                diag->message,
                                std::nullopt,
                                *diag,
                            });
                        } else {
                            resolvedMemoryOperand = ObjectMemoryOperand{
                                memory->mode,
                                memory->indexed,
                                ClassifiedOperandValue{*integer},
                            };
                        }
                    } else if (const auto* symbol = std::get_if<SymbolOperand>(&value)) {
                        pendingOrder.push_back(PendingResolutionEntry{
                            PendingResolutionKind::Instruction, pendingInstructions.size()});
                        pendingInstructions.push_back(PendingInstructionSymbol{
                            currentLineIndex,
                            format,
                            memory->mode,
                            memory->indexed,
                            *symbol,
                        });
                    }
                }
            }
        }

        result.lines.push_back(ObjectLocatedLine{
            LocatedLine{
                line,
                LocatedLineCategory::Instruction,
                static_cast<std::uint32_t>(locationCounter),
                instructionSize,
                std::string{spec.mnemonic},
                extended,
                spec.opcode,
                format,
                std::nullopt,
            },
            0,
            std::nullopt,
            std::move(resolvedMemoryOperand),
            std::nullopt,
            std::nullopt,
            std::nullopt,
            {},
            {},
        });
        locationCounter = nextLocationCounter;
    }

    if (!hasSeenStart && !hasSeenMisplacedStart) {
        result.diagnostics.push_back(ObjectPassOneDiagnostic{
            ObjectPassOneDiagnosticCode::MissingStart,
            firstActivePosition.value_or(SourcePosition{1U, 1U}),
            "Diretiva START com nome de secao e obrigatoria no modo objeto.",
            std::nullopt,
            std::nullopt,
        });
    }

    if (!hasSeenEnd) {
        const std::size_t fallbackLine = parsedLines.empty() ? 1U : parsedLines.back().number;
        result.diagnostics.push_back(ObjectPassOneDiagnostic{
            ObjectPassOneDiagnosticCode::MissingEnd,
            SourcePosition{fallbackLine, 1U},
            "Diretiva obrigatoria END ausente.",
            std::nullopt,
            std::nullopt,
        });
    }

    const auto startAddr32 = static_cast<std::uint32_t>(startAddress);
    const auto programLength = static_cast<std::uint32_t>(
        locationCounter >= startAddress ? locationCounter - startAddress : 0ULL);
    const auto computeOffset = [startAddr32](std::uint32_t addr) noexcept -> std::uint32_t {
        return addr >= startAddr32 ? (addr - startAddr32) : 0U;
    };

    if (layoutValid) {
        for (const auto& exp : pendingExports) {
            if (exp.conflicted) {
                continue;
            }
            if (const auto resolved = symbols.find(exp.name)) {
                const auto offset = computeOffset(resolved->address);
                if (offset >= programLength) {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::AddressOutOfRange,
                        exp.declaration,
                        "Simbolo exportado '" + exp.source + "' aponta para fora da secao.",
                        std::nullopt,
                        std::nullopt,
                    });
                    continue;
                }
                ExportedSymbol exported{
                    exp.name,
                    exp.source,
                    exp.declaration,
                    resolved->address,
                    offset,
                    resolved->definition,
                };
                if (exp.lineIndex < result.lines.size()) {
                    result.lines[exp.lineIndex].exportedSymbols.push_back(exported);
                }
                result.exports.push_back(std::move(exported));
            } else {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::UndefinedExportedSymbol,
                    exp.declaration,
                    "Simbolo exportado '" + exp.source + "' em EXTDEF nao foi definido localmente.",
                    std::nullopt,
                    std::nullopt,
                });
            }
        }

        for (const auto& entry : pendingOrder) {
            switch (entry.kind) {
                case PendingResolutionKind::Instruction: {
                    const auto& pending = pendingInstructions[entry.index];
                    if (const auto extIt = externalIndexByName.find(pending.symbol.name);
                        extIt != externalIndexByName.end()) {
                        if (pending.format != LocatedInstructionFormat::Format4) {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::ExternalInFormat3,
                                pending.symbol.position,
                                "Simbolo externo '" + pending.symbol.source +
                                    "' exige instrucao em formato 4 com prefixo '+'.",
                                std::nullopt,
                                std::nullopt,
                            });
                        } else {
                            const auto& ext = externalReferences[extIt->second];
                            result.lines[pending.lineIndex].memoryOperand = ObjectMemoryOperand{
                                pending.mode,
                                pending.indexed,
                                ClassifiedOperandValue{ExternalSymbolReference{
                                    ext.name,
                                    pending.symbol.source,
                                    ext.source,
                                    pending.symbol.position,
                                    ext.declaration,
                                }},
                            };
                        }
                    } else {
                        symbols.reference(pending.symbol.source, pending.symbol.position);
                        if (const auto resolved = symbols.find(pending.symbol.name)) {
                            result.lines[pending.lineIndex].memoryOperand = ObjectMemoryOperand{
                                pending.mode,
                                pending.indexed,
                                ClassifiedOperandValue{LocalSymbolReference{
                                    resolved->name,
                                    pending.symbol.source,
                                    pending.symbol.position,
                                    resolved->address,
                                    computeOffset(resolved->address),
                                    resolved->definition,
                                }},
                            };
                        } else {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::UndefinedSymbol,
                                pending.symbol.position,
                                "Simbolo '" + pending.symbol.source +
                                    "' nao foi definido nem declarado em EXTREF.",
                                std::nullopt,
                                std::nullopt,
                            });
                        }
                    }
                    break;
                }
                case PendingResolutionKind::Base: {
                    const auto& pending = pendingBases[entry.index];
                    if (externalIndexByName.contains(pending.symbol.name)) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::ExternalInBase,
                            pending.symbol.position,
                            "Diretiva BASE nao aceita simbolo externo '" + pending.symbol.source +
                                "'.",
                            std::nullopt,
                            std::nullopt,
                        });
                    } else {
                        symbols.reference(pending.symbol.source, pending.symbol.position);
                        if (const auto resolved = symbols.find(pending.symbol.name)) {
                            result.lines[pending.lineIndex].baseOperand = ObjectBaseOperand{
                                LocalSymbolReference{
                                    resolved->name,
                                    pending.symbol.source,
                                    pending.symbol.position,
                                    resolved->address,
                                    computeOffset(resolved->address),
                                    resolved->definition,
                                },
                            };
                        } else {
                            result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                ObjectPassOneDiagnosticCode::UndefinedSymbol,
                                pending.symbol.position,
                                "Simbolo '" + pending.symbol.source + "' nao foi definido.",
                                std::nullopt,
                                std::nullopt,
                            });
                        }
                    }
                    break;
                }
                case PendingResolutionKind::Word: {
                    const auto& pending = pendingWords[entry.index];
                    ObjectWordExpression classified;
                    classified.terms.reserve(pending.expression.terms.size());
                    bool wordValid = true;
                    for (const auto& term : pending.expression.terms) {
                        if (const auto* integer = std::get_if<IntegerOperand>(&term.value)) {
                            classified.terms.push_back(ObjectWordTerm{
                                term.sign,
                                term.position,
                                ClassifiedOperandValue{*integer},
                            });
                        } else if (const auto* symbol = std::get_if<SymbolOperand>(&term.value)) {
                            if (const auto extIt = externalIndexByName.find(symbol->name);
                                extIt != externalIndexByName.end()) {
                                const auto& ext = externalReferences[extIt->second];
                                classified.terms.push_back(ObjectWordTerm{
                                    term.sign,
                                    term.position,
                                    ClassifiedOperandValue{ExternalSymbolReference{
                                        ext.name,
                                        symbol->source,
                                        ext.source,
                                        symbol->position,
                                        ext.declaration,
                                    }},
                                });
                            } else {
                                symbols.reference(symbol->source, symbol->position);
                                if (const auto resolved = symbols.find(symbol->name)) {
                                    classified.terms.push_back(ObjectWordTerm{
                                        term.sign,
                                        term.position,
                                        ClassifiedOperandValue{LocalSymbolReference{
                                            resolved->name,
                                            symbol->source,
                                            symbol->position,
                                            resolved->address,
                                            computeOffset(resolved->address),
                                            resolved->definition,
                                        }},
                                    });
                                } else {
                                    wordValid = false;
                                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                                        ObjectPassOneDiagnosticCode::UndefinedSymbol,
                                        symbol->position,
                                        "Simbolo '" + symbol->source +
                                            "' nao foi definido nem declarado em EXTREF.",
                                        std::nullopt,
                                        std::nullopt,
                                    });
                                }
                            }
                        }
                    }
                    if (wordValid) {
                        result.lines[pending.lineIndex].wordExpression = std::move(classified);
                    }
                    break;
                }
            }
        }

        if (endEntrySymbol.has_value()) {
            if (externalIndexByName.contains(endEntrySymbol->name)) {
                result.diagnostics.push_back(ObjectPassOneDiagnostic{
                    ObjectPassOneDiagnosticCode::ExternalInEnd,
                    endEntrySymbol->position,
                    "Diretiva END nao aceita simbolo externo '" + endEntrySymbol->source + "'.",
                    std::nullopt,
                    std::nullopt,
                });
            } else {
                symbols.reference(endEntrySymbol->source, endEntrySymbol->position);
                if (const auto resolved = symbols.find(endEntrySymbol->name)) {
                    const auto offset = computeOffset(resolved->address);
                    if (offset >= programLength) {
                        result.diagnostics.push_back(ObjectPassOneDiagnostic{
                            ObjectPassOneDiagnosticCode::InvalidEndOperand,
                            endEntrySymbol->position,
                            "Endereco de entrada em END fora da secao.",
                            std::nullopt,
                            std::nullopt,
                        });
                    } else {
                        initialPc = resolved->address;
                        result.entry = ObjectEntryPoint{
                            resolved->name,
                            endEntrySymbol->source,
                            endEntrySymbol->position,
                            resolved->address,
                            offset,
                            resolved->definition,
                        };
                    }
                } else {
                    result.diagnostics.push_back(ObjectPassOneDiagnostic{
                        ObjectPassOneDiagnosticCode::UndefinedSymbol,
                        endEntrySymbol->position,
                        "Simbolo '" + endEntrySymbol->source + "' nao foi definido.",
                        std::nullopt,
                        std::nullopt,
                    });
                }
            }
        }
    }

    for (auto& locatedLine : result.lines) {
        locatedLine.offset = computeOffset(locatedLine.located.address);
    }

    result.startAddress = startAddr32;
    result.initialPc = static_cast<std::uint32_t>(initialPc);
    result.programLength = programLength;
    if (sectionHeader.has_value()) {
        sectionHeader->length = result.programLength;
        result.section = *sectionHeader;
    }
    result.externalReferences = std::move(externalReferences);
    result.symbols = symbols.symbols();
    return result;
}

} // namespace assembler
