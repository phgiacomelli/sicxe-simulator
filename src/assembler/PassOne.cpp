#include "PassOne.hpp"

#include "DataDirective.hpp"
#include "InstructionCatalog.hpp"
#include "InstructionOperand.hpp"
#include "OperandValue.hpp"
#include "SymbolTable.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

std::optional<LocatedDirectiveKind> findDirectiveKind(std::string_view mnemonic) {
    if (mnemonic == "START") {
        return LocatedDirectiveKind::Start;
    }
    if (mnemonic == "END") {
        return LocatedDirectiveKind::End;
    }
    if (mnemonic == "BASE") {
        return LocatedDirectiveKind::Base;
    }
    if (mnemonic == "NOBASE") {
        return LocatedDirectiveKind::NoBase;
    }
    if (mnemonic == "BYTE") {
        return LocatedDirectiveKind::Byte;
    }
    if (mnemonic == "WORD") {
        return LocatedDirectiveKind::Word;
    }
    if (mnemonic == "RESB") {
        return LocatedDirectiveKind::Resb;
    }
    if (mnemonic == "RESW") {
        return LocatedDirectiveKind::Resw;
    }
    return std::nullopt;
}

std::optional<PassOneDiagnostic>
defineSymbol(SymbolTable& symbols, const SourceIdentifier& label, std::uint32_t address) {
    if (const auto diagnostic = symbols.define(label, address)) {
        if (diagnostic->code == SymbolDiagnosticCode::DuplicateSymbol) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::DuplicateSymbol,
                diagnostic->position,
                diagnostic->message,
                diagnostic->previousDefinition,
            };
        }
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidLabel,
            diagnostic->position,
            diagnostic->message,
            std::nullopt,
        };
    }
    return std::nullopt;
}

std::optional<PassOneDiagnostic> validateAndReferenceBaseOperand(const SourceField& operandField,
                                                                 SymbolTable& symbols) {
    const SourceField trimmed = trimSourceField(operandField);
    const auto parsed = parseValue(trimmed);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsed)) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidDirectiveOperand,
            diagnostic->position,
            diagnostic->message,
            std::nullopt,
        };
    }
    const auto& value = std::get<OperandValue>(parsed);
    if (const auto refDiagnostic = referenceOperand(value, symbols, Address20BitRange)) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidDirectiveOperand,
            refDiagnostic->position,
            refDiagnostic->message,
            std::nullopt,
        };
    }
    return std::nullopt;
}

std::optional<PassOneDiagnostic> validateInstructionAndReference(const InstructionSpec& spec,
                                                                 const SourceLine& line,
                                                                 SymbolTable& symbols) {
    const auto parsedOperand = parseInstructionOperand(spec, line);
    if (const auto* diagnostic = std::get_if<InstructionDiagnostic>(&parsedOperand)) {
        const SourcePosition position =
            diagnostic->code == InstructionDiagnosticCode::MissingOperand
                ? line.statement->operation.source.position
                : diagnostic->source.position;
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidInstructionOperand,
            position,
            diagnostic->message,
            std::nullopt,
        };
    }

    const auto& operand = std::get<InstructionOperand>(parsedOperand);
    if (const auto* memory = std::get_if<MemoryInstructionOperand>(&operand.data)) {
        const auto parsedValue = parseValue(memory->value);
        if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedValue)) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidInstructionOperand,
                diagnostic->position,
                diagnostic->message,
                std::nullopt,
            };
        }
        const auto& value = std::get<OperandValue>(parsedValue);
        if (const auto refDiagnostic = referenceOperand(value, symbols, Address20BitRange)) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidInstructionOperand,
                refDiagnostic->position,
                refDiagnostic->message,
                std::nullopt,
            };
        }
    }
    return std::nullopt;
}

} // namespace

AssemblyDiagnostic toAssemblyDiagnostic(const PassOneDiagnostic& diagnostic) {
    AssemblyDiagnosticCode code = AssemblyDiagnosticCode::InvalidOperation;
    switch (diagnostic.code) {
        case PassOneDiagnosticCode::InvalidSourceLine:
            code = AssemblyDiagnosticCode::InvalidOperation;
            break;
        case PassOneDiagnosticCode::DuplicateSymbol:
            code = AssemblyDiagnosticCode::DuplicateSymbol;
            break;
        case PassOneDiagnosticCode::UndefinedSymbol:
            code = AssemblyDiagnosticCode::UndefinedSymbol;
            break;
        case PassOneDiagnosticCode::InvalidLabel:
            code = AssemblyDiagnosticCode::InvalidLabel;
            break;
        case PassOneDiagnosticCode::UnknownOperation:
        case PassOneDiagnosticCode::ExtendedFormatNotAllowed:
        case PassOneDiagnosticCode::DuplicateStart:
        case PassOneDiagnosticCode::MisplacedStart:
        case PassOneDiagnosticCode::MissingEnd:
        case PassOneDiagnosticCode::DuplicateEnd:
        case PassOneDiagnosticCode::StatementAfterEnd:
        case PassOneDiagnosticCode::DataDirectiveNotIntegrated:
            code = AssemblyDiagnosticCode::InvalidOperation;
            break;
        case PassOneDiagnosticCode::InvalidStartOperand:
        case PassOneDiagnosticCode::InvalidEndOperand:
        case PassOneDiagnosticCode::InvalidDirectiveOperand:
        case PassOneDiagnosticCode::InvalidInstructionOperand:
            code = AssemblyDiagnosticCode::UnexpectedField;
            break;
        case PassOneDiagnosticCode::AddressOutOfRange:
            code = AssemblyDiagnosticCode::ValueOutOfRange;
            break;
    }
    return AssemblyDiagnostic{code, diagnostic.position, diagnostic.message};
}

