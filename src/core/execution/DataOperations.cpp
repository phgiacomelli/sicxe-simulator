#include "DataOperations.hpp"

#include <stdexcept>
#include <variant>

static std::optional<std::uint32_t> readOperandWord(const ResolvedOperand& operand,
                                                    const Memory& memory) {
    if (auto* imm = std::get_if<ImmediateValue>(&operand)) {
        return imm->value;
    }
    if (auto* mem = std::get_if<MemoryAddress>(&operand)) {
        return memory.readWord(mem->address);
    }
    return std::nullopt;
}

ExecuteResult executeDataOperation(const DecodedInstruction& instruction,
                                   const ResolvedOperand& operand,
                                   std::uint32_t postFetchPc,
                                   const Memory& memory,
                                   const RegisterFile& registers) {
    ExecutionEffect effect{postFetchPc};

    try {
        switch (instruction.opcode) {
            case Opcode::CLEAR: {
                if (auto* singleReg = std::get_if<SingleRegisterOperand>(&operand)) {
                    effect.registerWrite = RegisterWrite{singleReg->r1, 0x000000};
                } else {
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};
                }
                break;
            }
            case Opcode::RMO: {
                if (auto* regPair = std::get_if<RegisterPairOperands>(&operand)) {
                    std::uint32_t val1 = registers.read(regPair->r1);
                    effect.registerWrite = RegisterWrite{regPair->r2, val1};
                } else {
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};
                }
                break;
            }
            case Opcode::LDA:
            case Opcode::LDB:
            case Opcode::LDL:
            case Opcode::LDS:
            case Opcode::LDT:
            case Opcode::LDX: {
                auto val = readOperandWord(operand, memory);
                if (!val)
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};

                RegisterId targetReg;
                if (instruction.opcode == Opcode::LDA)
                    targetReg = RegisterId::A;
                else if (instruction.opcode == Opcode::LDB)
                    targetReg = RegisterId::B;
                else if (instruction.opcode == Opcode::LDL)
                    targetReg = RegisterId::L;
                else if (instruction.opcode == Opcode::LDS)
                    targetReg = RegisterId::S;
                else if (instruction.opcode == Opcode::LDT)
                    targetReg = RegisterId::T;
                else
                    targetReg = RegisterId::X;

                effect.registerWrite = RegisterWrite{targetReg, *val};
                break;
            }
            case Opcode::STA:
            case Opcode::STB:
            case Opcode::STL:
            case Opcode::STS:
            case Opcode::STT:
            case Opcode::STX: {
                if (auto* mem = std::get_if<MemoryAddress>(&operand)) {
                    RegisterId srcReg;
                    if (instruction.opcode == Opcode::STA)
                        srcReg = RegisterId::A;
                    else if (instruction.opcode == Opcode::STB)
                        srcReg = RegisterId::B;
                    else if (instruction.opcode == Opcode::STL)
                        srcReg = RegisterId::L;
                    else if (instruction.opcode == Opcode::STS)
                        srcReg = RegisterId::S;
                    else if (instruction.opcode == Opcode::STT)
                        srcReg = RegisterId::T;
                    else
                        srcReg = RegisterId::X;

                    std::uint32_t val = registers.read(srcReg);
                    effect.memoryWrite = WordWrite{mem->address, val};
                } else {
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};
                }
                break;
            }
            case Opcode::LDCH: {
                std::uint32_t byteVal;
                if (auto* imm = std::get_if<ImmediateValue>(&operand)) {
                    byteVal = imm->value & 0xFF;
                } else if (auto* mem = std::get_if<MemoryAddress>(&operand)) {
                    byteVal = memory.readByte(mem->address);
                } else {
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};
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
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};
                }
                break;
            }
            case Opcode::AND:
            case Opcode::OR: {
                auto val = readOperandWord(operand, memory);
                if (!val)
                    return ExecuteFault{ExecuteFaultCode::InvalidOperand,
                                        instruction.instructionAddress};

                std::uint32_t currentA = registers.read(RegisterId::A);
                std::uint32_t newA =
                    (instruction.opcode == Opcode::AND) ? (currentA & *val) : (currentA | *val);
                newA &= 0xFFFFFF;

                effect.registerWrite = RegisterWrite{RegisterId::A, newA};
                break;
            }
            default:
                return ExecuteFault{ExecuteFaultCode::UnsupportedOpcode,
                                    instruction.instructionAddress};
        }
    } catch (const std::exception&) {
        return ExecuteFault{ExecuteFaultCode::MemoryAccessOutOfRange,
                            instruction.instructionAddress};
    }

    return effect;
}