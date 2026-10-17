#include "PassOne.hpp"

#if __has_include("OperandValue.hpp") && __has_include("SymbolTable.hpp")
#define SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE 1
#include "OperandValue.hpp"
#include "SymbolTable.hpp"
#else
#define SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE 0
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace assembler {
namespace {

constexpr std::uint64_t MaxMemoryAddress = MaxAddressableByte;
constexpr std::uint64_t MemoryCapacity = AddressSpaceBytes;

enum class InstructionOperandPattern {
    NoOperand,
    Format2SingleReg,
    Format2RegShift,
    Format2TwoRegs,
    Memory,
};

struct InstructionSpec {
    std::string_view mnemonic;
    std::uint8_t opcode{};
    InstructionOperandPattern pattern{InstructionOperandPattern::Memory};
    bool allowsExtended{true};
};

constexpr std::array<InstructionSpec, 38> InstructionSpecs = {{
    {"ADD", 0x18, InstructionOperandPattern::Memory, true},
    {"ADDR", 0x90, InstructionOperandPattern::Format2TwoRegs, false},
    {"AND", 0x40, InstructionOperandPattern::Memory, true},
    {"CLEAR", 0xB4, InstructionOperandPattern::Format2SingleReg, false},
    {"COMP", 0x28, InstructionOperandPattern::Memory, true},
    {"COMPR", 0xA0, InstructionOperandPattern::Format2TwoRegs, false},
    {"DIV", 0x24, InstructionOperandPattern::Memory, true},
    {"DIVR", 0x9C, InstructionOperandPattern::Format2TwoRegs, false},
    {"J", 0x3C, InstructionOperandPattern::Memory, true},
    {"JEQ", 0x30, InstructionOperandPattern::Memory, true},
    {"JGT", 0x34, InstructionOperandPattern::Memory, true},
    {"JLT", 0x38, InstructionOperandPattern::Memory, true},
    {"JSUB", 0x48, InstructionOperandPattern::Memory, true},
    {"LDA", 0x00, InstructionOperandPattern::Memory, true},
    {"LDB", 0x68, InstructionOperandPattern::Memory, true},
    {"LDCH", 0x50, InstructionOperandPattern::Memory, true},
    {"LDL", 0x08, InstructionOperandPattern::Memory, true},
    {"LDS", 0x6C, InstructionOperandPattern::Memory, true},
    {"LDT", 0x74, InstructionOperandPattern::Memory, true},
    {"LDX", 0x04, InstructionOperandPattern::Memory, true},
    {"MUL", 0x20, InstructionOperandPattern::Memory, true},
    {"MULR", 0x98, InstructionOperandPattern::Format2TwoRegs, false},
    {"OR", 0x44, InstructionOperandPattern::Memory, true},
    {"RMO", 0xAC, InstructionOperandPattern::Format2TwoRegs, false},
    {"RSUB", 0x4C, InstructionOperandPattern::NoOperand, false},
    {"SHIFTL", 0xA4, InstructionOperandPattern::Format2RegShift, false},
    {"SHIFTR", 0xA8, InstructionOperandPattern::Format2RegShift, false},
    {"STA", 0x0C, InstructionOperandPattern::Memory, true},
    {"STB", 0x78, InstructionOperandPattern::Memory, true},
    {"STCH", 0x54, InstructionOperandPattern::Memory, true},
    {"STL", 0x14, InstructionOperandPattern::Memory, true},
    {"STS", 0x7C, InstructionOperandPattern::Memory, true},
    {"STT", 0x84, InstructionOperandPattern::Memory, true},
    {"STX", 0x10, InstructionOperandPattern::Memory, true},
    {"SUB", 0x1C, InstructionOperandPattern::Memory, true},
    {"SUBR", 0x94, InstructionOperandPattern::Format2TwoRegs, false},
    {"TIX", 0x2C, InstructionOperandPattern::Memory, true},
    {"TIXR", 0xB8, InstructionOperandPattern::Format2SingleReg, false},
}};

constexpr std::array<std::string_view, 46> ReservedNames = {
    "ADD",  "ADDR",   "AND",    "BASE", "BYTE",  "CLEAR",  "COMP", "COMPR", "DIV",  "DIVR",
    "END",  "J",      "JEQ",    "JGT",  "JLT",   "JSUB",   "LDA",  "LDB",   "LDCH", "LDL",
    "LDS",  "LDT",    "LDX",    "MUL",  "MULR",  "NOBASE", "OR",   "RESB",  "RESW", "RMO",
    "RSUB", "SHIFTL", "SHIFTR", "STA",  "START", "STB",    "STCH", "STL",   "STS",  "STT",
    "STX",  "SUB",    "SUBR",   "TIX",  "TIXR",  "WORD",
};

constexpr std::array<std::string_view, 8> ValidRegisters = {
    "A", "B", "L", "PC", "S", "SW", "T", "X",
};

bool isHorizontalSpace(char character) {
    return character == ' ' || character == '\t';
}

bool isAsciiLetter(char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

bool isAsciiDigit(char character) {
    return character >= '0' && character <= '9';
}

std::string normalizeUpper(std::string_view text) {
    std::string normalized{text};
    for (char& character : normalized) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return normalized;
}

bool isValidIdentifierName(std::string_view text) {
    if (text.empty() || !isAsciiLetter(text.front())) {
        return false;
    }
    for (std::size_t index = 1; index < text.size(); ++index) {
        const char character = text[index];
        if (!isAsciiLetter(character) && !isAsciiDigit(character) && character != '_') {
            return false;
        }
    }
    return true;
}

bool isReservedIdentifierName(std::string_view text) {
    if (!isValidIdentifierName(text)) {
        return false;
    }
    const std::string key = normalizeUpper(text);
    return std::binary_search(ReservedNames.begin(), ReservedNames.end(), key);
}

bool isValidRegisterName(std::string_view text) {
    const std::string key = normalizeUpper(text);
    return std::binary_search(ValidRegisters.begin(), ValidRegisters.end(), key);
}

const InstructionSpec* findInstructionSpec(std::string_view mnemonic) {
    const auto it = std::lower_bound(
        InstructionSpecs.begin(),
        InstructionSpecs.end(),
        mnemonic,
        [](const InstructionSpec& spec, std::string_view target) {
            return spec.mnemonic < target;
        });
    if (it != InstructionSpecs.end() && it->mnemonic == mnemonic) {
        return &(*it);
    }
    return nullptr;
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

LocatedLineCategory categoryForDirective(LocatedDirectiveKind directive) {
    switch (directive) {
        case LocatedDirectiveKind::Byte:
        case LocatedDirectiveKind::Word:
            return LocatedLineCategory::Data;
        case LocatedDirectiveKind::Resb:
        case LocatedDirectiveKind::Resw:
            return LocatedLineCategory::Reservation;
        case LocatedDirectiveKind::Start:
        case LocatedDirectiveKind::End:
        case LocatedDirectiveKind::Base:
        case LocatedDirectiveKind::NoBase:
            return LocatedLineCategory::Directive;
    }
    return LocatedLineCategory::Directive;
}

struct TrimmedSlice {
    std::string_view text;
    std::size_t offset{};
};

TrimmedSlice trimSlice(std::string_view text, std::size_t baseOffset = 0) {
    std::size_t start = 0;
    while (start < text.size() && isHorizontalSpace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && isHorizontalSpace(text[end - 1])) {
        --end;
    }
    return TrimmedSlice{text.substr(start, end - start), baseOffset + start};
}

int parseDigitValue(char character, std::uint64_t radix) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (radix == 16ULL) {
        if (character >= 'a' && character <= 'f') {
            return 10 + (character - 'a');
        }
        if (character >= 'A' && character <= 'F') {
            return 10 + (character - 'A');
        }
    }
    return -1;
}

std::optional<std::uint64_t> parseUnsignedInteger(std::string_view token, std::uint64_t maxValue) {
    if (token.empty() || token.front() == '-') {
        return std::nullopt;
    }
    std::string_view digits = token;
    if (digits.front() == '+') {
        digits.remove_prefix(1);
    }
    std::uint64_t radix = 10ULL;
    if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        radix = 16ULL;
        digits.remove_prefix(2);
    }
    if (digits.empty()) {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    for (const char character : digits) {
        const int digit = parseDigitValue(character, radix);
        if (digit < 0) {
            return std::nullopt;
        }
        const auto digitValue = static_cast<std::uint64_t>(digit);
        if (value > (maxValue - digitValue) / radix) {
            return std::nullopt;
        }
        value = value * radix + digitValue;
    }
    return value;
}

class PassOneSymbolTracker {
public:
    std::optional<PassOneDiagnostic> define(const SourceIdentifier& label, std::uint32_t address) {
#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
        if (const auto diagnostic = table_.define(label, address)) {
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
#else
        const std::string_view rawName = label.source.text.empty()
                                             ? std::string_view{label.key}
                                             : std::string_view{label.source.text};
        const SourcePosition position = label.source.position;
        if (!isValidIdentifierName(rawName) || isReservedIdentifierName(rawName)) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidLabel,
                position,
                "Rotulo '" + std::string{rawName} +
                    "' invalido ou colide com mnemonico/diretiva reservada.",
                std::nullopt,
            };
        }
        const std::string key = normalizeUpper(rawName);
        if (const auto existing = indexByName_.find(key); existing != indexByName_.end()) {
            const SourcePosition previous = symbols_[existing->second].definition;
            return PassOneDiagnostic{
                PassOneDiagnosticCode::DuplicateSymbol,
                position,
                "Rotulo '" + std::string{rawName} + "' redefinido.",
                previous,
            };
        }
        indexByName_.emplace(key, symbols_.size());
        symbols_.push_back(AssemblySymbol{key, address, position});
        return std::nullopt;
#endif
    }

    bool reference(std::string_view symbolText, SourcePosition position) {
#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
        return !table_.reference(symbolText, position).has_value();
#else
        if (!isValidIdentifierName(symbolText) || isReservedIdentifierName(symbolText)) {
            return false;
        }
        references_.push_back(PendingReference{
            normalizeUpper(symbolText),
            std::string{symbolText},
            position,
        });
        return true;
#endif
    }

    std::optional<AssemblySymbol> find(std::string_view name) const {
#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
        return table_.find(name);
#else
        if (!isValidIdentifierName(name)) {
            return std::nullopt;
        }
        const std::string key = normalizeUpper(name);
        const auto existing = indexByName_.find(key);
        if (existing == indexByName_.end()) {
            return std::nullopt;
        }
        return symbols_[existing->second];
#endif
    }

    void appendUnresolvedDiagnostics(std::vector<PassOneDiagnostic>& diagnostics) const {
#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
        for (const auto& unresolved : table_.unresolvedDiagnostics()) {
            diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::UndefinedSymbol,
                unresolved.position,
                unresolved.message,
                std::nullopt,
            });
        }
#else
        for (const auto& entry : references_) {
            if (!indexByName_.contains(entry.key)) {
                diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::UndefinedSymbol,
                    entry.position,
                    "Simbolo '" + entry.source + "' nao foi definido.",
                    std::nullopt,
                });
            }
        }
