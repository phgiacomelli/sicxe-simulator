#pragma once

#include "AddressResolver.hpp"
#include "Architecture.hpp"
#include "Decoder.hpp"
#include "ExecutionEffect.hpp"
#include "Executor.hpp"
#include "Instruction.hpp"
#include "MachineFault.hpp"
#include "Memory.hpp"
#include "RegisterFile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

struct LoadSuccess {
    std::uint32_t loadAddress{};
    std::size_t byteCount{};
    std::uint32_t initialPc{};

    bool operator==(const LoadSuccess&) const = default;
};

using LoadResult = std::variant<LoadSuccess, LoadFault>;

struct StepSuccess {
    DecodedInstruction instruction;
    ExecutionEffect effect;
    std::uint32_t nextPc;
};

using StepResult = std::variant<StepSuccess, MachineFault>;

struct RegisterSnapshot {
    std::uint32_t a{};
    std::uint32_t x{};
    std::uint32_t l{};
    std::uint32_t b{};
    std::uint32_t s{};
    std::uint32_t t{};
    std::uint64_t f{};
    std::uint32_t pc{};
    std::uint32_t sw{};
    std::optional<ConditionCode> conditionCode{};

    bool operator==(const RegisterSnapshot&) const = default;
};

struct MachineSnapshot {
    RegisterSnapshot registers{};
    std::vector<std::uint8_t> memory{};
    std::optional<DecodedInstruction> currentInstruction{};
    std::optional<MachineFault> lastFault{};
};

class Machine {
private:
    Memory memory_;
    RegisterFile registers_;
    Decoder decoder_;
    AddressResolver addressResolver_;
    Executor executor_;
    std::optional<MachineFault> lastFault_;

    StepResult fail(MachineFault fault);
    std::uint32_t commit(const ExecutionEffect& effect);

public:
    static constexpr std::size_t DefaultMemorySize = 1U << 20;

    explicit Machine(std::size_t memorySize = DefaultMemorySize);

    LoadResult loadProgram(std::span<const std::uint8_t> bytes,
                           std::uint32_t loadAddress,
                           std::uint32_t initialPc);
    void reset();
    StepResult step();
    MachineSnapshot snapshot() const;
};
