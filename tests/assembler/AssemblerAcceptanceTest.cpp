#include "InstructionTestSupport.hpp"
#include "assembler/Assembler.hpp"
#include "core/Decoder.hpp"
#include "core/Machine.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

namespace assembler {
namespace {

TEST(AssemblerAcceptanceTest, CompleteProgramHasHandCalculatedBytesAndExecutesInVm) {
    std::ifstream source(std::filesystem::path{ASSEMBLER_FIXTURE_DIR} / "sum-and-table.asm");
    ASSERT_TRUE(source.good());
    const std::string text{std::istreambuf_iterator<char>(source),
                           std::istreambuf_iterator<char>()};
    const auto result = assemble(text);
    const auto* program = std::get_if<AssemblySuccess>(&result);
    ASSERT_NE(program, nullptr);
    // Addresses worked from START 0x100: DONE=0x126, SUM=0x129,
    // RESULT=0x12C, DOUBLE=0x12F, GAP=0x138, TABLE=0x1138.
    std::vector<std::uint8_t> expected{
        0xB4, 0x10, 0x01, 0,    0,    0x6D, 0,    5,    0xB8, 0x40, 0x90, 0x10, 0x3B, 0x2F,
        0xF9, 0x0F, 0x20, 0x17, 0x4B, 0x10, 0x01, 0x2F, 0x0F, 0x20, 0x13, 0x69, 0x10, 0x11,
        0x38, 0x03, 0x40, 0,    0x1B, 0x20, 9,    0x0F, 0x20, 6,    0x3F, 0x2F, 0xFD, 0,
        0,    0,    0,    0,    0,    0x03, 0x2F, 0xF7, 0x21, 0,    2,    0x4F, 0,    0};
    expected.insert(expected.end(), 4096, 0);
    expected.insert(expected.end(), {0, 0, 7, 0, 1, 0x29, 0x4F, 0x4B});
    ASSERT_EQ(expected.size(), 4160U);
    EXPECT_EQ(program->bytes, expected);
    EXPECT_EQ(program->loadAddress, 0x100U);
    EXPECT_EQ(program->initialPc, 0x100U);
    EXPECT_EQ(program->listing[19].reservedBytes, 3U);

    Machine machine;
    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(
        machine.loadProgram(program->bytes, program->loadAddress, program->initialPc)));
    for (int step = 0; step < 28; ++step)
        ASSERT_TRUE(std::holds_alternative<StepSuccess>(machine.step())) << step;
    const auto snapshot = machine.snapshot();
    EXPECT_EQ(snapshot.registers.a, 37U);
    EXPECT_EQ(snapshot.registers.x, 5U);
    EXPECT_EQ(snapshot.registers.s, 5U);
    EXPECT_EQ(snapshot.registers.b, 0x1138U);
    EXPECT_EQ(snapshot.registers.l, 0x116U);
    EXPECT_EQ(snapshot.registers.pc, 0x126U);
    EXPECT_EQ(snapshot.registers.t, 0U);
    EXPECT_EQ(snapshot.registers.f, 0U);
    EXPECT_EQ(snapshot.registers.conditionCode, ConditionCode::Equal);
    EXPECT_FALSE(snapshot.lastFault);
    std::vector<std::uint8_t> expectedMemory(Machine::DefaultMemorySize, 0);
    std::copy(expected.begin(), expected.end(), expectedMemory.begin() + 0x100);
    expectedMemory[0x12B] = 15;
    expectedMemory[0x12E] = 37;
    EXPECT_EQ(snapshot.memory, expectedMemory);
}

TEST(AssemblerAcceptanceTest, All38InstructionsAssembleThroughTheTwoPassPipeline) {
    for (const auto& instruction : test::ExpectedIsa) {
        SCOPED_TRACE(instruction.name);
        std::string operand;
        switch (instruction.operands) {
            case OperandPattern::None:
                break;
            case OperandPattern::OneRegister:
                operand = " A";
                break;
            case OperandPattern::TwoRegisters:
                operand = " A,X";
                break;
            case OperandPattern::RegisterAndCount:
                operand = " A,16";
                break;
            case OperandPattern::Memory:
                operand = " #7";
                break;
        }
        const auto result = assemble(" " + std::string{instruction.name} + operand + "\n END\n");
        const auto* program = std::get_if<AssemblySuccess>(&result);
        ASSERT_NE(program, nullptr);
        Memory memory(16);
        for (std::size_t i = 0; i < program->bytes.size(); ++i)
            memory.writeByte(i, program->bytes[i]);
        const auto decoded = Decoder{}.decode(memory, 0);
        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
        EXPECT_EQ(std::get<DecodedInstruction>(decoded).opcode, instruction.vmOpcode);
    }
}

} // namespace
} // namespace assembler