#endif
    }

    const std::vector<AssemblySymbol>& symbols() const noexcept {
#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
        return table_.symbols();
#else
        return symbols_;
#endif
    }

private:
#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
    SymbolTable table_;
#else
    struct PendingReference {
        std::string key;
        std::string source;
        SourcePosition position;
    };

    std::vector<AssemblySymbol> symbols_;
    std::unordered_map<std::string, std::size_t> indexByName_;
    std::vector<PendingReference> references_;
#endif
};

bool validateFormat2Operand(InstructionOperandPattern pattern, const SourceField& operandField) {
    const std::string_view text = operandField.text;
    const auto commaPos = text.find(',');
    if (pattern == InstructionOperandPattern::Format2SingleReg) {
        if (commaPos != std::string_view::npos) {
            return false;
        }
        const auto reg = trimSlice(text);
        return isValidRegisterName(reg.text);
    }

    if (commaPos == std::string_view::npos ||
        text.find(',', commaPos + 1) != std::string_view::npos) {
        return false;
    }

    const auto first = trimSlice(text.substr(0, commaPos), 0);
    const auto second = trimSlice(text.substr(commaPos + 1), commaPos + 1);
    if (!isValidRegisterName(first.text) || second.text.empty()) {
        return false;
    }

    if (pattern == InstructionOperandPattern::Format2TwoRegs) {
        return isValidRegisterName(second.text);
    }

    const auto shiftCount = parseUnsignedInteger(second.text, 16ULL);
    return shiftCount.has_value() && *shiftCount >= 1ULL && *shiftCount <= 16ULL;
}