std::optional<AssemblySymbol> PassOneResult::findSymbol(std::string_view name) const {
    return symbolTable.find(name);
}

PassOneResult PassOne::run(const SourceParseResult& parseResult) {
    if (!parseResult.succeeded()) {
        PassOneResult failed;
        failed.diagnostics.reserve(parseResult.diagnostics.size());
        for (const auto& diagnostic : parseResult.diagnostics) {
            failed.diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidSourceLine,
                diagnostic.position,
                diagnostic.message,
                std::nullopt,
            });
        }
        return failed;
    }
    return run(std::span<const SourceLine>{parseResult.lines});
}

PassOneResult PassOne::run(std::span<const SourceLine> parsedLines) {
    PassOneResult result;
    for (const auto& line : parsedLines) {
        if (line.kind == SourceLineKind::Invalid ||
            (line.kind == SourceLineKind::Statement && !line.statement.has_value())) {
            result.diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidSourceLine,
                SourcePosition{line.number, 1},
                "Linha invalida nao pode ser consumida pela passagem 1.",
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
    bool hasSeenEnd = false;
    bool layoutValid = true;
    std::optional<SourceField> endEntrySymbol;

    for (const auto& line : parsedLines) {
        if (line.kind == SourceLineKind::Blank || line.kind == SourceLineKind::Comment) {
            const auto clampedAddress =
                static_cast<std::uint32_t>(std::min(locationCounter, MaxMemoryAddress));
            result.lines.push_back(LocatedLine{
                line,
                LocatedLineCategory::Comment,
                clampedAddress,
                0,
                {},
                false,
                std::nullopt,
                std::nullopt,
                std::nullopt,
            });
            continue;
        }

        const ParsedStatement& statement = *line.statement;
        const std::string_view operationKey = statement.operation.key;
        const bool extended = !operationKey.empty() && operationKey.front() == '+';
        const std::string_view mnemonic = extended ? operationKey.substr(1) : operationKey;
        const SourcePosition operationPosition = statement.operation.source.position;

        if (hasSeenEnd) {
            if (!extended && mnemonic == "END") {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::DuplicateEnd,
                    operationPosition,
                    "Diretiva END duplicada.",
                    std::nullopt,
                });
            } else if (!extended && mnemonic == "START" && hasSeenStart) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::DuplicateStart,
                    operationPosition,
                    "Diretiva START duplicada.",
                    std::nullopt,
                });
            } else {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::StatementAfterEnd,
                    operationPosition,
                    "Declaracao ativa apos a diretiva END.",
                    std::nullopt,
                });
            }
            continue;
        }

        if (const auto directive = findDirectiveKind(mnemonic)) {
            if (extended) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::ExtendedFormatNotAllowed,
                    operationPosition,
                    "Diretiva '" + std::string{mnemonic} + "' nao aceita prefixo '+'.",
                    std::nullopt,
                });
                hasSeenActiveStatement = true;
                layoutValid = false;
                continue;
            }

            if (*directive == LocatedDirectiveKind::Start) {
                if (hasSeenStart) {
                    result.diagnostics.push_back(PassOneDiagnostic{
                        PassOneDiagnosticCode::DuplicateStart,
                        operationPosition,
                        "Diretiva START duplicada.",
                        std::nullopt,
                    });
                    continue;
                }
                if (hasSeenActiveStatement) {
                    result.diagnostics.push_back(PassOneDiagnostic{
                        PassOneDiagnosticCode::MisplacedStart,
                        operationPosition,
                        "Diretiva START permitida apenas na primeira linha ativa.",
                        std::nullopt,
                    });
                    continue;
                }

                hasSeenStart = true;
                hasSeenActiveStatement = true;

                if (!statement.operand.has_value()) {
                    result.diagnostics.push_back(PassOneDiagnostic{
                        PassOneDiagnosticCode::InvalidStartOperand,
                        operationPosition,
                        "Diretiva START exige um endereco inicial.",
                        std::nullopt,
                    });
                    layoutValid = false;
                    continue;
                }

                const SourceField trimmedStart = trimSourceField(*statement.operand);
                const auto parsedStart = parseKnownInteger(trimmedStart, Address20BitRange);
                if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedStart)) {
                    result.diagnostics.push_back(PassOneDiagnostic{
                        PassOneDiagnosticCode::InvalidStartOperand,
                        diagnostic->position,
                        diagnostic->message,
                        std::nullopt,
                    });
                    layoutValid = false;
                    continue;
                }

                const auto resolvedStart =
                    static_cast<std::uint64_t>(std::get<std::int64_t>(parsedStart));
                startAddress = resolvedStart;
                locationCounter = resolvedStart;
                initialPc = resolvedStart;

                if (statement.label.has_value()) {
                    const auto startAddr32 = static_cast<std::uint32_t>(startAddress);
                    if (const auto diag = defineSymbol(symbols, *statement.label, startAddr32)) {
                        result.diagnostics.push_back(*diag);
                    }
                }

                result.lines.push_back(LocatedLine{
                    line,
                    LocatedLineCategory::Directive,
                    static_cast<std::uint32_t>(startAddress),
                    0,
                    std::string{mnemonic},
                    false,
                    std::nullopt,
                    std::nullopt,
                    LocatedDirectiveKind::Start,
                });
                continue;
            }

            hasSeenActiveStatement = true;

            if (*directive == LocatedDirectiveKind::End) {
                hasSeenEnd = true;

                if (statement.label.has_value() && layoutValid) {
                    if (locationCounter > MaxMemoryAddress) {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::AddressOutOfRange,
                            statement.label->source.position,
                            "Contador de localizacao excede 0xFFFFF no rotulo de END.",
                            std::nullopt,
                        });
                        layoutValid = false;
                    } else {
                        const auto endAddr32 = static_cast<std::uint32_t>(locationCounter);
                        if (const auto diag = defineSymbol(symbols, *statement.label, endAddr32)) {
                            result.diagnostics.push_back(*diag);
                        }
                    }
                }

                if (statement.operand.has_value()) {
                    const SourceField trimmedEnd = trimSourceField(*statement.operand);
                    const auto parsedEnd = parseValue(trimmedEnd);
                    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedEnd)) {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::InvalidEndOperand,
                            diagnostic->position,
                            diagnostic->message,
                            std::nullopt,
                        });
                    } else if (const auto* symbol =
                                   std::get_if<SymbolOperand>(&std::get<OperandValue>(parsedEnd))) {
                        symbols.reference(symbol->source, symbol->position);
                        endEntrySymbol = SourceField{symbol->source, symbol->position};
                    } else {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::InvalidEndOperand,
                            trimmedEnd.position,
                            "Diretiva END aceita apenas um rotulo de entrada valido.",
                            std::nullopt,
                        });
                    }
                }

                result.lines.push_back(LocatedLine{
                    line,
                    LocatedLineCategory::Directive,
                    static_cast<std::uint32_t>(std::min(locationCounter, MaxMemoryAddress)),
                    0,
                    std::string{mnemonic},
                    false,
                    std::nullopt,
                    std::nullopt,
                    LocatedDirectiveKind::End,
                });
                continue;
            }

            if (*directive == LocatedDirectiveKind::Base ||
                *directive == LocatedDirectiveKind::NoBase) {
                if (!layoutValid) {
                    continue;
                }
                if (statement.label.has_value()) {
                    if (locationCounter > MaxMemoryAddress) {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::AddressOutOfRange,
                            statement.label->source.position,
                            "Contador de localizacao fora de 0x00000..0xFFFFF.",
                            std::nullopt,
                        });
                        layoutValid = false;
                        continue;
                    }
                    const auto baseAddr32 = static_cast<std::uint32_t>(locationCounter);
                    if (const auto diag = defineSymbol(symbols, *statement.label, baseAddr32)) {
                        result.diagnostics.push_back(*diag);
                    }
                }

                if (*directive == LocatedDirectiveKind::NoBase) {
                    if (statement.operand.has_value()) {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::InvalidDirectiveOperand,
                            statement.operand->position,
                            "Diretiva NOBASE nao aceita operando.",
                            std::nullopt,
                        });
                    }
                } else {
                    if (!statement.operand.has_value()) {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::InvalidDirectiveOperand,
                            operationPosition,
                            "Diretiva BASE exige operando.",
                            std::nullopt,
                        });
                    } else if (const auto baseDiag =
                                   validateAndReferenceBaseOperand(*statement.operand, symbols)) {
                        result.diagnostics.push_back(*baseDiag);
                    }
                }

                result.lines.push_back(LocatedLine{
                    line,
                    LocatedLineCategory::Directive,
                    static_cast<std::uint32_t>(std::min(locationCounter, MaxMemoryAddress)),
                    0,
                    std::string{mnemonic},
                    false,
                    std::nullopt,
                    std::nullopt,
                    *directive,
                });
                continue;
            }

            if (!layoutValid) {
                continue;
            }
            const auto measured = measureData(line, symbols);
            if (const auto* diagnostic = std::get_if<DataDiagnostic>(&measured)) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::InvalidDirectiveOperand,
                    diagnostic->position,
                    diagnostic->message,
                    std::nullopt,
                });
                layoutValid = false;
                continue;
            }
            const auto& data = std::get<DataMeasurement>(measured);
            const auto nextLocationCounter = locationCounter + data.sizeBytes;
            if (locationCounter > MaxMemoryAddress || nextLocationCounter > MemoryCapacity) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::AddressOutOfRange,
                    operationPosition,
                    "Dados/reserva excedem o espaco enderecavel 0x00000..0xFFFFF.",
                    std::nullopt,
                });
                layoutValid = false;
                continue;
            }
            if (statement.label) {
                if (const auto diagnostic = defineSymbol(
                        symbols, *statement.label, static_cast<std::uint32_t>(locationCounter))) {
                    result.diagnostics.push_back(*diagnostic);
                }
            }
            result.lines.push_back(LocatedLine{
                line,
                data.reservation ? LocatedLineCategory::Reservation : LocatedLineCategory::Data,
                static_cast<std::uint32_t>(locationCounter),
                data.sizeBytes,
                std::string{mnemonic},
                false,
                std::nullopt,
                std::nullopt,
                *directive,
            });
            locationCounter = nextLocationCounter;
            continue;
        }

        hasSeenActiveStatement = true;

        const auto lookup = findInstruction(statement.operation);
        if (const auto* lookupError = std::get_if<InstructionDiagnostic>(&lookup)) {
            const PassOneDiagnosticCode code =
                lookupError->code == InstructionDiagnosticCode::InvalidExtendedFormat
                    ? PassOneDiagnosticCode::ExtendedFormatNotAllowed
                    : PassOneDiagnosticCode::UnknownOperation;
            result.diagnostics.push_back(PassOneDiagnostic{
                code,
                lookupError->source.position,
                lookupError->message,
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
            result.diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::AddressOutOfRange,
                operationPosition,
                "Instrucao excede o espaco enderecavel 0x00000..0xFFFFF.",
                std::nullopt,
            });
            layoutValid = false;
            continue;
        }

        if (statement.label.has_value()) {
            const auto instrAddr32 = static_cast<std::uint32_t>(locationCounter);
            if (const auto diag = defineSymbol(symbols, *statement.label, instrAddr32)) {
                result.diagnostics.push_back(*diag);
            }
        }

        if (const auto operandDiag = validateInstructionAndReference(spec, line, symbols)) {
            result.diagnostics.push_back(*operandDiag);
        }

        result.lines.push_back(LocatedLine{
            line,
            LocatedLineCategory::Instruction,
            static_cast<std::uint32_t>(locationCounter),
            instructionSize,
            std::string{spec.mnemonic},
            extended,
            spec.opcode,
            format,
            std::nullopt,
        });
        locationCounter = nextLocationCounter;
    }

    if (!hasSeenEnd) {
        const std::size_t fallbackLine = parsedLines.empty() ? 1U : parsedLines.back().number;
        result.diagnostics.push_back(PassOneDiagnostic{
            PassOneDiagnosticCode::MissingEnd,
            SourcePosition{fallbackLine, 1U},
            "Diretiva obrigatoria END ausente.",
            std::nullopt,
        });
    }

    for (const auto& unresolved : symbols.unresolvedDiagnostics()) {
        result.diagnostics.push_back(PassOneDiagnostic{
            PassOneDiagnosticCode::UndefinedSymbol,
            unresolved.position,
            unresolved.message,
            std::nullopt,
        });
    }

    if (endEntrySymbol.has_value()) {
        if (const auto resolved = symbols.find(endEntrySymbol->text)) {
            if (resolved->address > MaxMemoryAddress) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::InvalidEndOperand,
                    endEntrySymbol->position,
                    "Endereco de entrada em END fora de 0x00000..0xFFFFF.",
                    std::nullopt,
                });
            } else {
                initialPc = resolved->address;
            }
        }
    }

    result.startAddress = static_cast<std::uint32_t>(startAddress);
    result.initialPc = static_cast<std::uint32_t>(initialPc);
    result.programLength = static_cast<std::uint32_t>(
        locationCounter >= startAddress ? (locationCounter - startAddress) : 0ULL);
    result.symbols = symbols.symbols();
    return result;
}

} // namespace assembler
