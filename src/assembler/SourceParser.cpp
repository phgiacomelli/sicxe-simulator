#include "SourceParser.hpp"

#include <optional>
#include <string>
#include <utility>

namespace assembler {
namespace {

bool horizontalSpace(char character) {
    return character == ' ' || character == '\t';
}

bool letter(char character) {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

std::size_t skipSpaces(std::string_view text, std::size_t cursor) {
    while (cursor < text.size() && horizontalSpace(text[cursor])) {
        ++cursor;
    }
    return cursor;
}

std::size_t tokenEnd(std::string_view text, std::size_t cursor) {
    while (cursor < text.size() && !horizontalSpace(text[cursor])) {
        ++cursor;
    }
    return cursor;
}

std::optional<std::size_t> invalidIdentifier(std::string_view text) {
    if (text.empty() || !letter(text.front())) {
        return 0;
    }
    for (std::size_t index = 1; index < text.size(); ++index) {
        const char character = text[index];
        if (!letter(character) && !(character >= '0' && character <= '9') && character != '_') {
            return index;
        }
    }
    return std::nullopt;
}

SourceIdentifier identifier(std::string_view text, std::size_t line, std::size_t column) {
    std::string key{text};
    for (char& character : key) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return SourceIdentifier{SourceField{std::string{text}, SourcePosition{line, column}},
                            std::move(key)};
}

struct LineBoundary {
    std::size_t codeEnd{};
    std::optional<std::size_t> commentStart;
    std::optional<std::size_t> openQuote;
};

LineBoundary findBoundary(std::string_view text) {
    std::optional<std::size_t> openQuote;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '\'') {
            if (openQuote) {
                openQuote.reset();
            } else {
                openQuote = index;
            }
        } else if (text[index] == ';' && !openQuote) {
            return LineBoundary{index, index, std::nullopt};
        }
    }
    return LineBoundary{text.size(), std::nullopt, openQuote};
}

std::optional<std::size_t> unexpectedField(std::string_view operand) {
    bool quoted = false;
    std::size_t cursor = 0;
    while (cursor < operand.size()) {
        if (operand[cursor] == '\'') {
            quoted = !quoted;
        } else if (!quoted && horizontalSpace(operand[cursor])) {
            const std::size_t next = skipSpaces(operand, cursor);
            if (next < operand.size() && operand[cursor - 1] != ',' && operand[next] != ',') {
                return next;
            }
            cursor = next;
            continue;
        }
        ++cursor;
    }
    return std::nullopt;
}

SourceLine
parseLine(std::string_view text, std::size_t number, std::vector<AssemblyDiagnostic>& diagnostics) {
    SourceLine line{number, std::string{text}, SourceLineKind::Blank, std::nullopt, std::nullopt};
    std::size_t cursor = skipSpaces(text, 0);
    if (cursor == text.size()) {
        return line;
    }
    if (text[cursor] == '.') {
        line.kind = SourceLineKind::Comment;
        line.comment =
            SourceField{std::string{text.substr(cursor)}, SourcePosition{number, cursor + 1}};
        return line;
    }

    const auto boundary = findBoundary(text);
    if (boundary.commentStart) {
        const auto start = *boundary.commentStart;
        line.comment =
            SourceField{std::string{text.substr(start)}, SourcePosition{number, start + 1}};
    }
    const auto code = text.substr(0, boundary.codeEnd);
    if (cursor == code.size()) {
        line.kind = SourceLineKind::Comment;
        return line;
    }

    const auto reject =
        [&](AssemblyDiagnosticCode diagnosticCode, std::size_t column, std::string message) {
            line.kind = SourceLineKind::Invalid;
            diagnostics.push_back(AssemblyDiagnostic{
                diagnosticCode, SourcePosition{number, column}, std::move(message)});
            return std::move(line);
        };

    ParsedStatement statement;
    if (cursor == 0) {
        const auto end = tokenEnd(code, cursor);
        const auto label = code.substr(cursor, end - cursor);
        if (const auto invalid = invalidIdentifier(label)) {
            return reject(AssemblyDiagnosticCode::InvalidLabel,
                          cursor + *invalid + 1,
                          "Rotulo invalido; use uma letra seguida de letras, digitos ou '_'.");
        }
        statement.label = identifier(label, number, cursor + 1);
        cursor = skipSpaces(code, end);
    }

    if (cursor == code.size()) {
        return reject(AssemblyDiagnosticCode::MissingOperation,
                      cursor + 1,
                      "Operacao ausente depois do rotulo.");
    }
    const auto operationEnd = tokenEnd(code, cursor);
    const auto operation = code.substr(cursor, operationEnd - cursor);
    const std::size_t prefix = operation.front() == '+' ? 1 : 0;
    if (const auto invalid = invalidIdentifier(operation.substr(prefix))) {
        const auto offset = operation.size() == prefix ? 0 : prefix + *invalid;
        return reject(AssemblyDiagnosticCode::InvalidOperation,
                      cursor + offset + 1,
                      "Operacao invalida; use um identificador com prefixo '+' opcional.");
    }
    statement.operation = identifier(operation, number, cursor + 1);

    if (boundary.openQuote) {
        return reject(AssemblyDiagnosticCode::UnterminatedQuote,
                      *boundary.openQuote + 1,
                      "Aspas simples nao foram fechadas nesta linha.");
    }

    cursor = skipSpaces(code, operationEnd);
    if (cursor < code.size()) {
        std::size_t end = code.size();
        while (end > cursor && horizontalSpace(code[end - 1])) {
            --end;
        }
        const auto operand = code.substr(cursor, end - cursor);
        if (const auto extra = unexpectedField(operand)) {
            return reject(AssemblyDiagnosticCode::UnexpectedField,
                          cursor + *extra + 1,
                          "Campo extra depois do operando; use ';' para iniciar um comentario.");
        }
        statement.operand = SourceField{std::string{operand}, SourcePosition{number, cursor + 1}};
    }

    line.kind = SourceLineKind::Statement;
    line.statement = std::move(statement);
    return line;
}

} // namespace

SourceParseResult SourceParser::parse(std::string_view sourceText) {
    SourceParseResult result;
    std::size_t cursor = 0;
    std::size_t lineNumber = 1;
    while (cursor < sourceText.size()) {
        const auto separator = sourceText.find_first_of("\r\n", cursor);
        const auto end = separator == std::string_view::npos ? sourceText.size() : separator;
        result.lines.push_back(
            parseLine(sourceText.substr(cursor, end - cursor), lineNumber, result.diagnostics));
        cursor = end;
        if (cursor < sourceText.size()) {
            const char terminator = sourceText[cursor++];
            if (terminator == '\r' && cursor < sourceText.size() && sourceText[cursor] == '\n') {
                ++cursor;
            }
        }
        ++lineNumber;
    }
    return result;
}

} // namespace assembler
