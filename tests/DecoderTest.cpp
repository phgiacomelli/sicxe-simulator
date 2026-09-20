#include "core/Decoder.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <utility>
#include <variant>

namespace {
void writeBytes(Memory& memory, std::size_t address, std::initializer_list<std::uint8_t> bytes) {
    auto offset = std::size_t{0};
    for (const auto byte : bytes) {
        memory.writeByte(address + offset, byte);
        ++offset;
    }
}
} // namespace

TEST(DecoderTest, DecodesEveryRequiredFormat2Opcode) {
    constexpr std::array instructions{
        std::pair{0x90, Opcode::ADDR},
        std::pair{0xB4, Opcode::CLEAR},
        std::pair{0xA0, Opcode::COMPR},
        std::pair{0x9C, Opcode::DIVR},
        std::pair{0x98, Opcode::MULR},
        std::pair{0xAC, Opcode::RMO},
        std::pair{0xA4, Opcode::SHIFTL},
        std::pair{0xA8, Opcode::SHIFTR},
        std::pair{0x94, Opcode::SUBR},
        std::pair{0xB8, Opcode::TIXR},
    };

    for (const auto& [machineCode, expectedOpcode] : instructions) {
        Memory memory(2);
        const auto operands =
            expectedOpcode == Opcode::CLEAR || expectedOpcode == Opcode::TIXR ? 0x10 : 0x12;
        writeBytes(memory,
                   0,
                   {static_cast<std::uint8_t>(machineCode), static_cast<std::uint8_t>(operands)});

        const auto result = Decoder{}.decode(memory, 0);

        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
        EXPECT_EQ(std::get<DecodedInstruction>(result).opcode, expectedOpcode);
    }
}

TEST(DecoderTest, DecodesEveryRequiredMemoryOpcode) {
    constexpr std::array instructions{
        std::pair{0x18, Opcode::ADD},  std::pair{0x40, Opcode::AND}, std::pair{0x28, Opcode::COMP},
        std::pair{0x24, Opcode::DIV},  std::pair{0x3C, Opcode::J},   std::pair{0x30, Opcode::JEQ},
        std::pair{0x34, Opcode::JGT},  std::pair{0x38, Opcode::JLT}, std::pair{0x48, Opcode::JSUB},
        std::pair{0x00, Opcode::LDA},  std::pair{0x68, Opcode::LDB}, std::pair{0x50, Opcode::LDCH},
        std::pair{0x08, Opcode::LDL},  std::pair{0x6C, Opcode::LDS}, std::pair{0x74, Opcode::LDT},
        std::pair{0x04, Opcode::LDX},  std::pair{0x20, Opcode::MUL}, std::pair{0x44, Opcode::OR},
        std::pair{0x4C, Opcode::RSUB}, std::pair{0x0C, Opcode::STA}, std::pair{0x78, Opcode::STB},
        std::pair{0x54, Opcode::STCH}, std::pair{0x14, Opcode::STL}, std::pair{0x7C, Opcode::STS},
        std::pair{0x84, Opcode::STT},  std::pair{0x10, Opcode::STX}, std::pair{0x1C, Opcode::SUB},
        std::pair{0x2C, Opcode::TIX},
    };

    for (const auto& [machineCode, expectedOpcode] : instructions) {
        Memory memory(3);
        writeBytes(memory, 0, {static_cast<std::uint8_t>(machineCode | 0x03), 0x00, 0x00});

        const auto result = Decoder{}.decode(memory, 0);

        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
        EXPECT_EQ(std::get<DecodedInstruction>(result).opcode, expectedOpcode);
    }
}

TEST(DecoderTest, DecodesOneRegisterFormat2) {
    Memory memory(2);
    writeBytes(memory, 0, {0xB4, 0x10});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.opcode, Opcode::CLEAR);
    EXPECT_EQ(instruction.format, InstructionFormat::Format2);
    EXPECT_EQ(instruction.length, 2);
    EXPECT_EQ(instruction.r1, 1);
    EXPECT_EQ(instruction.r2, 0);
}

TEST(DecoderTest, DecodesTwoRegisterFormat2) {
    Memory memory(2);
    writeBytes(memory, 0, {0x90, 0x04});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.opcode, Opcode::ADDR);
    EXPECT_EQ(instruction.r1, 0);
    EXPECT_EQ(instruction.r2, 4);
}

