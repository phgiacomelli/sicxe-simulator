#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

struct ProgramInput {
    std::vector<std::uint8_t> bytes;
    std::uint32_t loadAddress{};
    std::uint32_t initialPc{};

    bool operator==(const ProgramInput&) const = default;
};

enum class InputFaultCode {
    EmptyInput,
    InvalidDigit,
    IncompleteByte,
    ByteOutOfRange,
    OpenFailed,
    ReadFailed
};

struct InputFault {
    InputFaultCode code{};
    // Zero-based byte offset in hexadecimal text, absent for file errors.
    std::optional<std::size_t> offset{};

    bool operator==(const InputFault&) const = default;
};

using ProgramInputResult = std::variant<ProgramInput, InputFault>;