#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
std::optional<OperandDiagnostic> referenceOperand(const OperandValue& value,
                                                  PassOneSymbolTracker& symbols,
                                                  IntegerRange range) {
    if (const auto* symbol = std::get_if<SymbolOperand>(&value)) {
        if (!symbols.reference(symbol->source, symbol->position)) {
            return OperandDiagnostic{
                OperandDiagnosticCode::ReservedSymbolName,
                symbol->source,
                symbol->position,
                "Simbolo reservado.",
            };
        }
        return std::nullopt;
    }
    const auto validated = validateInteger(value, range);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&validated)) {
        return *diagnostic;
    }
    return std::nullopt;
}
#endif

std::optional<PassOneDiagnostic> validateAndReferenceMemoryOperand(
    const SourceField& operandField,
    PassOneSymbolTracker& symbols) {
    std::string_view raw = operandField.text;
    std::size_t offset = 0;
    if (raw.empty()) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidInstructionOperand,
            operandField.position,
            "Operando de instrucao vazio.",
            std::nullopt,
        };
    }

    const bool hasPrefix = raw.front() == '#' || raw.front() == '@';
    if (hasPrefix) {
        raw.remove_prefix(1);
        offset += 1;
    }

    const auto commaPos = raw.find(',');
    std::size_t targetOffset = offset;
    std::string_view targetText = raw;

    if (commaPos != std::string_view::npos) {
        if (hasPrefix || raw.find(',', commaPos + 1) != std::string_view::npos) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidInstructionOperand,
                SourcePosition{operandField.position.line,
                               operandField.position.column + offset + commaPos},
                "Indexacao ',X' e permitida apenas no modo simples e com um unico sufixo.",
                std::nullopt,
            };
        }
        const auto left = trimSlice(raw.substr(0, commaPos), offset);
        const auto right = trimSlice(raw.substr(commaPos + 1), offset + commaPos + 1);
        if (normalizeUpper(right.text) != "X") {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidInstructionOperand,
                SourcePosition{operandField.position.line,
                               operandField.position.column + right.offset},
                "Indexacao invalida; apenas o registrador X e aceito.",
                std::nullopt,
            };
        }
        targetText = left.text;
        targetOffset = left.offset;
    } else {
        const auto trimmed = trimSlice(raw, offset);
        targetText = trimmed.text;
        targetOffset = trimmed.offset;
    }

    const SourcePosition targetPosition{
        operandField.position.line,
        operandField.position.column + targetOffset,
    };

    if (targetText.empty()) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidInstructionOperand,
            targetPosition,
            "Valor do operando ausente.",
            std::nullopt,
        };
    }

