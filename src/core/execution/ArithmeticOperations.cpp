#include "ArithmeticOperations.hpp"

#include "OperationSupport.hpp"

#include <cstdint>
#include <stdexcept>
#include <variant>

namespace {

std::int32_t toSigned24(std::uint32_t value) {
    value &= WordMask;
    if ((value & 0x800000U) != 0) {
        return static_cast<std::int32_t>(value | ~WordMask);
    }
    return static_cast<std::int32_t>(value);
}

std::uint32_t toUnsigned24(std::int64_t value) {
    return static_cast<std::uint32_t>(value) & WordMask;
}

ConditionCode compareSigned24(std::int32_t a, std::int32_t b) {
    if (a < b) {
        return ConditionCode::Less;
    }
    if (a == b) {
        return ConditionCode::Equal;
    }
    return ConditionCode::Greater;
}

} // namespace

ExecuteResult executeArithmeticOperation(const DecodedInstruction& instruction,
                                         const ResolvedOperand& operand,
                                         std::uint32_t postFetchPc,
                                         const Memory& memory,
                                         const RegisterFile& registers) {
    ExecutionEffect effect{postFetchPc};

    try {
        switch (instruction.opcode) {
            case Opcode::COMP: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t valA = toSigned24(registers.read(RegisterId::A));
                const std::int32_t valOp = toSigned24(*value);
                effect.conditionCode = compareSigned24(valA, valOp);
                break;
            }
            case Opcode::COMPR: {
                const auto* registerPair = std::get_if<RegisterPairOperands>(&operand);
                if (registerPair == nullptr) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t val1 = toSigned24(registers.read(registerPair->r1));
                const std::int32_t val2 = toSigned24(registers.read(registerPair->r2));
                effect.conditionCode = compareSigned24(val1, val2);
                break;
            }
            case Opcode::ADD:
            case Opcode::SUB: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t valA = toSigned24(registers.read(RegisterId::A));
                const std::int32_t valOp = toSigned24(*value);
                const std::int64_t result = (instruction.opcode == Opcode::ADD)
                                                ? static_cast<std::int64_t>(valA) + valOp
                                                : static_cast<std::int64_t>(valA) - valOp;
                effect.registerWrite = RegisterWrite{RegisterId::A, toUnsigned24(result)};
                break;
            }
            case Opcode::ADDR:
            case Opcode::SUBR: {
                const auto* registerPair = std::get_if<RegisterPairOperands>(&operand);
                if (registerPair == nullptr || registerPair->r2 == RegisterId::SW) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t val1 = toSigned24(registers.read(registerPair->r1));
                const std::int32_t val2 = toSigned24(registers.read(registerPair->r2));
                const std::int64_t result = (instruction.opcode == Opcode::ADDR)
                                                ? static_cast<std::int64_t>(val2) + val1
                                                : static_cast<std::int64_t>(val2) - val1;
                effect.registerWrite = RegisterWrite{registerPair->r2, toUnsigned24(result)};
                break;
            }
            case Opcode::MUL: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t valA = toSigned24(registers.read(RegisterId::A));
                const std::int32_t valOp = toSigned24(*value);
                const std::int64_t result = static_cast<std::int64_t>(valA) * valOp;
                effect.registerWrite = RegisterWrite{RegisterId::A, toUnsigned24(result)};
                break;
            }
            case Opcode::MULR: {
                const auto* registerPair = std::get_if<RegisterPairOperands>(&operand);
                if (registerPair == nullptr || registerPair->r2 == RegisterId::SW) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t val1 = toSigned24(registers.read(registerPair->r1));
                const std::int32_t val2 = toSigned24(registers.read(registerPair->r2));
                const std::int64_t result = static_cast<std::int64_t>(val2) * val1;
                effect.registerWrite = RegisterWrite{registerPair->r2, toUnsigned24(result)};
                break;
            }
            case Opcode::DIV: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t valOp = toSigned24(*value);
                if (valOp == 0) {
                    return ExecuteFault{
                        ExecuteFaultCode::DivisionByZero, instruction.instructionAddress, 0};
                }
                const std::int32_t valA = toSigned24(registers.read(RegisterId::A));
                const std::int64_t result = static_cast<std::int64_t>(valA) / valOp;
                effect.registerWrite = RegisterWrite{RegisterId::A, toUnsigned24(result)};
                break;
            }
            case Opcode::DIVR: {
                const auto* registerPair = std::get_if<RegisterPairOperands>(&operand);
                if (registerPair == nullptr || registerPair->r2 == RegisterId::SW) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::int32_t val1 = toSigned24(registers.read(registerPair->r1));
                if (val1 == 0) {
                    return ExecuteFault{
                        ExecuteFaultCode::DivisionByZero, instruction.instructionAddress, 0};
                }
                const std::int32_t val2 = toSigned24(registers.read(registerPair->r2));
                const std::int64_t result = static_cast<std::int64_t>(val2) / val1;
                effect.registerWrite = RegisterWrite{registerPair->r2, toUnsigned24(result)};
                break;
            }
            case Opcode::SHIFTL:
            case Opcode::SHIFTR: {
                const auto* regAndNibble = std::get_if<RegisterAndNibbleOperand>(&operand);
                if (regAndNibble == nullptr || regAndNibble->r1 == RegisterId::SW ||
                    regAndNibble->encodedNibble > 0x0F) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::uint32_t count =
                    static_cast<std::uint32_t>(regAndNibble->encodedNibble) + 1;
                const std::uint32_t val = registers.read(regAndNibble->r1) & WordMask;
                std::uint32_t result = 0;
                if (instruction.opcode == Opcode::SHIFTL) {
                    result = ((val << count) | (val >> (24 - count))) & WordMask;
                } else {
                    const std::int32_t signedVal = toSigned24(val);
                    const std::int32_t shifted = signedVal >> count;
                    result = static_cast<std::uint32_t>(shifted) & WordMask;
                }
                effect.registerWrite = RegisterWrite{regAndNibble->r1, result};
                break;
            }
            case Opcode::TIX: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::uint32_t currentX = registers.read(RegisterId::X);
                const std::uint32_t newX = (currentX + 1) & WordMask;
                const std::int32_t valX = toSigned24(newX);
                const std::int32_t valOp = toSigned24(*value);
                effect.registerWrite = RegisterWrite{RegisterId::X, newX};
                effect.conditionCode = compareSigned24(valX, valOp);
                break;
            }
            case Opcode::TIXR: {
                const auto* singleRegister = std::get_if<SingleRegisterOperand>(&operand);
                if (singleRegister == nullptr) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::uint32_t currentX = registers.read(RegisterId::X);
                const std::uint32_t newX = (currentX + 1) & WordMask;
                const std::int32_t valX = toSigned24(newX);
                const std::int32_t valR1 = (singleRegister->r1 == RegisterId::X)
                                               ? valX
                                               : toSigned24(registers.read(singleRegister->r1));
                effect.registerWrite = RegisterWrite{RegisterId::X, newX};
                effect.conditionCode = compareSigned24(valX, valR1);
                break;
            }

            default:
                return ExecuteFault{ExecuteFaultCode::UnsupportedOpcode,
                                    instruction.instructionAddress};
        }
    } catch (const std::out_of_range&) {
        return ExecuteFault{ExecuteFaultCode::MemoryAccessOutOfRange,
                            instruction.instructionAddress};
    }

    return effect;
}
