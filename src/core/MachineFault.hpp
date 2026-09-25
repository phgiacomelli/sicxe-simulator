#pragma once

#include "ExecutionEffect.hpp"
#include "Instruction.hpp"
#include "Operand.hpp"

#include <cstdint>
#include <optional>
#include <variant>

enum class CycleFaultCode {
    PostFetchPcOutOfRange,
    NextPcOutOfRange,
    InvalidRegisterWrite,
    InvalidConditionCode,
    MemoryWriteOutOfRange,
    ConflictingEffect,
};

struct CycleFault {
    CycleFaultCode code{};
    std::optional<std::int64_t> offendingValue{};

    bool operator==(const CycleFault&) const = default;
};

struct MachineFault {
    std::uint32_t instructionAddress{};
    std::variant<DecodeFault, ResolveFault, ExecuteFault, CycleFault> cause{};
};

enum class LoadFaultCode {
    EmptyProgram,
    LoadAddressOutOfRange,
    ProgramExceedsMemory,
    InitialPcOutOfRange,
};

struct LoadFault {
    LoadFaultCode code{};
    std::optional<std::int64_t> offendingValue{};

    bool operator==(const LoadFault&) const = default;
};
