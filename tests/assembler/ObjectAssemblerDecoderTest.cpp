#include "InstructionTestSupport.hpp"
#include "assembler/Assembler.hpp"
#include "assembler/ObjectAssembler.hpp"
#include "core/Decoder.hpp"

#include <gtest/gtest.h>

namespace assembler {
namespace {

TEST(ObjectAssemblerDecoderTest, All38OpcodesMatchAbsoluteEncodingAndDecodeInTheVm) {
    for (const auto& spec : test::ExpectedIsa) {
        SCOPED_TRACE(spec.name);
        std::string operand;
        switch (spec.operands) {
            case OperandPattern::Memory:
                operand = " #0x123";
                break;
            case OperandPattern::OneRegister:
                operand = " T";
                break;
            case OperandPattern::TwoRegisters:
                operand = " A,X";
                break;
            case OperandPattern::RegisterAndCount:
                operand = " T,16";
                break;
            case OperandPattern::None:
                break;
        }
        const auto source = "Main START 0\n " + std::string(spec.name) + operand + "\n END\n";
        const auto result = assembleObject(source);
        ASSERT_TRUE(std::holds_alternative<linker::ObjectModule>(result));
        const auto& module = std::get<linker::ObjectModule>(result);
        ASSERT_EQ(module.text.size(), 1u);
        const auto absolute = assemble(source);
        ASSERT_TRUE(std::holds_alternative<AssemblySuccess>(absolute));
        EXPECT_EQ(module.text[0].bytes, std::get<AssemblySuccess>(absolute).bytes);
        EXPECT_TRUE(module.modifications.empty());
        Memory memory(16);
        for (std::size_t i = 0; i < module.text[0].bytes.size(); ++i)
            memory.writeByte(i, module.text[0].bytes[i]);
        const auto decoded = Decoder{}.decode(memory, 0);
        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
        EXPECT_EQ(std::get<DecodedInstruction>(decoded).opcode, spec.vmOpcode);
    }
}

TEST(ObjectAssemblerDecoderTest, ExternalFormat4PreservesAllAddressingAndFlagBits) {
    for (const auto& spec : test::ExpectedIsa) {
        if (spec.operands != OperandPattern::Memory)
            continue;
        for (const auto mode : {"Ext", "#Ext", "@Ext", "Ext,X"}) {
            SCOPED_TRACE(std::string(spec.name) + " " + mode);
            const auto result = assembleObject("Main START 0x4000\n EXTREF Ext\n +" +
                                               std::string(spec.name) + " " + mode + "\n END\n");
            ASSERT_TRUE(std::holds_alternative<linker::ObjectModule>(result));
            const auto& module = std::get<linker::ObjectModule>(result);
            ASSERT_EQ(module.modifications.size(), 1u);
            EXPECT_EQ(module.modifications[0].offset, 1u);
            EXPECT_EQ(module.modifications[0].halfBytes, 5u);
            Memory memory(16);
            for (std::size_t i = 0; i < 4; ++i)
                memory.writeByte(i, module.text[0].bytes[i]);
            const auto decoded = Decoder{}.decode(memory, 0);
            ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
            const auto& instruction = std::get<DecodedInstruction>(decoded);
            EXPECT_EQ(instruction.opcode, spec.vmOpcode);
            EXPECT_EQ(instruction.format, InstructionFormat::Format4);
            EXPECT_EQ(instruction.n, mode[0] != '#');
            EXPECT_EQ(instruction.i, mode[0] != '@');
            EXPECT_EQ(instruction.x, std::string_view(mode).ends_with(",X"));
            EXPECT_FALSE(instruction.b);
            EXPECT_FALSE(instruction.p);
            EXPECT_TRUE(instruction.e);
            EXPECT_EQ(instruction.encodedOperand, 0u);
        }
    }
}

} // namespace
} // namespace assembler
