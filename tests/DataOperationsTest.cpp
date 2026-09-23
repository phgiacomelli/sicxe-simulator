#include "core/execution/DataOperations.hpp"

#include "core/Memory.hpp"
#include "core/RegisterFile.hpp"

#include <gtest/gtest.h>
#include <vector>

class DataOperationsTest : public ::testing::Test {
protected:
    Memory memory{0x100000};
    RegisterFile registers;
    std::uint32_t defaultPc = 0x1000;

    void SetUp() override {
        registers.reset();
        for (std::uint32_t i = 0; i < 100; ++i) {
            memory.writeByte(i, 0x00);
        }
    }
};

TEST_F(DataOperationsTest, CLEAR_ZeroesRegister) {
    registers.write(RegisterId::X, 0xFFFFFF);
    DecodedInstruction inst{Opcode::CLEAR, InstructionFormat::Format2, 0x0000, 2};
    ResolvedOperand operand = SingleRegisterOperand{RegisterId::X};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    EXPECT_EQ(registers.read(RegisterId::X), 0xFFFFFF);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::X);
    EXPECT_EQ(effect.registerWrite->value, 0x000000);
}

TEST_F(DataOperationsTest, RMO_CopiesRegisterAndDoesNotMutate) {
    registers.write(RegisterId::A, 0x123456);
    registers.write(RegisterId::X, 0x000000);
    DecodedInstruction inst{Opcode::RMO, InstructionFormat::Format2, 0x0000, 2};
    ResolvedOperand operand = RegisterPairOperands{RegisterId::A, RegisterId::X};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::X);
    EXPECT_EQ(effect.registerWrite->value, 0x123456);
    EXPECT_EQ(registers.read(RegisterId::A), 0x123456);
    EXPECT_EQ(registers.read(RegisterId::X), 0x000000);
    EXPECT_FALSE(effect.memoryWrite.has_value());
    EXPECT_FALSE(effect.conditionCode.has_value());
}

TEST_F(DataOperationsTest, CLEAR_AndRMORejectSWAsDestination) {
    registers.write(RegisterId::SW, 0x000001);
    const DecodedInstruction clear{Opcode::CLEAR, InstructionFormat::Format2, 0x0000, 2};
    const DecodedInstruction rmo{Opcode::RMO, InstructionFormat::Format2, 0x0000, 2};

    const auto clearResult = executeDataOperation(
        clear, SingleRegisterOperand{RegisterId::SW}, defaultPc, memory, registers);
    const auto rmoResult = executeDataOperation(
        rmo, RegisterPairOperands{RegisterId::A, RegisterId::SW}, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(clearResult));
    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(rmoResult));
    EXPECT_EQ(std::get<ExecuteFault>(clearResult).code, ExecuteFaultCode::InvalidOperand);
    EXPECT_EQ(std::get<ExecuteFault>(rmoResult).code, ExecuteFaultCode::InvalidOperand);
    EXPECT_EQ(registers.read(RegisterId::SW), 0x000001);
}

TEST_F(DataOperationsTest, LDA_LoadsBitPatterns) {
    std::vector<std::uint32_t> patterns = {0x000000, 0xFFFFFF, 0x800000};

    for (auto pattern : patterns) {
        DecodedInstruction inst{Opcode::LDA, InstructionFormat::Format3, 0x0000, 3};
        ResolvedOperand operand = ImmediateValue{pattern};

        auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
        auto effect = std::get<ExecutionEffect>(result);

        ASSERT_TRUE(effect.registerWrite.has_value());
        EXPECT_EQ(effect.registerWrite->target, RegisterId::A);
        EXPECT_EQ(effect.registerWrite->value, pattern);
    }
}

TEST_F(DataOperationsTest, LDB_LoadsFromMemory) {
    memory.writeByte(0x0050, 0x11);
    memory.writeByte(0x0051, 0x22);
    memory.writeByte(0x0052, 0x33);

    DecodedInstruction inst{Opcode::LDB, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{0x0050};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::B);
    EXPECT_EQ(effect.registerWrite->value, 0x112233);
}

TEST_F(DataOperationsTest, LDL_LoadsImmediate) {
    DecodedInstruction inst{Opcode::LDL, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0xABCDEF};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::L);
    EXPECT_EQ(effect.registerWrite->value, 0xABCDEF);
}

TEST_F(DataOperationsTest, LDS_LoadsImmediate) {
    DecodedInstruction inst{Opcode::LDS, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x123456};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(effect.registerWrite->target, RegisterId::S);
}

TEST_F(DataOperationsTest, LDT_LoadsImmediate) {
    DecodedInstruction inst{Opcode::LDT, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x123456};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(effect.registerWrite->target, RegisterId::T);
}

