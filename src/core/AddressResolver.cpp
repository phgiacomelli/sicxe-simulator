#include "AddressResolver.hpp"

#include <cstdint>
#include <utility>

namespace {
ResolveResult resolved(ResolvedOperand operand) {
    return ResolveResult{std::move(operand)};
}

ResolveResult fault(ResolveFaultCode code,
                    std::optional<std::int64_t> offendingValue = std::nullopt) {
    return ResolveResult{ResolveFault{code, offendingValue}};
}

bool isAddressInMemory(std::int64_t address, const Memory& memory) {
    return address >= 0 && static_cast<std::uint64_t>(address) < memory.size();
}

ResolveResult resolveFormat2(const DecodedInstruction& instruction) {
    const auto r1 = registerIdFromCode(instruction.r1);
    if (!r1.has_value()) {
        return fault(ResolveFaultCode::InvalidRegister, instruction.r1);
    }

    switch (instruction.opcode) {
        case Opcode::CLEAR:
        case Opcode::TIXR:
            return resolved(ResolvedOperand{SingleRegisterOperand{*r1}});
        case Opcode::SHIFTL:
        case Opcode::SHIFTR:
            return resolved(ResolvedOperand{RegisterAndNibbleOperand{*r1, instruction.r2}});
        case Opcode::ADDR:
        case Opcode::COMPR:
        case Opcode::DIVR:
        case Opcode::MULR:
        case Opcode::RMO:
        case Opcode::SUBR: {
            const auto r2 = registerIdFromCode(instruction.r2);
            if (!r2.has_value()) {
                return fault(ResolveFaultCode::InvalidRegister, instruction.r2);
            }
            return resolved(ResolvedOperand{RegisterPairOperands{*r1, *r2}});
        }
        default:
            return fault(ResolveFaultCode::InvalidInstruction);
    }
}

std::int64_t signExtend12(std::uint32_t encodedOperand) {
    auto displacement = static_cast<std::int64_t>(encodedOperand);
    if ((encodedOperand & 0x800U) != 0) {
        displacement -= 0x1000;
    }
    return displacement;
}
} // namespace

ResolveResult AddressResolver::resolve(const DecodedInstruction& instruction,
                                       std::uint32_t postFetchPc,
                                       const Memory& memory,
                                       const RegisterFile& registers) const {
    if (instruction.opcode == Opcode::RSUB) {
        if (instruction.format != InstructionFormat::Sic &&
            instruction.format != InstructionFormat::Format3) {
            return fault(ResolveFaultCode::InvalidInstruction);
        }
        return resolved(ResolvedOperand{NoOperand{}});
    }

    if (instruction.format == InstructionFormat::Format2) {
        return resolveFormat2(instruction);
    }

    std::int64_t address = 0;
    switch (instruction.format) {
        case InstructionFormat::Sic:
            if (instruction.encodedOperand > 0x7FFFU) {
                return fault(ResolveFaultCode::InvalidInstruction);
            }
            address = instruction.encodedOperand;
            break;
        case InstructionFormat::Format3:
            if (instruction.encodedOperand > 0xFFFU || (!instruction.n && !instruction.i)) {
                return fault(ResolveFaultCode::InvalidInstruction);
            }
            if (instruction.p) {
                address = static_cast<std::int64_t>(postFetchPc) +
                          signExtend12(instruction.encodedOperand);
            } else if (instruction.b) {
                address = static_cast<std::int64_t>(registers.getB()) + instruction.encodedOperand;
            } else {
                address = instruction.encodedOperand;
            }
            break;
        case InstructionFormat::Format4:
            if (instruction.encodedOperand > 0xFFFFFU || (!instruction.n && !instruction.i)) {
                return fault(ResolveFaultCode::InvalidInstruction);
            }
            address = instruction.encodedOperand;
            break;
        case InstructionFormat::Format2:
            return fault(ResolveFaultCode::InvalidInstruction);
    }

    if (instruction.x) {
        address += registers.getX();
    }

    if (!isAddressInMemory(address, memory)) {
        return fault(ResolveFaultCode::AddressOutOfRange, address);
    }

    if (!instruction.n && instruction.i) {
        return resolved(ResolvedOperand{ImmediateValue{static_cast<std::uint32_t>(address)}});
    }

    if (instruction.n && !instruction.i) {
        const auto pointerAddress = static_cast<std::size_t>(address);
        if (memory.size() < 3 || pointerAddress > memory.size() - 3) {
            return fault(ResolveFaultCode::IndirectReadOutOfRange, address);
        }

        const auto indirectAddress = memory.readWord(pointerAddress);
        if (!isAddressInMemory(indirectAddress, memory)) {
            return fault(ResolveFaultCode::AddressOutOfRange, indirectAddress);
        }
        return resolved(ResolvedOperand{MemoryAddress{indirectAddress}});
    }

    return resolved(ResolvedOperand{MemoryAddress{static_cast<std::uint32_t>(address)}});
}
