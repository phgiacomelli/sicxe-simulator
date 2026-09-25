#pragma once

#include "ExecutionEffect.hpp"
#include "Instruction.hpp"
#include "Memory.hpp"
#include "Operand.hpp"
#include "RegisterFile.hpp"

#include <cstdint>

class Executor {
public:
    Executor() = default;

    ExecuteResult execute(const DecodedInstruction& instruction,
                          const ResolvedOperand& operand,
                          std::uint32_t postFetchPc,
                          const Memory& memory,
                          const RegisterFile& registers) const;
};