#if SICXE_ASSEMBLER_HAS_SYMBOL_PACKAGE
    const auto parsed = parseValue(targetText, targetPosition);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsed)) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidInstructionOperand,
            diagnostic->position,
            diagnostic->message,
            std::nullopt,
        };
    }
    const auto& value = std::get<OperandValue>(parsed);
    if (const auto refDiagnostic = referenceOperand(value, symbols, Address20BitRange)) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidInstructionOperand,
            refDiagnostic->position,
            refDiagnostic->message,
            std::nullopt,
        };
    }
    return std::nullopt;
#else
    if (isValidIdentifierName(targetText)) {
        if (!symbols.reference(targetText, targetPosition)) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidInstructionOperand,
                targetPosition,
                "Simbolo '" + std::string{targetText} +
                    "' colide com mnemonico ou diretiva reservada.",
                std::nullopt,
            };
        }
        return std::nullopt;
    }

    if (parseUnsignedInteger(targetText, MaxMemoryAddress).has_value()) {
        return std::nullopt;
    }

    return PassOneDiagnostic{
        PassOneDiagnosticCode::InvalidInstructionOperand,
        targetPosition,
        "Operando '" + std::string{targetText} +
            "' invalido; use um rotulo ou inteiro nao negativo de ate 20 bits.",
        std::nullopt,
    };
