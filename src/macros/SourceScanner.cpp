#include "SourceScanner.hpp"

#include <utility>

namespace macro {
namespace {

bool horizontalSpace(char value) {
    return value == ' ' || value == '\t';
}

std::size_t skipSpaces(std::string_view text, std::size_t cursor) {
    while (cursor < text.size() && horizontalSpace(text[cursor]))
        ++cursor;
    return cursor;
}

std::size_t tokenEnd(std::string_view text, std::size_t cursor) {
    while (cursor < text.size() && !horizontalSpace(text[cursor]))
        ++cursor;
    return cursor;
}

SourceField field(std::string_view text, std::size_t line, std::size_t start, std::size_t end) {
    return {std::string{text.substr(start, end - start)}, {line, start + 1}};
}

SourceLine scanLine(std::string_view text, std::size_t number) {
    SourceLine line;
    line.number = number;
    line.text = text;
    std::size_t cursor = skipSpaces(text, 0);
    if (cursor == text.size())
        return line;
    if (text[cursor] == '.') {
        line.kind = SourceLineKind::Comment;
        line.comment = field(text, number, cursor, text.size());
        return line;
    }
    for (std::size_t index = cursor; index < text.size(); ++index) {
        if (text[index] == '\'') {
            if (line.openQuote)
                line.openQuote.reset();
            else
                line.openQuote = SourcePosition{number, index + 1};
        } else if (text[index] == ';' && !line.openQuote) {
            line.comment = field(text, number, index, text.size());
            text = text.substr(0, index);
            break;
        }
    }
    if (cursor == text.size()) {
        line.kind = SourceLineKind::Comment;
        return line;
    }
    line.kind = SourceLineKind::Code;
    if (cursor == 0) {
        const auto end = tokenEnd(text, cursor);
        line.label = field(text, number, cursor, end);
        cursor = skipSpaces(text, end);
    }
    if (cursor < text.size()) {
        const auto end = tokenEnd(text, cursor);
        line.operation = field(text, number, cursor, end);
        cursor = skipSpaces(text, end);
    }
    if (cursor < text.size()) {
        auto end = text.size();
        while (end > cursor && horizontalSpace(text[end - 1]))
            --end;
        line.operand = field(text, number, cursor, end);
    }
    return line;
}

} // namespace

std::string SourceScanResult::normalizedText() const {
    std::string text;
    for (const auto& line : lines) {
        text += line.text;
        if (line.terminated)
            text += '\n';
    }
    return text;
}

SourceScanResult SourceScanner::scan(std::string_view sourceText) {
    SourceScanResult result;
    std::size_t cursor = 0;
    while (cursor < sourceText.size()) {
        const auto separator = sourceText.find_first_of("\r\n", cursor);
        const auto end = separator == std::string_view::npos ? sourceText.size() : separator;
        auto line = scanLine(sourceText.substr(cursor, end - cursor), result.lines.size() + 1);
        line.terminated = end < sourceText.size();
        result.lines.push_back(std::move(line));
        cursor = end;
        if (cursor < sourceText.size()) {
            const auto terminator = sourceText[cursor++];
            if (terminator == '\r' && cursor < sourceText.size() && sourceText[cursor] == '\n')
                ++cursor;
        }
    }
    return result;
}

} // namespace macro
