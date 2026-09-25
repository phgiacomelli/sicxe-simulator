#include "HexParser.hpp"

namespace {
bool whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
}
int digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
} // namespace

ProgramInputResult
HexParser::parse(std::string_view text, std::uint32_t loadAddress, std::uint32_t initialPc) {
    ProgramInput program{{}, loadAddress, initialPc};
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        if (whitespace(text[cursor])) {
            ++cursor;
            continue;
        }
        const auto start = cursor;
        while (cursor < text.size() && !whitespace(text[cursor])) {
            if (digit(text[cursor]) < 0)
                return InputFault{InputFaultCode::InvalidDigit, cursor};
            ++cursor;
        }
        const auto length = cursor - start;
        if (length < 2)
            return InputFault{InputFaultCode::IncompleteByte, start};
        if (length > 2)
            return InputFault{InputFaultCode::ByteOutOfRange, start};
        program.bytes.push_back(
            static_cast<std::uint8_t>(digit(text[start]) * 16 + digit(text[start + 1])));
    }
    if (program.bytes.empty())
        return InputFault{InputFaultCode::EmptyInput};
    return program;
}
