#include "MacroAssemblerTestSupport.hpp"
#include "assembler/Assembler.hpp"
#include "assembler/ListingWriter.hpp"
#include "core/Machine.hpp"
#include "macros/MacroAssemblerCli.hpp"

#include <algorithm>
#include <cstdint>
#include <variant>

namespace macro {
namespace {
namespace fs = std::filesystem;
using MacroAssemblerAcceptanceTest = test::MacroAssemblerFiles;

TEST_F(MacroAssemblerAcceptanceTest, NestedDefinitionsAndCallsMatchManualAssemblyAndExecuteInVm) {
    const auto fixtures = fs::path{MACRO_ASSEMBLER_FIXTURE_DIR};
    fs::create_directory("input");
    write("input/macros.asm", read(fixtures / "nested-sum.asm"));
    ASSERT_EQ(invoke({"input/macros.asm"}), 0) << errors.str();
    const auto manualSource = read(fixtures / "nested-sum-expanded.asm");
    EXPECT_EQ(read("MASMAPRG.ASM"), manualSource);
    const auto manual = assembler::assemble(manualSource);
    const auto* program = std::get_if<assembler::AssemblySuccess>(&manual);
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(read("input/macros.lst"), assembler::ListingWriter::render(*program));

    // Independently calculated: START=100, SUM=129, RESULT=12C, TABLE=1138 (hex).
    std::vector<std::uint8_t> expected{
        0xB4, 0x10, 0x01, 0,    0,    0x6D, 0,    5,    0xB8, 0x40, 0x90, 0x10, 0x3B, 0x2F,
        0xF9, 0x0F, 0x20, 0x17, 0x4B, 0x10, 0x01, 0x2F, 0x0F, 0x20, 0x13, 0x69, 0x10, 0x11,
        0x38, 0x03, 0x40, 0,    0x1B, 0x20, 9,    0x0F, 0x20, 6,    0x3F, 0x2F, 0xFD, 0,
        0,    0,    0,    0,    0,    0x03, 0x2F, 0xF7, 0x21, 0,    2,    0x4F, 0,    0};
    expected.insert(expected.end(), 4096, 0);
    expected.insert(expected.end(), {0, 0, 7, 0, 1, 0x29, 0x4F, 0x4B});
    ASSERT_EQ(expected.size(), 4160U);
    const auto image = read("input/macros.bin");
    const std::vector<std::uint8_t> bytes(image.begin(), image.end());
    EXPECT_EQ(bytes, expected);
    EXPECT_EQ(bytes, program->bytes);
    EXPECT_EQ(program->loadAddress, 0x100U);
    EXPECT_EQ(program->initialPc, 0x100U);

    Machine machine;
    ASSERT_TRUE(std::holds_alternative<LoadSuccess>(machine.loadProgram(bytes, 0x100, 0x100)));
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
    expectNoStagingFiles();
}

} // namespace
} // namespace macro