TEST_F(DataOperationsTest, LDX_LoadsImmediate) {
    DecodedInstruction inst{Opcode::LDX, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x123456};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(effect.registerWrite->target, RegisterId::X);
}

TEST_F(DataOperationsTest, STA_WritesAtValidMemoryBoundaries) {
    registers.write(RegisterId::A, 0xFFFFFF);
    const std::uint32_t lastValidWordAddress = 0x100000 - 3;

    DecodedInstruction inst{Opcode::STA, InstructionFormat::Format4, 0x0000, 4};
    ResolvedOperand operand = MemoryAddress{lastValidWordAddress};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.memoryWrite.has_value());
    auto wordWrite = std::get<WordWrite>(*effect.memoryWrite);
    EXPECT_EQ(wordWrite.address, lastValidWordAddress);
    EXPECT_EQ(wordWrite.value, 0xFFFFFF);
    EXPECT_EQ(memory.readWord(lastValidWordAddress), 0x000000);
    EXPECT_FALSE(effect.registerWrite.has_value());
    EXPECT_FALSE(effect.conditionCode.has_value());
}

TEST_F(DataOperationsTest, STB_WritesAtMemoryStart) {
    registers.write(RegisterId::B, 0x123456);
    std::uint32_t startAddress = 0x0000;

    DecodedInstruction inst{Opcode::STB, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{startAddress};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.memoryWrite.has_value());
    EXPECT_EQ(std::get<WordWrite>(*effect.memoryWrite).address, startAddress);
}

