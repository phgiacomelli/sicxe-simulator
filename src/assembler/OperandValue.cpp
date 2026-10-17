#include "OperandValue.hpp"

#include <cstddef>
#include <limits>
#include <utility>

namespace assembler {
namespace {

bool isAsciiDigit(char character) {
    return character >= '0' && character <= '9';
}

int parseDigit(char character, IntegerBase base) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (base == IntegerBase::Hexadecimal) {
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
    }
    return -1;
}

bool isCompoundExpression(std::string_view token) {
    for (std::size_t index = 0; index < token.size(); ++index) {
        const char character = token[index];
        if (character == '(' || character == ')' || character == '*' || character == '/') {
            return true;
        }
        if ((character == '+' || character == '-') && index > 0) {
            return true;
        }
    }
    if ((token.front() == '+' || token.front() == '-') && token.size() > 1 &&
        !isAsciiDigit(token[1])) {
        return true;
    }
    return false;
}

bool startsLikeInteger(std::string_view token) {
    return !token.empty() &&
           (isAsciiDigit(token.front()) || token.front() == '+' || token.front() == '-');
}

OperandDiagnostic makeDiagnostic(OperandDiagnosticCode code,
                                 std::string_view token,
                                 SourcePosition position,
                                 std::string message) {
    return OperandDiagnostic{code, std::string{token}, position, std::move(message)};
}

OperandParseResult parseIntegerToken(std::string_view token, SourcePosition position) {
    const bool hasSign = token.front() == '+' || token.front() == '-';
    const bool negative = token.front() == '-';
    std::string_view digits = token.substr(hasSign ? 1 : 0);

    IntegerBase base = IntegerBase::Decimal;
    if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = IntegerBase::Hexadecimal;
        digits.remove_prefix(2);
    }

    if (digits.empty()) {
        return makeDiagnostic(OperandDiagnosticCode::InvalidInteger,
                              token,
                              position,
                              "Inteiro '" + std::string{token} + "' invalido.");
    }

    for (const char character : digits) {
        if (parseDigit(character, base) < 0) {
            return makeDiagnostic(OperandDiagnosticCode::InvalidInteger,
                                  token,
                                  position,
                                  "Inteiro '" + std::string{token} + "' contem digito invalido.");
        }
    }

    const std::uint64_t radix = base == IntegerBase::Hexadecimal ? 16ULL : 10ULL;
    const std::uint64_t maxPositive =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const std::uint64_t limit = negative ? (maxPositive + 1ULL) : maxPositive;

    std::uint64_t magnitude = 0;
    for (const char character : digits) {
        const auto digit = static_cast<std::uint64_t>(parseDigit(character, base));
        if (magnitude > (limit - digit) / radix) {
            return makeDiagnostic(OperandDiagnosticCode::IntegerOverflow,
                                  token,
                                  position,
                                  "Inteiro '" + std::string{token} +
                                      "' excede o limite representavel.");
        }
        magnitude = magnitude * radix + digit;
    }

    std::int64_t value = 0;
    if (negative) {
        if (magnitude == maxPositive + 1ULL) {
            value = std::numeric_limits<std::int64_t>::min();
        } else {
            value = -static_cast<std::int64_t>(magnitude);
        }
    } else {
        value = static_cast<std::int64_t>(magnitude);
    }

    return OperandValue{
        IntegerOperand{value, base, negative, std::string{token}, position},
    };
}

OperandDiagnostic mapSymbolDiagnostic(const SymbolDiagnostic& diagnostic) {
    OperandDiagnosticCode code = OperandDiagnosticCode::InvalidSymbolName;
    switch (diagnostic.code) {
        case SymbolDiagnosticCode::InvalidSymbolName:
            code = OperandDiagnosticCode::InvalidSymbolName;
            break;
        case SymbolDiagnosticCode::ReservedSymbolName:
            code = OperandDiagnosticCode::ReservedSymbolName;
            break;
        case SymbolDiagnosticCode::DuplicateSymbol:
            code = OperandDiagnosticCode::DuplicateSymbol;
            break;
        case SymbolDiagnosticCode::UndefinedSymbol:
            code = OperandDiagnosticCode::UndefinedSymbol;
            break;
    }
    return OperandDiagnostic{code, diagnostic.symbol, diagnostic.position, diagnostic.message};
}

} // namespace

