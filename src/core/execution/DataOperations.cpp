#include "DataOperations.hpp"

#include "OperationSupport.hpp"

#include <optional>
#include <stdexcept>
#include <variant>

namespace {

std::optional<RegisterId> dataRegisterFor(Opcode opcode) {
    switch (opcode) {
        case Opcode::LDA:
        case Opcode::STA:
            return RegisterId::A;
        case Opcode::LDB:
        case Opcode::STB:
            return RegisterId::B;
        case Opcode::LDL:
        case Opcode::STL:
            return RegisterId::L;
        case Opcode::LDS:
        case Opcode::STS:
            return RegisterId::S;
        case Opcode::LDT:
        case Opcode::STT:
            return RegisterId::T;
        case Opcode::LDX:
        case Opcode::STX:
            return RegisterId::X;
        default:
            return std::nullopt;
    }
}

} // namespace

ExecuteResult executeDataOperation(const DecodedInstruction& instruction,
                                   const ResolvedOperand& operand,
                                   std::uint32_t postFetchPc,
                                   const Memory& memory,
                                   const RegisterFile& registers) {
    ExecutionEffect effect{postFetchPc};

    try {
        switch (instruction.opcode) {
            case Opcode::CLEAR: {
                const auto* singleRegister = std::get_if<SingleRegisterOperand>(&operand);
                if (singleRegister == nullptr || singleRegister->r1 == RegisterId::SW) {
                    return operation_support::invalidOperand(instruction);
                }
                effect.registerWrite = RegisterWrite{singleRegister->r1, 0x000000};
                break;
            }
            case Opcode::RMO: {
                const auto* registerPair = std::get_if<RegisterPairOperands>(&operand);
                if (registerPair == nullptr || registerPair->r2 == RegisterId::SW) {
                    return operation_support::invalidOperand(instruction);
                }
                const std::uint32_t sourceValue = registers.read(registerPair->r1);
                effect.registerWrite = RegisterWrite{registerPair->r2, sourceValue};
                break;
            }
            case Opcode::LDA:
            case Opcode::LDB:
            case Opcode::LDL:
            case Opcode::LDS:
            case Opcode::LDT:
            case Opcode::LDX: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }

                effect.registerWrite = RegisterWrite{*dataRegisterFor(instruction.opcode), *value};
                break;
            }
            case Opcode::STA:
            case Opcode::STB:
            case Opcode::STL:
            case Opcode::STS:
            case Opcode::STT:
            case Opcode::STX: {
                const auto* memoryAddress = std::get_if<MemoryAddress>(&operand);
                if (memoryAddress == nullptr) {
                    return operation_support::invalidOperand(instruction);
                }

                const RegisterId sourceRegister = *dataRegisterFor(instruction.opcode);
                const std::uint32_t sourceValue = registers.read(sourceRegister);
                effect.memoryWrite = WordWrite{memoryAddress->address, sourceValue};
                break;
            }
            case Opcode::LDCH: {
                std::uint32_t byteVal;
                if (auto* imm = std::get_if<ImmediateValue>(&operand)) {
                    byteVal = imm->value & 0xFF;
                } else if (auto* mem = std::get_if<MemoryAddress>(&operand)) {
                    byteVal = memory.readByte(mem->address);
                } else {
                    return operation_support::invalidOperand(instruction);
                }

                std::uint32_t currentA = registers.read(RegisterId::A);
                std::uint32_t newA = (currentA & 0xFFFF00) | byteVal;
                effect.registerWrite = RegisterWrite{RegisterId::A, newA};
                break;
            }
            case Opcode::STCH: {
                if (auto* mem = std::get_if<MemoryAddress>(&operand)) {
                    std::uint32_t currentA = registers.read(RegisterId::A);
                    std::uint8_t byteVal = static_cast<std::uint8_t>(currentA & 0xFF);
                    effect.memoryWrite = ByteWrite{mem->address, byteVal};
                } else {
                    return operation_support::invalidOperand(instruction);
                }
                break;
            }
            case Opcode::AND:
            case Opcode::OR: {
                const auto value = operation_support::readWordOperand(operand, memory);
                if (!value) {
                    return operation_support::invalidOperand(instruction);
                }

                const std::uint32_t currentA = registers.read(RegisterId::A);
                std::uint32_t newA =
                    (instruction.opcode == Opcode::AND) ? (currentA & *value) : (currentA | *value);
                newA &= WordMask;

                effect.registerWrite = RegisterWrite{RegisterId::A, newA};
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
