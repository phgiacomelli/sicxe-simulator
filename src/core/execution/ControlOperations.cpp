#include "ControlOperations.hpp"

#include "OperationSupport.hpp"

#include <optional>
#include <stdexcept>
#include <variant>

namespace {

std::optional<std::uint32_t> jumpTarget(const ResolvedOperand& operand) {
    if (const auto* memoryAddress = std::get_if<MemoryAddress>(&operand)) {
        return memoryAddress->address;
    }
    if (const auto* immediate = std::get_if<ImmediateValue>(&operand)) {
        return immediate->value;
    }
    return std::nullopt;
}

std::optional<ConditionCode> requiredConditionCode(Opcode opcode) {
    switch (opcode) {
        case Opcode::JEQ:
            return ConditionCode::Equal;
        case Opcode::JGT:
            return ConditionCode::Greater;
        case Opcode::JLT:
            return ConditionCode::Less;
        default:
            return std::nullopt;
    }
}

std::optional<ConditionCode> currentConditionCode(const RegisterFile& registers) {
    try {
        return registers.getConditionCode();
    } catch (const std::logic_error&) {
        return std::nullopt;
    }
}

ExecuteFault makeFault(ExecuteFaultCode code,
                       const DecodedInstruction& instruction,
                       std::optional<std::int64_t> offendingValue = std::nullopt) {
    return ExecuteFault{code, instruction.instructionAddress, offendingValue};
}

} // namespace

ExecuteResult executeControlOperation(const DecodedInstruction& instruction,
                                      const ResolvedOperand& operand,
                                      std::uint32_t postFetchPc,
                                      [[maybe_unused]] const Memory& memory,
                                      const RegisterFile& registers) {
    ExecutionEffect effect{postFetchPc};

    switch (instruction.opcode) {
        case Opcode::RSUB:
            if (!std::holds_alternative<NoOperand>(operand)) {
                return operation_support::invalidOperand(instruction);
            }
            effect.nextPc = registers.read(RegisterId::L);
            return effect;
        case Opcode::J:
        case Opcode::JEQ:
        case Opcode::JGT:
        case Opcode::JLT:
        case Opcode::JSUB:
            break;
        default:
            return makeFault(ExecuteFaultCode::UnsupportedOpcode, instruction);
    }

    const auto target = jumpTarget(operand);
    if (!target) {
        return operation_support::invalidOperand(instruction);
    }

    if (const auto required = requiredConditionCode(instruction.opcode)) {
        const auto current = currentConditionCode(registers);
        if (!current) {
            const std::int64_t rawConditionCode = registers.getSW() & ConditionCodeMask;
            return makeFault(ExecuteFaultCode::InvalidOperand, instruction, rawConditionCode);
        }
        if (*current == *required) {
            effect.nextPc = *target;
        }
        return effect;
    }

    if (instruction.opcode == Opcode::JSUB) {
        effect.registerWrite = RegisterWrite{RegisterId::L, postFetchPc};
    }
    effect.nextPc = *target;
    return effect;
}
