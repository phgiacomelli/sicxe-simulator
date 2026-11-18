#include "ObjectOperand.hpp"

#include <utility>

namespace assembler {
namespace {

bool isHorizontalSpace(char character) {
    return character == ' ' || character == '\t';
}

bool isAsciiDigit(char character) {
    return character >= '0' && character <= '9';
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
            // The first sign can belong to a literal. Later signs are binary operators.
            const auto parsed =
                parseValue(firstTerm ? signedRaw : rawTerm, firstTerm ? signedPos : termPos);
            if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&parsed)) {
                return *diagnostic;
            }
            const auto& value = std::get<OperandValue>(parsed);
            const auto validated = validateInteger(value, Word24BitRange);
            if (const auto* diagnostic = std::get_if<OperandDiagnostic>(&validated)) {
                return *diagnostic;
            }
            const auto& integer = std::get<IntegerOperand>(value);
            expression.terms.push_back(ParsedWordTerm{
                sign,
                signedPos,
                IntegerOperand{integer.negative ? -integer.value : integer.value,
                               integer.base,
                               false,
                               std::string{rawTerm},
                               termPos},
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