AssemblyDiagnostic toAssemblyDiagnostic(const OperandDiagnostic& diagnostic) {
    AssemblyDiagnosticCode code{};
    switch (diagnostic.code) {
        case OperandDiagnosticCode::EmptyOperand:
            code = AssemblyDiagnosticCode::EmptyOperand;
            break;
        case OperandDiagnosticCode::UnsupportedExpression:
            code = AssemblyDiagnosticCode::UnsupportedExpression;
            break;
        case OperandDiagnosticCode::InvalidInteger:
            code = AssemblyDiagnosticCode::InvalidInteger;
            break;
        case OperandDiagnosticCode::IntegerOverflow:
            code = AssemblyDiagnosticCode::IntegerOverflow;
            break;
        case OperandDiagnosticCode::NegativeValueNotAllowed:
            code = AssemblyDiagnosticCode::NegativeValueNotAllowed;
            break;
        case OperandDiagnosticCode::ValueOutOfRange:
            code = AssemblyDiagnosticCode::ValueOutOfRange;
            break;
        case OperandDiagnosticCode::InvalidSymbolName:
            code = AssemblyDiagnosticCode::InvalidSymbolName;
            break;
        case OperandDiagnosticCode::ReservedSymbolName:
            code = AssemblyDiagnosticCode::ReservedSymbolName;
            break;
        case OperandDiagnosticCode::DuplicateSymbol:
            code = AssemblyDiagnosticCode::DuplicateSymbol;
            break;
        case OperandDiagnosticCode::SymbolNotAllowed:
            code = AssemblyDiagnosticCode::SymbolNotAllowed;
            break;
        case OperandDiagnosticCode::UndefinedSymbol:
            code = AssemblyDiagnosticCode::UndefinedSymbol;
            break;
    }
    return AssemblyDiagnostic{code, diagnostic.position, diagnostic.message};
}

OperandParseResult parseValue(std::string_view token, SourcePosition position) {
    if (token.empty()) {
        return makeDiagnostic(
            OperandDiagnosticCode::EmptyOperand, token, position, "Operando ausente ou vazio.");
    }
    if (isCompoundExpression(token)) {
        return makeDiagnostic(OperandDiagnosticCode::UnsupportedExpression,
                              token,
                              position,
                              "Expressao ou atributo '" + std::string{token} +
                                  "' nao e suportado; use apenas um rotulo ou um inteiro.");
    }
    if (startsLikeInteger(token)) {
        return parseIntegerToken(token, position);
    }
    if (!SymbolTable::isValidIdentifier(token)) {
        return makeDiagnostic(OperandDiagnosticCode::InvalidSymbolName,
                              token,
                              position,
                              "Simbolo '" + std::string{token} +
                                  "' invalido; use uma letra seguida de letras, digitos ou '_'.");
    }
    if (SymbolTable::isReservedName(token)) {
        return makeDiagnostic(OperandDiagnosticCode::ReservedSymbolName,
                              token,
                              position,
                              "Simbolo '" + std::string{token} +
                                  "' colide com mnemonico ou diretiva reservada.");
    }
    return OperandValue{
        SymbolOperand{SymbolTable::normalizeName(token), std::string{token}, position},
    };
}

OperandParseResult parseValue(const SourceField& field) {
    return parseValue(field.text, field.position);
}

OperandNumberResult validateInteger(const OperandValue& operand, IntegerRange range) {
    if (const auto* symbol = std::get_if<SymbolOperand>(&operand)) {
        return makeDiagnostic(OperandDiagnosticCode::SymbolNotAllowed,
                              symbol->source,
                              symbol->position,
                              "Operando '" + symbol->source +
                                  "' deve ser um inteiro conhecido nesta diretiva.");
    }
    const auto& integer = std::get<IntegerOperand>(operand);
    if (!range.allowNegative && (integer.negative || integer.value < 0)) {
        return makeDiagnostic(OperandDiagnosticCode::NegativeValueNotAllowed,
                              integer.text,
                              integer.position,
                              "Inteiro negativo '" + integer.text +
                                  "' nao e permitido neste operando.");
    }
    if (integer.value < range.min || integer.value > range.max) {
        return makeDiagnostic(OperandDiagnosticCode::ValueOutOfRange,
                              integer.text,
                              integer.position,
                              "Inteiro '" + integer.text + "' fora da faixa permitida.");
    }
    return integer.value;
}

OperandNumberResult
parseKnownInteger(std::string_view token, SourcePosition position, IntegerRange range) {
    const auto parsed = parseValue(token, position);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsed)) {
        return *diagnostic;
    }
    return validateInteger(std::get<OperandValue>(parsed), range);
}

OperandNumberResult parseKnownInteger(const SourceField& field, IntegerRange range) {
    return parseKnownInteger(field.text, field.position, range);
}

std::optional<OperandDiagnostic>
referenceOperand(const OperandValue& operand, SymbolTable& symbols, IntegerRange integerRange) {
    if (const auto* symbol = std::get_if<SymbolOperand>(&operand)) {
        if (const auto diagnostic = symbols.reference(symbol->source, symbol->position)) {
            return mapSymbolDiagnostic(*diagnostic);
        }
        return std::nullopt;
    }
    const auto validated = validateInteger(operand, integerRange);
    if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&validated)) {
        return *diagnostic;
    }
    return std::nullopt;
}

OperandNumberResult
resolveOperand(const OperandValue& operand, const SymbolTable& symbols, IntegerRange integerRange) {
    if (const auto* symbol = std::get_if<SymbolOperand>(&operand)) {
        const auto resolved = symbols.resolve(symbol->source, symbol->position);
        if (const auto* diagnostic = std::get_if<SymbolDiagnostic>(&resolved)) {
            return mapSymbolDiagnostic(*diagnostic);
        }
        return static_cast<std::int64_t>(std::get<std::uint32_t>(resolved));
    }
    return validateInteger(operand, integerRange);
}

} // namespace assembler