#endif
}

std::optional<PassOneDiagnostic> validateAndReferenceBaseOperand(const SourceField& operandField,
                                                                 PassOneSymbolTracker& symbols) {
    const auto trimmed = trimSlice(operandField.text, 0);
    const SourcePosition position{
        operandField.position.line,
        operandField.position.column + trimmed.offset,
    };
    if (trimmed.text.empty()) {
        return PassOneDiagnostic{
            PassOneDiagnosticCode::InvalidDirectiveOperand,
            position,
            "Diretiva BASE exige um rotulo ou endereco.",
            std::nullopt,
        };
    }
    if (isValidIdentifierName(trimmed.text)) {
        if (!symbols.reference(trimmed.text, position)) {
            return PassOneDiagnostic{
                PassOneDiagnosticCode::InvalidDirectiveOperand,
                position,
                "Operando de BASE colide com nome reservado.",
                std::nullopt,
            };
        }
        return std::nullopt;
    }
    if (parseUnsignedInteger(trimmed.text, MaxMemoryAddress).has_value()) {
        return std::nullopt;
    }
    return PassOneDiagnostic{
        PassOneDiagnosticCode::InvalidDirectiveOperand,
        position,
        "Operando de BASE deve ser um rotulo ou endereco de 20 bits.",
        std::nullopt,
    };
}

} // namespace

