#pragma once

#include "../ExecutionEffect.hpp"
#include "../Instruction.hpp"
#include "../Memory.hpp"
#include "../Operand.hpp"

#include <optional>
#include <variant>

namespace operation_support {

inline std::optional<std::uint32_t> readWordOperand(const ResolvedOperand& operand,
                                                    const Memory& memory) {
    if (const auto* immediate = std::get_if<ImmediateValue>(&operand)) {
        return immediate->value;
    }
    if (const auto* memoryAddress = std::get_if<MemoryAddress>(&operand)) {
        return memory.readWord(memoryAddress->address);
    }
    return std::nullopt;
}

inline ExecuteFault invalidOperand(const DecodedInstruction& instruction) {
    return ExecuteFault{ExecuteFaultCode::InvalidOperand, instruction.instructionAddress};
}

} // namespace operation_support
