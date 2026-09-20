#pragma once

#include "Architecture.hpp"

#include <cstdint>
#include <optional>
#include <variant>

struct RegisterWrite {
    constexpr RegisterWrite(RegisterId target, std::uint32_t value)
        : target{target}, value{value & WordMask} {
    }

    RegisterId target;
    std::uint32_t value;

    bool operator==(const RegisterWrite&) const = default;
};

struct ByteWrite {
    std::uint32_t address{};
    std::uint8_t value{};

    bool operator==(const ByteWrite&) const = default;
};

struct WordWrite {
    constexpr WordWrite(std::uint32_t address, std::uint32_t value)
        : address{address}, value{value & WordMask} {
    }

    std::uint32_t address;
    std::uint32_t value;

    bool operator==(const WordWrite&) const = default;
};

using MemoryWrite = std::variant<ByteWrite, WordWrite>;

struct ExecutionEffect {
    explicit constexpr ExecutionEffect(std::uint32_t nextPc) : nextPc{nextPc} {
    }

    std::uint32_t nextPc;
    std::optional<RegisterWrite> registerWrite{};
    std::optional<MemoryWrite> memoryWrite{};
    std::optional<ConditionCode> conditionCode{};

    bool operator==(const ExecutionEffect&) const = default;
};

enum class ExecuteFaultCode {
    UnsupportedOpcode,
    InvalidOperand,
    DivisionByZero,
    MemoryAccessOutOfRange,
};

struct ExecuteFault {
    ExecuteFaultCode code{};
    std::uint32_t instructionAddress{};
    std::optional<std::int64_t> offendingValue{};

    bool operator==(const ExecuteFault&) const = default;
};

using ExecuteResult = std::variant<ExecutionEffect, ExecuteFault>;

constexpr std::optional<std::uint32_t> computePostFetchPc(std::uint32_t instructionAddress,
                                                          std::uint8_t instructionLength) {
    if (instructionLength == 0) {
        return std::nullopt;
    }

    const auto next = static_cast<std::uint64_t>(instructionAddress) + instructionLength;
    if (next > WordMask) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(next);
}