TEST(DecoderTest, DecodesImmediateFormat3WithoutResolvingItsValue) {
    Memory memory(3);
    writeBytes(memory, 0, {0x01, 0x00, 0x05});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.opcode, Opcode::LDA);
    EXPECT_EQ(instruction.format, InstructionFormat::Format3);
    EXPECT_EQ(instruction.length, 3);
    EXPECT_FALSE(instruction.n);
    EXPECT_TRUE(instruction.i);
    EXPECT_FALSE(instruction.x);
    EXPECT_FALSE(instruction.b);
    EXPECT_FALSE(instruction.p);
    EXPECT_FALSE(instruction.e);
    EXPECT_EQ(instruction.encodedOperand, 0x005);
}

TEST(DecoderTest, DecodesPcRelativeFormat3WithoutApplyingPc) {
    Memory memory(32);
    writeBytes(memory, 10, {0x03, 0x20, 0x0A});

    const auto result = Decoder{}.decode(memory, 10);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.instructionAddress, 10);
    EXPECT_TRUE(instruction.p);
    EXPECT_EQ(instruction.encodedOperand, 0x00A);
}

TEST(DecoderTest, DecodesFormat4Address) {
    Memory memory(4);
    writeBytes(memory, 0, {0x4B, 0x10, 0x10, 0x00});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.opcode, Opcode::JSUB);
    EXPECT_EQ(instruction.format, InstructionFormat::Format4);
    EXPECT_EQ(instruction.length, 4);
    EXPECT_TRUE(instruction.e);
    EXPECT_EQ(instruction.encodedOperand, 0x01000);
}

TEST(DecoderTest, DecodesEveryAddressingCombinationAllowedByTheSpecification) {
    struct Case {
        std::array<std::uint8_t, 4> bytes;
        InstructionFormat format;
        std::uint8_t length;
    };

    constexpr std::array cases{
        Case{{0x03, 0x00, 0x01, 0x00}, InstructionFormat::Format3, 3}, // simple/direct
        Case{{0x03, 0x80, 0x01, 0x00}, InstructionFormat::Format3, 3}, // simple/indexed
        Case{{0x03, 0x20, 0x01, 0x00}, InstructionFormat::Format3, 3}, // simple/PC
        Case{{0x03, 0x40, 0x01, 0x00}, InstructionFormat::Format3, 3}, // simple/base
        Case{{0x03, 0x90, 0x00, 0x01}, InstructionFormat::Format4, 4}, // simple/extended/indexed
        Case{{0x02, 0x00, 0x01, 0x00}, InstructionFormat::Format3, 3}, // indirect/direct
        Case{{0x02, 0x20, 0x01, 0x00}, InstructionFormat::Format3, 3}, // indirect/PC
        Case{{0x02, 0x40, 0x01, 0x00}, InstructionFormat::Format3, 3}, // indirect/base
        Case{{0x02, 0x10, 0x00, 0x01}, InstructionFormat::Format4, 4}, // indirect/extended
        Case{{0x01, 0x00, 0x01, 0x00}, InstructionFormat::Format3, 3}, // immediate/direct
        Case{{0x01, 0x20, 0x01, 0x00}, InstructionFormat::Format3, 3}, // immediate/PC
        Case{{0x01, 0x40, 0x01, 0x00}, InstructionFormat::Format3, 3}, // immediate/base
        Case{{0x01, 0x10, 0x00, 0x01}, InstructionFormat::Format4, 4}, // immediate/extended
    };

    for (const auto& testCase : cases) {
        Memory memory(testCase.length);
        for (std::size_t index = 0; index < testCase.length; ++index) {
            memory.writeByte(index, testCase.bytes[index]);
        }

        const auto result = Decoder{}.decode(memory, 0);

        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
        const auto& instruction = std::get<DecodedInstruction>(result);
        EXPECT_EQ(instruction.format, testCase.format);
        EXPECT_EQ(instruction.length, testCase.length);
    }
}

TEST(DecoderTest, PreservesEncodedShiftCountNibble) {
    Memory memory(2);
    writeBytes(memory, 0, {0xA4, 0x1F});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.r1, 1);
    EXPECT_EQ(instruction.r2, 15);
}

TEST(DecoderTest, DecodesSicCompatibleInstructionBeforeInterpretingXbpe) {
    Memory memory(3);
    writeBytes(memory, 0, {0x00, 0x80, 0x10});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(result));
    const auto& instruction = std::get<DecodedInstruction>(result);
    EXPECT_EQ(instruction.opcode, Opcode::LDA);
    EXPECT_EQ(instruction.format, InstructionFormat::Sic);
    EXPECT_TRUE(instruction.x);
    EXPECT_EQ(instruction.encodedOperand, 0x0010);
}

