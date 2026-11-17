#include "ObjectOperand.hpp"

#include <limits>
#include <utility>

namespace assembler {
namespace {

bool isHorizontalSpace(char character) {
    return character == ' ' || character == '\t';
}

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

bool isUnsupportedOperatorOrDelimiter(char character) {
    return character == '*' || character == '/' || character == '(' || character == ')' ||
           character == '=';
}

OperandDiagnostic makeDiagnostic(OperandDiagnosticCode code,
                                 std::string_view token,
                                 SourcePosition position,
                                 std::string message) {
    return OperandDiagnostic{code, std::string{token}, position, std::move(message)};
}

std::variant<IntegerOperand, OperandDiagnostic> parseUnsignedTermInteger(std::string_view rawTerm,
                                                                         std::string_view signedRaw,
                                                                         ObjectTermSign sign,
                                                                         SourcePosition termPos,
                                                                         SourcePosition signedPos) {
    std::string_view digits = rawTerm;
    IntegerBase base = IntegerBase::Decimal;
    if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = IntegerBase::Hexadecimal;
        digits.remove_prefix(2);
    }

    if (digits.empty()) {
        return makeDiagnostic(OperandDiagnosticCode::InvalidInteger,
                              signedRaw,
                              signedPos,
                              "Inteiro '" + std::string{signedRaw} + "' invalido em WORD.");
    }

    for (const char character : digits) {
        if (parseDigit(character, base) < 0) {
            return makeDiagnostic(OperandDiagnosticCode::InvalidInteger,
                                  signedRaw,
                                  signedPos,
                                  "Inteiro '" + std::string{signedRaw} +
                                      "' contem digito invalido.");
        }
    }

    const std::uint64_t radix = base == IntegerBase::Hexadecimal ? 16ULL : 10ULL;
    const std::uint64_t maxPositive =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const bool negative = sign == ObjectTermSign::Subtract;
    const std::uint64_t limit = negative ? (maxPositive + 1ULL) : maxPositive;

    std::uint64_t magnitude = 0;
    for (const char character : digits) {
        const auto digit = static_cast<std::uint64_t>(parseDigit(character, base));
        if (magnitude > (limit - digit) / radix) {
            return makeDiagnostic(OperandDiagnosticCode::IntegerOverflow,
                                  signedRaw,
                                  signedPos,
                                  "Inteiro '" + std::string{signedRaw} +
                                      "' excede o limite representavel.");
        }
        magnitude = magnitude * radix + digit;
    }

    std::int64_t signedValue = 0;
    if (negative) {
        if (magnitude == maxPositive + 1ULL) {
            signedValue = std::numeric_limits<std::int64_t>::min();
        } else {
            signedValue = -static_cast<std::int64_t>(magnitude);
        }
    } else {
        signedValue = static_cast<std::int64_t>(magnitude);
    }

    if (signedValue < Word24BitRange.min || signedValue > Word24BitRange.max) {
        return makeDiagnostic(OperandDiagnosticCode::ValueOutOfRange,
                              signedRaw,
                              signedPos,
                              "Inteiro '" + std::string{signedRaw} +
                                  "' fora da faixa permitida de 24 bits para WORD.");
    }

    return IntegerOperand{
        static_cast<std::int64_t>(magnitude),
        base,
        negative,
        std::string{rawTerm},
        termPos,
    };
}

} // namespace

bool isObjectReservedName(std::string_view name) {
    if (!SymbolTable::isValidIdentifier(name)) {
        return false;
    }
    if (SymbolTable::isReservedName(name)) {
        return true;
    }
    const std::string normalized = SymbolTable::normalizeName(name);
    return normalized == "EXTDEF" || normalized == "EXTREF";
}

bool isValidObjectGlobalName(std::string_view name) {
    return SymbolTable::isValidIdentifier(name) && !isObjectReservedName(name) &&
           name.size() <= MaxObjectSymbolLength;
}

std::int64_t ObjectWordExpression::evaluateInitialAddend() const noexcept {
    std::int64_t total = 0;
    for (const auto& term : terms) {
        total += term.initialContribution();
    }
    return total;
}

std::int64_t ObjectWordExpression::localRelocationBalance() const noexcept {
    std::int64_t balance = 0;
    for (const auto& term : terms) {
        if (std::holds_alternative<LocalSymbolReference>(term.value)) {
            balance += (term.sign == ObjectTermSign::Subtract ? -1 : 1);
        }
    }
    return balance;
}

