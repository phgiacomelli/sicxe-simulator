#include "Decoder.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace {
enum class Format2Operands {
    OneRegister,
    TwoRegisters,
    RegisterAndNibble,
};

struct Format2Spec {
    Opcode opcode;
    Format2Operands operands;
};

struct MemorySpec {
    Opcode opcode;
    bool sicCompatible;
};

constexpr std::array Format2Specs{
    Format2Spec{Opcode::ADDR, Format2Operands::TwoRegisters},
    Format2Spec{Opcode::CLEAR, Format2Operands::OneRegister},
    Format2Spec{Opcode::COMPR, Format2Operands::TwoRegisters},
    Format2Spec{Opcode::DIVR, Format2Operands::TwoRegisters},
    Format2Spec{Opcode::MULR, Format2Operands::TwoRegisters},
    Format2Spec{Opcode::RMO, Format2Operands::TwoRegisters},
    Format2Spec{Opcode::SHIFTL, Format2Operands::RegisterAndNibble},
    Format2Spec{Opcode::SHIFTR, Format2Operands::RegisterAndNibble},
    Format2Spec{Opcode::SUBR, Format2Operands::TwoRegisters},
    Format2Spec{Opcode::TIXR, Format2Operands::OneRegister},
};

constexpr std::array MemorySpecs{
    MemorySpec{Opcode::ADD, true},  MemorySpec{Opcode::AND, true},  MemorySpec{Opcode::COMP, true},
    MemorySpec{Opcode::DIV, true},  MemorySpec{Opcode::J, true},    MemorySpec{Opcode::JEQ, true},
    MemorySpec{Opcode::JGT, true},  MemorySpec{Opcode::JLT, true},  MemorySpec{Opcode::JSUB, true},
    MemorySpec{Opcode::LDA, true},  MemorySpec{Opcode::LDB, false}, MemorySpec{Opcode::LDCH, true},
    MemorySpec{Opcode::LDL, true},  MemorySpec{Opcode::LDS, false}, MemorySpec{Opcode::LDT, false},
    MemorySpec{Opcode::LDX, true},  MemorySpec{Opcode::MUL, true},  MemorySpec{Opcode::OR, true},
    MemorySpec{Opcode::RSUB, true}, MemorySpec{Opcode::STA, true},  MemorySpec{Opcode::STB, false},
    MemorySpec{Opcode::STCH, true}, MemorySpec{Opcode::STL, true},  MemorySpec{Opcode::STS, false},
    MemorySpec{Opcode::STT, false}, MemorySpec{Opcode::STX, true},  MemorySpec{Opcode::SUB, true},
    MemorySpec{Opcode::TIX, true},
};

template <typename Spec, std::size_t Size>
const Spec* findSpec(const std::array<Spec, Size>& specs, std::uint8_t machineCode) {
    for (const auto& spec : specs) {
        if (static_cast<std::uint8_t>(spec.opcode) == machineCode) {
            return &spec;
        }
    }
    return nullptr;
}

bool isValidRegister(std::uint8_t registerNumber) {
    return registerNumber <= 5 || registerNumber == 8 || registerNumber == 9;
}

DecodeFault makeFault(DecodeFaultCode code,
                      std::uint32_t address,
                      const std::array<std::uint8_t, 4>& rawBytes,
                      std::uint8_t bytesRead) {
    return DecodeFault{code, address, rawBytes, bytesRead};
}
} // namespace

