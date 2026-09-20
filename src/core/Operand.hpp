#pragma once

#include "Architecture.hpp"

#include <cstdint>
#include <optional>
#include <variant>

struct NoOperand {
    bool operator==(const NoOperand&) const = default;
};

struct RegisterOperands {
    RegisterId r1{};
    std::optional<RegisterId> r2{};
    std::optional<std::uint8_t> encodedNibble{};

    bool operator==(const RegisterOperands&) const = default;
};

struct ImmediateValue {
    std::uint32_t value{};

    bool operator==(const ImmediateValue&) const = default;
};

struct MemoryAddress {
    std::uint32_t address{};

    bool operator==(const MemoryAddress&) const = default;
};

using ResolvedOperand = std::variant<NoOperand, RegisterOperands, ImmediateValue, MemoryAddress>;

enum class ResolveFaultCode {
    InvalidInstruction,
    InvalidRegister,
    AddressOutOfRange,
    IndirectReadOutOfRange,
};

struct ResolveFault {
    ResolveFaultCode code{};
    std::int64_t calculatedAddress{};

    bool operator==(const ResolveFault&) const = default;
};

using ResolveResult = std::variant<ResolvedOperand, ResolveFault>;