ObjectWordParseResult parseObjectWordExpression(std::string_view text, SourcePosition position) {
    std::size_t start = 0;
    while (start < text.size() && isHorizontalSpace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && isHorizontalSpace(text[end - 1])) {
        --end;
    }
    const std::string_view trimmed = text.substr(start, end - start);
    const SourcePosition basePosition{position.line, position.column + start};

    if (trimmed.empty()) {
        return makeDiagnostic(OperandDiagnosticCode::EmptyOperand,
                              trimmed,
                              basePosition,
                              "Operando ausente ou vazio em WORD.");
    }

    for (std::size_t index = 0; index < trimmed.size(); ++index) {
        if (isUnsupportedOperatorOrDelimiter(trimmed[index])) {
            return makeDiagnostic(OperandDiagnosticCode::UnsupportedExpression,
                                  trimmed,
                                  basePosition,
                                  "Operador ou delimitador nao suportado em WORD; use apenas '+' e "
                                  "'-'.");
        }
    }

    ParsedObjectWordExpression expression;
    std::size_t cursor = 0;
    bool firstTerm = true;

    while (cursor < trimmed.size()) {
        ObjectTermSign sign = ObjectTermSign::Add;
        bool explicitSign = false;
        const std::size_t signOffset = cursor;

        if (trimmed[cursor] == '+' || trimmed[cursor] == '-') {
            sign = trimmed[cursor] == '-' ? ObjectTermSign::Subtract : ObjectTermSign::Add;
            explicitSign = true;
            ++cursor;
        } else if (!firstTerm) {
            return makeDiagnostic(OperandDiagnosticCode::UnsupportedExpression,
                                  trimmed,
                                  SourcePosition{basePosition.line, basePosition.column + cursor},
                                  "Termo sem operador '+' ou '-' em WORD.");
        }

        const std::size_t termStart = cursor;
        while (cursor < trimmed.size() && trimmed[cursor] != '+' && trimmed[cursor] != '-') {
            ++cursor;
        }

        const std::string_view rawTerm = trimmed.substr(termStart, cursor - termStart);
        if (rawTerm.empty()) {
            const SourcePosition signPos{basePosition.line, basePosition.column + signOffset};
            return makeDiagnostic(OperandDiagnosticCode::UnsupportedExpression,
                                  trimmed.substr(signOffset, 1),
                                  signPos,
                                  "Sinal sem termo na expressao de WORD.");
        }

        const SourcePosition termPos{basePosition.line, basePosition.column + termStart};
        const std::size_t signedStart = explicitSign ? signOffset : termStart;
        const SourcePosition signedPos{basePosition.line, basePosition.column + signedStart};
        const std::string_view signedRaw = trimmed.substr(signedStart, cursor - signedStart);

        if (isAsciiDigit(rawTerm.front())) {
            const auto parsedInt =
                parseUnsignedTermInteger(rawTerm, signedRaw, sign, termPos, signedPos);
            if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsedInt)) {
                return *diagnostic;
            }
            expression.terms.push_back(ParsedWordTerm{
                sign,
                signedPos,
                OperandValue{std::get<IntegerOperand>(parsedInt)},
            });
        } else {
            if (!SymbolTable::isValidIdentifier(rawTerm)) {
                return makeDiagnostic(OperandDiagnosticCode::InvalidSymbolName,
                                      rawTerm,
                                      termPos,
                                      "Simbolo '" + std::string{rawTerm} +
                                          "' invalido; use uma letra seguida de letras, digitos ou "
                                          "'_'.");
            }
            if (isObjectReservedName(rawTerm)) {
                return makeDiagnostic(OperandDiagnosticCode::ReservedSymbolName,
                                      rawTerm,
                                      termPos,
                                      "Simbolo '" + std::string{rawTerm} +
                                          "' colide com mnemonico ou diretiva reservada.");
            }
            expression.terms.push_back(ParsedWordTerm{
                sign,
                termPos,
                OperandValue{
                    SymbolOperand{
                        SymbolTable::normalizeName(rawTerm), std::string{rawTerm}, termPos},
                },
            });
        }

        firstTerm = false;
    }

    return expression;
}

ObjectWordParseResult parseObjectWordExpression(const SourceField& field) {
    return parseObjectWordExpression(field.text, field.position);
}

} // namespace assembler