TEST_F(DataOperationsTest, STL_WritesToMemory) {
    registers.write(RegisterId::L, 0xABCDEF);
    DecodedInstruction inst{Opcode::STL, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{0x0100};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(std::get<WordWrite>(*effect.memoryWrite).value, 0xABCDEF);
}

TEST_F(DataOperationsTest, STS_WritesToMemory) {
    registers.write(RegisterId::S, 0x111111);
    DecodedInstruction inst{Opcode::STS, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{0x0100};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(std::get<WordWrite>(*effect.memoryWrite).value, 0x111111);
}

TEST_F(DataOperationsTest, STT_WritesToMemory) {
    registers.write(RegisterId::T, 0x222222);
    DecodedInstruction inst{Opcode::STT, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{0x0100};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(std::get<WordWrite>(*effect.memoryWrite).value, 0x222222);
}

TEST_F(DataOperationsTest, STX_WritesToMemory) {
    registers.write(RegisterId::X, 0x333333);
    DecodedInstruction inst{Opcode::STX, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{0x0100};
    auto effect = std::get<ExecutionEffect>(
        executeDataOperation(inst, operand, defaultPc, memory, registers));
    EXPECT_EQ(std::get<WordWrite>(*effect.memoryWrite).value, 0x333333);
}

TEST_F(DataOperationsTest, LDCH_PreservesUpperBits) {
    registers.write(RegisterId::A, 0xAABBCC);
    DecodedInstruction inst{Opcode::LDCH, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x99};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::A);
    EXPECT_EQ(effect.registerWrite->value, 0xAABB99);
}

TEST_F(DataOperationsTest, STCH_WritesSingleByte) {
    registers.write(RegisterId::A, 0x112233);
    DecodedInstruction inst{Opcode::STCH, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = MemoryAddress{0x0050};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.memoryWrite.has_value());
    ASSERT_TRUE(std::holds_alternative<ByteWrite>(*effect.memoryWrite));

    auto byteWrite = std::get<ByteWrite>(*effect.memoryWrite);
    EXPECT_EQ(byteWrite.address, 0x0050);
    EXPECT_EQ(byteWrite.value, 0x33);
    EXPECT_EQ(memory.readByte(0x0050), 0x00);
    EXPECT_FALSE(effect.registerWrite.has_value());
    EXPECT_FALSE(effect.conditionCode.has_value());
}

TEST_F(DataOperationsTest, AND_PerformsLogicalAnd) {
    registers.write(RegisterId::A, 0x00FF00);
    DecodedInstruction inst{Opcode::AND, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x0F0F0F};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    EXPECT_EQ(effect.registerWrite->value, 0x00FF00 & 0x0F0F0F);
}

TEST_F(DataOperationsTest, OR_PerformsLogicalOr) {
    registers.write(RegisterId::A, 0x00FF00);
    DecodedInstruction inst{Opcode::OR, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x0000FF};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    EXPECT_EQ(effect.registerWrite->value, 0x00FF00 | 0x0000FF);
}

TEST_F(DataOperationsTest, AND_AndORReadWordsFromMemory) {
    registers.write(RegisterId::A, 0xF0FF0F);
    memory.writeWord(0x0200, 0x0FF0FF);
    const DecodedInstruction andInstruction{Opcode::AND, InstructionFormat::Format3, 0x0000, 3};
    const DecodedInstruction orInstruction{Opcode::OR, InstructionFormat::Format3, 0x0000, 3};
    const ResolvedOperand operand = MemoryAddress{0x0200};

    const auto andEffect = std::get<ExecutionEffect>(
        executeDataOperation(andInstruction, operand, defaultPc, memory, registers));
    const auto orEffect = std::get<ExecutionEffect>(
        executeDataOperation(orInstruction, operand, defaultPc, memory, registers));

    ASSERT_TRUE(andEffect.registerWrite.has_value());
    ASSERT_TRUE(orEffect.registerWrite.has_value());
    EXPECT_EQ(andEffect.registerWrite->value, 0xF0FF0F & 0x0FF0FF);
    EXPECT_EQ(orEffect.registerWrite->value, 0xF0FF0F | 0x0FF0FF);
    EXPECT_EQ(registers.read(RegisterId::A), 0xF0FF0F);
    EXPECT_EQ(memory.readWord(0x0200), 0x0FF0FF);
    EXPECT_FALSE(andEffect.memoryWrite.has_value());
    EXPECT_FALSE(orEffect.memoryWrite.has_value());
    EXPECT_FALSE(andEffect.conditionCode.has_value());
    EXPECT_FALSE(orEffect.conditionCode.has_value());
}

TEST_F(DataOperationsTest, StoreWithImmediateFails) {
    DecodedInstruction inst{Opcode::STA, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x10};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    auto fault = std::get<ExecuteFault>(result);
    EXPECT_EQ(fault.code, ExecuteFaultCode::InvalidOperand);
}

TEST_F(DataOperationsTest, NoOperandFails) {
    DecodedInstruction inst{Opcode::LDA, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = NoOperand{};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    EXPECT_EQ(std::get<ExecuteFault>(result).code, ExecuteFaultCode::InvalidOperand);
}

TEST_F(DataOperationsTest, InvalidRegisterVariantFails) {
    DecodedInstruction inst{Opcode::CLEAR, InstructionFormat::Format2, 0x0000, 2};

    ResolvedOperand operand = RegisterPairOperands{RegisterId::A, RegisterId::X};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    EXPECT_EQ(std::get<ExecuteFault>(result).code, ExecuteFaultCode::InvalidOperand);
}

TEST_F(DataOperationsTest, WordReadCrossingMemoryEndFails) {
    std::uint32_t outOfBoundsAddr = 0x100000 - 2;
    DecodedInstruction inst{Opcode::LDA, InstructionFormat::Format4, 0x0000, 4};
    ResolvedOperand operand = MemoryAddress{outOfBoundsAddr};

    auto result = executeDataOperation(inst, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    EXPECT_EQ(std::get<ExecuteFault>(result).code, ExecuteFaultCode::MemoryAccessOutOfRange);
}

TEST_F(DataOperationsTest, ByteOperationsSucceedAtLastAddress) {
    std::uint32_t lastAddress = 0x100000 - 1;
    memory.writeByte(lastAddress, 0x42);

    DecodedInstruction instLd{Opcode::LDCH, InstructionFormat::Format4, 0x0000, 4};
    ResolvedOperand operand = MemoryAddress{lastAddress};
    auto resultLd = executeDataOperation(instLd, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecutionEffect>(resultLd));
    EXPECT_EQ(std::get<ExecutionEffect>(resultLd).registerWrite->value & 0xFF, 0x42);

    DecodedInstruction instSt{Opcode::STCH, InstructionFormat::Format4, 0x0000, 4};
    auto resultSt = executeDataOperation(instSt, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecutionEffect>(resultSt));
    auto effectSt = std::get<ExecutionEffect>(resultSt);
    ASSERT_TRUE(effectSt.memoryWrite.has_value());
    EXPECT_EQ(std::get<ByteWrite>(*effectSt.memoryWrite).address, lastAddress);
}

TEST_F(DataOperationsTest, PropagatesPostFetchPc) {
    std::uint32_t customPc = 0x2468;
    DecodedInstruction inst{Opcode::LDA, InstructionFormat::Format3, 0x0000, 3};
    ResolvedOperand operand = ImmediateValue{0x0};

    auto result = executeDataOperation(inst, operand, customPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecutionEffect>(result));
    EXPECT_EQ(std::get<ExecutionEffect>(result).nextPc, customPc);
}
