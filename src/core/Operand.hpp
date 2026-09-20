#pragma once

#include "Architecture.hpp"

#include <cstdint>
#include <optional>
#include <variant>

struct NoOperand {
    bool operator==(const NoOperand&) const = default;
};

struct SingleRegisterOperand {
    RegisterId r1{};

    bool operator==(const SingleRegisterOperand&) const = default;
};

struct RegisterPairOperands {
    RegisterId r1{};
    RegisterId r2{};

    bool operator==(const RegisterPairOperands&) const = default;
};

struct RegisterAndNibbleOperand {
    RegisterId r1{};
    std::uint8_t encodedNibble{};

    bool operator==(const RegisterAndNibbleOperand&) const = default;
};

struct ImmediateValue {
    std::uint32_t value{};

    bool operator==(const ImmediateValue&) const = default;
};

struct MemoryAddress {
    std::uint32_t address{};

    bool operator==(const MemoryAddress&) const = default;
};

using ResolvedOperand = std::variant<NoOperand,
                                     SingleRegisterOperand,
                                     RegisterPairOperands,
                                     RegisterAndNibbleOperand,
                                     ImmediateValue,
                                     MemoryAddress>;

enum class ResolveFaultCode {
    InvalidInstruction,
    InvalidRegister,
    AddressOutOfRange,
    IndirectReadOutOfRange,
};

struct ResolveFault {
    ResolveFaultCode code{};
    std::optional<std::int64_t> offendingValue{};

    bool operator==(const ResolveFault&) const = default;
};

using ResolveResult = std::variant<ResolvedOperand, ResolveFault>;