std::optional<AssemblySymbol> PassOneResult::findSymbol(std::string_view name) const {
    if (!isValidIdentifierName(name)) {
        return std::nullopt;
    }
    const std::string key = normalizeUpper(name);
    for (const auto& symbol : symbols) {
        if (symbol.name == key) {
            return symbol;
        }
    }
    return std::nullopt;
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

    PassOneSymbolTracker symbols;
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
        const std::string_view mnemonic =
            extended ? operationKey.substr(1) : operationKey;
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

                const auto trimmedStart = trimSlice(statement.operand->text, 0);
                const SourcePosition startOperandPosition{
                    statement.operand->position.line,
                    statement.operand->position.column + trimmedStart.offset,
                };
                const auto parsedStart =
                    parseUnsignedInteger(trimmedStart.text, MaxMemoryAddress);
                if (!parsedStart.has_value()) {
                    result.diagnostics.push_back(PassOneDiagnostic{
                        PassOneDiagnosticCode::InvalidStartOperand,
                        startOperandPosition,
                        "Endereco inicial de START invalido ou fora de 0x00000..0xFFFFF.",
                        std::nullopt,
                    });
                    layoutValid = false;
                    continue;
                }

                startAddress = *parsedStart;
                locationCounter = *parsedStart;
                initialPc = *parsedStart;

                if (statement.label.has_value()) {
                    const auto startAddr32 = static_cast<std::uint32_t>(startAddress);
                    if (const auto labelDiag = symbols.define(*statement.label, startAddr32)) {
                        result.diagnostics.push_back(*labelDiag);
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
                    } else if (const auto labelDiag = symbols.define(
                                   *statement.label,
                                   static_cast<std::uint32_t>(locationCounter))) {
                        result.diagnostics.push_back(*labelDiag);
                    }
                }

                if (statement.operand.has_value()) {
                    const auto trimmedEnd = trimSlice(statement.operand->text, 0);
                    const SourcePosition endOperandPosition{
                        statement.operand->position.line,
                        statement.operand->position.column + trimmedEnd.offset,
                    };
                    if (!isValidIdentifierName(trimmedEnd.text) ||
                        isReservedIdentifierName(trimmedEnd.text)) {
                        result.diagnostics.push_back(PassOneDiagnostic{
                            PassOneDiagnosticCode::InvalidEndOperand,
                            endOperandPosition,
                            "Diretiva END aceita apenas um rotulo de entrada valido.",
                            std::nullopt,
                        });
                    } else {
                        symbols.reference(trimmedEnd.text, endOperandPosition);
                        endEntrySymbol =
                            SourceField{std::string{trimmedEnd.text}, endOperandPosition};
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
                    if (const auto labelDiag = symbols.define(
                            *statement.label, static_cast<std::uint32_t>(locationCounter))) {
                        result.diagnostics.push_back(*labelDiag);
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

            // Data/reservation directives (BYTE, WORD, RESB, RESW) before Package 5 integration.
            if (statement.label.has_value() && layoutValid && locationCounter <= MaxMemoryAddress) {
                if (const auto labelDiag = symbols.define(
                        *statement.label, static_cast<std::uint32_t>(locationCounter))) {
                    result.diagnostics.push_back(*labelDiag);
                }
            }
            if (layoutValid && locationCounter <= MaxMemoryAddress) {
                result.lines.push_back(LocatedLine{
                    line,
                    categoryForDirective(*directive),
                    static_cast<std::uint32_t>(locationCounter),
                    0,
                    std::string{mnemonic},
                    false,
                    std::nullopt,
                    std::nullopt,
                    *directive,
                });
            }
            result.diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::DataDirectiveNotIntegrated,
                operationPosition,
                "Diretiva de dados/reserva '" + std::string{mnemonic} +
                    "' ainda nao integrada; depende do pacote 5.",
                std::nullopt,
            });
            layoutValid = false;
            continue;
        }

        hasSeenActiveStatement = true;

        const InstructionSpec* spec = findInstructionSpec(mnemonic);
        if (spec == nullptr) {
            result.diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::UnknownOperation,
                operationPosition,
                "Operacao '" + statement.operation.source.text + "' desconhecida na SIC/XE.",
                std::nullopt,
            });
            layoutValid = false;
            continue;
        }

        if (extended && !spec->allowsExtended) {
            result.diagnostics.push_back(PassOneDiagnostic{
                PassOneDiagnosticCode::ExtendedFormatNotAllowed,
                operationPosition,
                "Instrucao '" + std::string{mnemonic} + "' nao suporta formato 4 ('+').",
                std::nullopt,
            });
            layoutValid = false;
            continue;
        }

        LocatedInstructionFormat format = LocatedInstructionFormat::Format3;
        std::size_t instructionSize = 3;
        if (spec->pattern == InstructionOperandPattern::Format2SingleReg ||
            spec->pattern == InstructionOperandPattern::Format2RegShift ||
            spec->pattern == InstructionOperandPattern::Format2TwoRegs) {
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
            if (const auto labelDiag = symbols.define(
                    *statement.label, static_cast<std::uint32_t>(locationCounter))) {
                result.diagnostics.push_back(*labelDiag);
            }
        }

        if (spec->pattern == InstructionOperandPattern::NoOperand) {
            if (statement.operand.has_value()) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::InvalidInstructionOperand,
                    statement.operand->position,
                    "Instrucao RSUB nao aceita operando.",
                    std::nullopt,
                });
            }
        } else if (spec->pattern == InstructionOperandPattern::Memory) {
            if (!statement.operand.has_value()) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::InvalidInstructionOperand,
                    operationPosition,
                    "Instrucao '" + std::string{mnemonic} + "' exige operando.",
                    std::nullopt,
                });
            } else if (const auto operandDiag =
                           validateAndReferenceMemoryOperand(*statement.operand, symbols)) {
                result.diagnostics.push_back(*operandDiag);
            }
        } else {
            if (!statement.operand.has_value()) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::InvalidInstructionOperand,
                    operationPosition,
                    "Instrucao de formato 2 '" + std::string{mnemonic} + "' exige operando.",
                    std::nullopt,
                });
            } else if (!validateFormat2Operand(spec->pattern, *statement.operand)) {
                result.diagnostics.push_back(PassOneDiagnostic{
                    PassOneDiagnosticCode::InvalidInstructionOperand,
                    statement.operand->position,
                    "Operando invalido para instrucao de formato 2 '" +
                        std::string{mnemonic} + "'.",
                    std::nullopt,
                });
            }
        }

        result.lines.push_back(LocatedLine{
            line,
            LocatedLineCategory::Instruction,
            static_cast<std::uint32_t>(locationCounter),
            instructionSize,
            std::string{mnemonic},
            extended,
            spec->opcode,
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

    symbols.appendUnresolvedDiagnostics(result.diagnostics);

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