DecodeResult Decoder::decode(const Memory& memory, std::uint32_t address) const {
    std::array<std::uint8_t, 4> rawBytes{};
    std::uint8_t bytesRead = 0;

    const auto readByte = [&](std::size_t offset) {
        try {
            rawBytes[offset] = memory.readByte(static_cast<std::size_t>(address) + offset);
            bytesRead = static_cast<std::uint8_t>(offset + 1);
            return true;
        } catch (const std::out_of_range&) {
            return false;
        }
    };

    if (!readByte(0)) {
        return makeFault(DecodeFaultCode::TruncatedInstruction, address, rawBytes, bytesRead);
    }

    if (const auto* spec = findSpec(Format2Specs, rawBytes[0]); spec != nullptr) {
        if (!readByte(1)) {
            return makeFault(DecodeFaultCode::TruncatedInstruction, address, rawBytes, bytesRead);
        }

        const auto r1 = static_cast<std::uint8_t>(rawBytes[1] >> 4);
        const auto r2 = static_cast<std::uint8_t>(rawBytes[1] & 0x0F);

        if (!isValidRegister(r1) ||
            (spec->operands == Format2Operands::TwoRegisters && !isValidRegister(r2))) {
            return makeFault(DecodeFaultCode::InvalidRegister, address, rawBytes, bytesRead);
        }
        if (spec->operands == Format2Operands::OneRegister && r2 != 0) {
            return makeFault(DecodeFaultCode::InvalidOperandEncoding, address, rawBytes, bytesRead);
        }

        DecodedInstruction instruction{};
        instruction.opcode = spec->opcode;
        instruction.format = InstructionFormat::Format2;
        instruction.instructionAddress = address;
        instruction.length = 2;
        instruction.rawBytes = rawBytes;
        instruction.r1 = r1;
        instruction.r2 = r2;
        return instruction;
    }

    const auto machineCode = static_cast<std::uint8_t>(rawBytes[0] & 0xFC);
    const auto* spec = findSpec(MemorySpecs, machineCode);
    if (spec == nullptr) {
        return makeFault(DecodeFaultCode::UnknownOpcode, address, rawBytes, bytesRead);
    }

    const auto n = (rawBytes[0] & 0x02) != 0;
    const auto i = (rawBytes[0] & 0x01) != 0;

    if (!readByte(1) || !readByte(2)) {
        return makeFault(DecodeFaultCode::TruncatedInstruction, address, rawBytes, bytesRead);
    }

    DecodedInstruction instruction{};
    instruction.opcode = spec->opcode;
    instruction.instructionAddress = address;
    instruction.rawBytes = rawBytes;
    instruction.n = n;
    instruction.i = i;

    if (!n && !i) {
        if (!spec->sicCompatible) {
            return makeFault(DecodeFaultCode::InvalidOperandEncoding, address, rawBytes, bytesRead);
        }

        instruction.format = InstructionFormat::Sic;
        instruction.length = 3;
        instruction.x = (rawBytes[1] & 0x80) != 0;
        instruction.encodedOperand =
            (static_cast<std::uint32_t>(rawBytes[1] & 0x7F) << 8) | rawBytes[2];
        if (instruction.opcode == Opcode::RSUB &&
            (instruction.x || instruction.encodedOperand != 0)) {
            return makeFault(DecodeFaultCode::InvalidOperandEncoding, address, rawBytes, bytesRead);
        }
        return instruction;
    }

    instruction.x = (rawBytes[1] & 0x80) != 0;
    instruction.b = (rawBytes[1] & 0x40) != 0;
    instruction.p = (rawBytes[1] & 0x20) != 0;
    instruction.e = (rawBytes[1] & 0x10) != 0;
    instruction.length = static_cast<std::uint8_t>(instruction.e ? 4 : 3);

    if (instruction.e && !readByte(3)) {
        return makeFault(DecodeFaultCode::TruncatedInstruction, address, rawBytes, bytesRead);
    }
    instruction.rawBytes = rawBytes;

    if ((instruction.b && instruction.p) || (instruction.e && (instruction.b || instruction.p)) ||
        (instruction.x && !(instruction.n && instruction.i))) {
        return makeFault(DecodeFaultCode::InvalidFlagCombination, address, rawBytes, bytesRead);
    }

    instruction.format = instruction.e ? InstructionFormat::Format4 : InstructionFormat::Format3;
    instruction.encodedOperand =
        (static_cast<std::uint32_t>(rawBytes[1] & 0x0F) << 8) | rawBytes[2];
    if (instruction.e) {
        instruction.encodedOperand = (instruction.encodedOperand << 8) | rawBytes[3];
    }

    if (instruction.opcode == Opcode::RSUB &&
        (!(instruction.n && instruction.i) || instruction.x || instruction.b || instruction.p ||
         instruction.e || instruction.encodedOperand != 0)) {
        return makeFault(DecodeFaultCode::InvalidOperandEncoding, address, rawBytes, bytesRead);
    }

    return instruction;
}