TEST(DecoderTest, ReportsUnknownOpcode) {
    Memory memory(1);
    writeBytes(memory, 0, {0xFF});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::UnknownOpcode);
}

TEST(DecoderTest, ReportsInstructionTruncatedAtEndOfMemory) {
    Memory memory(4);
    writeBytes(memory, 3, {0x03});

    const auto result = Decoder{}.decode(memory, 3);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    const auto& fault = std::get<DecodeFault>(result);
    EXPECT_EQ(fault.code, DecodeFaultCode::TruncatedInstruction);
    EXPECT_EQ(fault.bytesRead, 1);
}

TEST(DecoderTest, ReportsExtendedInstructionTruncatedAfterThreeBytes) {
    Memory memory(3);
    writeBytes(memory, 0, {0x03, 0x10, 0x00});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    const auto& fault = std::get<DecodeFault>(result);
    EXPECT_EQ(fault.code, DecodeFaultCode::TruncatedInstruction);
    EXPECT_EQ(fault.bytesRead, 3);
}

TEST(DecoderTest, ReportsInvalidRegisterNumber) {
    Memory memory(2);
    writeBytes(memory, 0, {0xB4, 0x70});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidRegister);
}

TEST(DecoderTest, RejectsFloatingPointRegisterInFormat2Instruction) {
    Memory memory(2);
    writeBytes(memory, 0, {0x90, 0x06});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidRegister);
}

TEST(DecoderTest, ReportsUnexpectedSecondNibbleForOneRegisterInstruction) {
    Memory memory(2);
    writeBytes(memory, 0, {0xB4, 0x11});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidOperandEncoding);
}

TEST(DecoderTest, ReportsBaseAndPcRelativeCombination) {
    Memory memory(3);
    writeBytes(memory, 0, {0x03, 0x60, 0x00});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidFlagCombination);
}

TEST(DecoderTest, ReportsRelativeFormat4Combination) {
    Memory memory(4);
    writeBytes(memory, 0, {0x03, 0x50, 0x00, 0x00});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidFlagCombination);
}

TEST(DecoderTest, ReportsIndexedImmediateCombination) {
    Memory memory(3);
    writeBytes(memory, 0, {0x01, 0x80, 0x00});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidFlagCombination);
}

TEST(DecoderTest, ReportsSicEncodingForXeOnlyInstruction) {
    Memory memory(3);
    writeBytes(memory, 0, {0x68, 0x00, 0x00});

    const auto result = Decoder{}.decode(memory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidOperandEncoding);
}

TEST(DecoderTest, AcceptsOnlyOperandlessSicRsubEncoding) {
    Memory validMemory(3);
    writeBytes(validMemory, 0, {0x4C, 0x00, 0x00});
    EXPECT_TRUE(std::holds_alternative<DecodedInstruction>(Decoder{}.decode(validMemory, 0)));

    Memory invalidMemory(3);
    writeBytes(invalidMemory, 0, {0x4C, 0x00, 0x01});
    const auto result = Decoder{}.decode(invalidMemory, 0);

    ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
    EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidOperandEncoding);
}

TEST(DecoderTest, AcceptsOnlyOperandlessXeRsubEncoding) {
    Memory validMemory(3);
    writeBytes(validMemory, 0, {0x4F, 0x00, 0x00});
    EXPECT_TRUE(std::holds_alternative<DecodedInstruction>(Decoder{}.decode(validMemory, 0)));

    constexpr std::array invalidEncodings{
        std::array<std::uint8_t, 4>{0x4E, 0x00, 0x00, 0x00},
        std::array<std::uint8_t, 4>{0x4D, 0x00, 0x00, 0x00},
        std::array<std::uint8_t, 4>{0x4F, 0x80, 0x00, 0x00},
        std::array<std::uint8_t, 4>{0x4F, 0x20, 0x00, 0x00},
        std::array<std::uint8_t, 4>{0x4F, 0x10, 0x00, 0x00},
        std::array<std::uint8_t, 4>{0x4F, 0x00, 0x01, 0x00},
    };

    for (const auto& encoding : invalidEncodings) {
        Memory memory(4);
        writeBytes(memory, 0, {encoding[0], encoding[1], encoding[2], encoding[3]});

        const auto result = Decoder{}.decode(memory, 0);

        ASSERT_TRUE(std::holds_alternative<DecodeFault>(result));
        EXPECT_EQ(std::get<DecodeFault>(result).code, DecodeFaultCode::InvalidOperandEncoding);
    }
}
