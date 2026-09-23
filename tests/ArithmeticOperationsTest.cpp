#include "core/execution/ArithmeticOperations.hpp"

#include "core/Memory.hpp"
#include "core/RegisterFile.hpp"

#include <gtest/gtest.h>
#include <variant>

class ArithmeticOperationsTest : public ::testing::Test {
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

TEST_F(ArithmeticOperationsTest, COMP_ProducesThreeConditionCodes) {
    DecodedInstruction inst{Opcode::COMP, InstructionFormat::Format3, 0x0000, 3};
    registers.write(RegisterId::SW, 0xABCDE1);

    registers.write(RegisterId::A, 10);
    auto resLess =
        executeArithmeticOperation(inst, ImmediateValue{20}, defaultPc, memory, registers);
    auto effectLess = std::get<ExecutionEffect>(resLess);
    EXPECT_EQ(effectLess.conditionCode, ConditionCode::Less);
    EXPECT_FALSE(effectLess.registerWrite.has_value());

    registers.write(RegisterId::A, 20);
    auto resEq = executeArithmeticOperation(inst, ImmediateValue{20}, defaultPc, memory, registers);
    auto effectEq = std::get<ExecutionEffect>(resEq);
    EXPECT_EQ(effectEq.conditionCode, ConditionCode::Equal);

    registers.write(RegisterId::A, 30);
    auto resGt = executeArithmeticOperation(inst, ImmediateValue{20}, defaultPc, memory, registers);
    auto effectGt = std::get<ExecutionEffect>(resGt);
    EXPECT_EQ(effectGt.conditionCode, ConditionCode::Greater);
    EXPECT_EQ(registers.read(RegisterId::SW), 0xABCDE1);
}

TEST_F(ArithmeticOperationsTest, COMP_ComparesSignedNegativeValues) {
    DecodedInstruction inst{Opcode::COMP, InstructionFormat::Format3, 0x0000, 3};

    registers.write(RegisterId::A, 0xFFFFFF);
    auto res = executeArithmeticOperation(inst, ImmediateValue{1}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);
    EXPECT_EQ(effect.conditionCode, ConditionCode::Less);
}

TEST_F(ArithmeticOperationsTest, COMPR_ComparesR1AndR2) {
    DecodedInstruction inst{Opcode::COMPR, InstructionFormat::Format2, 0x0000, 2};
    ResolvedOperand op = RegisterPairOperands{RegisterId::S, RegisterId::T};

    registers.write(RegisterId::S, 5);
    registers.write(RegisterId::T, 10);
    auto resLess = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    EXPECT_EQ(std::get<ExecutionEffect>(resLess).conditionCode, ConditionCode::Less);

    registers.write(RegisterId::S, 10);
    registers.write(RegisterId::T, 10);
    auto resEq = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    EXPECT_EQ(std::get<ExecutionEffect>(resEq).conditionCode, ConditionCode::Equal);

    registers.write(RegisterId::S, 15);
    registers.write(RegisterId::T, 10);
    auto resGt = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    EXPECT_EQ(std::get<ExecutionEffect>(resGt).conditionCode, ConditionCode::Greater);
}

TEST_F(ArithmeticOperationsTest, ADD_AddsImmediateAndWrapsOnOverflow) {
    DecodedInstruction inst{Opcode::ADD, InstructionFormat::Format3, 0x0000, 3};

    registers.write(RegisterId::A, 0x7FFFFF);
    auto res = executeArithmeticOperation(inst, ImmediateValue{1}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::A);
    EXPECT_EQ(effect.registerWrite->value, 0x800000);
}

TEST_F(ArithmeticOperationsTest, ADD_WrapsOnNegativeOverflow) {
    DecodedInstruction inst{Opcode::ADD, InstructionFormat::Format3, 0x0000, 3};
    registers.write(RegisterId::A, 0x800000);

    auto result =
        executeArithmeticOperation(inst, ImmediateValue{0xFFFFFF}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->value, 0x7FFFFF);
    EXPECT_EQ(registers.read(RegisterId::A), 0x800000);
}

TEST_F(ArithmeticOperationsTest, ADDR_AddsR1IntoR2) {
    DecodedInstruction inst{Opcode::ADDR, InstructionFormat::Format2, 0x0000, 2};
    registers.write(RegisterId::A, 15);
    registers.write(RegisterId::S, 25);
    ResolvedOperand op = RegisterPairOperands{RegisterId::A, RegisterId::S};

    auto res = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::S);
    EXPECT_EQ(effect.registerWrite->value, 40);
}

TEST_F(ArithmeticOperationsTest, SUB_SubtractsFromMemory) {
    DecodedInstruction inst{Opcode::SUB, InstructionFormat::Format3, 0x0000, 3};
    memory.writeWord(0x20, 50);
    registers.write(RegisterId::A, 120);

    auto res = executeArithmeticOperation(inst, MemoryAddress{0x20}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::A);
    EXPECT_EQ(effect.registerWrite->value, 70);
}

TEST_F(ArithmeticOperationsTest, SUBR_SubtractsR1FromR2) {
    DecodedInstruction inst{Opcode::SUBR, InstructionFormat::Format2, 0x0000, 2};
    registers.write(RegisterId::A, 30);
    registers.write(RegisterId::S, 100);
    ResolvedOperand op = RegisterPairOperands{RegisterId::A, RegisterId::S};

    auto res = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::S);
    EXPECT_EQ(effect.registerWrite->value, 70);
}

TEST_F(ArithmeticOperationsTest, MUL_MultipliesCorrectly) {
    DecodedInstruction inst{Opcode::MUL, InstructionFormat::Format3, 0x0000, 3};
    registers.write(RegisterId::A, 6);

    auto res = executeArithmeticOperation(inst, ImmediateValue{7}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::A);
    EXPECT_EQ(effect.registerWrite->value, 42);
}

TEST_F(ArithmeticOperationsTest, MULR_MultipliesR1AndR2IntoR2) {
    DecodedInstruction inst{Opcode::MULR, InstructionFormat::Format2, 0x0000, 2};
    registers.write(RegisterId::S, 10);
    registers.write(RegisterId::T, 20);
    ResolvedOperand op = RegisterPairOperands{RegisterId::S, RegisterId::T};

    auto res = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::T);
    EXPECT_EQ(effect.registerWrite->value, 200);
}

TEST_F(ArithmeticOperationsTest, DIV_DividesWithTruncationTowardsZero) {
    DecodedInstruction inst{Opcode::DIV, InstructionFormat::Format3, 0x0000, 3};

    registers.write(RegisterId::A, 7);
    auto resPos = executeArithmeticOperation(inst, ImmediateValue{2}, defaultPc, memory, registers);
    EXPECT_EQ(std::get<ExecutionEffect>(resPos).registerWrite->value, 3);

    registers.write(RegisterId::A, 0xFFFFFB);
    auto resNeg = executeArithmeticOperation(inst, ImmediateValue{2}, defaultPc, memory, registers);
    EXPECT_EQ(std::get<ExecutionEffect>(resNeg).registerWrite->value, 0xFFFFFE);
}

TEST_F(ArithmeticOperationsTest, DIV_HandlesMinSignedDividedByMinusOne) {
    DecodedInstruction inst{Opcode::DIV, InstructionFormat::Format3, 0x0000, 3};

    registers.write(RegisterId::A, 0x800000);
    auto res =
        executeArithmeticOperation(inst, ImmediateValue{0xFFFFFF}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->value, 0x800000);
}

TEST_F(ArithmeticOperationsTest, DIV_DivisionByZeroReturnsFault) {
    DecodedInstruction inst{Opcode::DIV, InstructionFormat::Format3, 0x0000, 3};
    registers.write(RegisterId::A, 100);

    auto res = executeArithmeticOperation(inst, ImmediateValue{0}, defaultPc, memory, registers);
    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(res));
    auto fault = std::get<ExecuteFault>(res);
    EXPECT_EQ(fault.code, ExecuteFaultCode::DivisionByZero);
}

TEST_F(ArithmeticOperationsTest, DIV_DivisionByZeroFromMemoryReturnsFaultWithoutMutation) {
    DecodedInstruction inst{Opcode::DIV, InstructionFormat::Format3, 0x0000, 3};
    registers.write(RegisterId::A, 100);
    memory.writeWord(0x20, 0);

    auto result =
        executeArithmeticOperation(inst, MemoryAddress{0x20}, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    EXPECT_EQ(std::get<ExecuteFault>(result).code, ExecuteFaultCode::DivisionByZero);
    EXPECT_EQ(registers.read(RegisterId::A), 100);
    EXPECT_EQ(memory.readWord(0x20), 0);
}

TEST_F(ArithmeticOperationsTest, DIVR_DividesR2ByR1WithoutMutatingRegisters) {
    DecodedInstruction inst{Opcode::DIVR, InstructionFormat::Format2, 0x0000, 2};
    registers.write(RegisterId::S, 4);
    registers.write(RegisterId::T, 20);
    ResolvedOperand operand = RegisterPairOperands{RegisterId::S, RegisterId::T};

    auto result = executeArithmeticOperation(inst, operand, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(result);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::T);
    EXPECT_EQ(effect.registerWrite->value, 5);
    EXPECT_EQ(registers.read(RegisterId::S), 4);
    EXPECT_EQ(registers.read(RegisterId::T), 20);
}

TEST_F(ArithmeticOperationsTest, DIVR_DivisionByZeroInRegisterReturnsFault) {
    DecodedInstruction inst{Opcode::DIVR, InstructionFormat::Format2, 0x0000, 2};
    registers.write(RegisterId::S, 0);
    registers.write(RegisterId::T, 100);
    ResolvedOperand op = RegisterPairOperands{RegisterId::S, RegisterId::T};

    auto res = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(res));
    auto fault = std::get<ExecuteFault>(res);
    EXPECT_EQ(fault.code, ExecuteFaultCode::DivisionByZero);
}

TEST_F(ArithmeticOperationsTest, SHIFTL_CircularRotationLeft) {
    DecodedInstruction inst{Opcode::SHIFTL, InstructionFormat::Format2, 0x0000, 2};

    registers.write(RegisterId::A, 0x800000);
    ResolvedOperand op1 = RegisterAndNibbleOperand{RegisterId::A, 0};

    auto res1 = executeArithmeticOperation(inst, op1, defaultPc, memory, registers);
    auto effect1 = std::get<ExecutionEffect>(res1);
    EXPECT_EQ(effect1.registerWrite->value, 0x000001);

    registers.write(RegisterId::A, 0x123456);
    ResolvedOperand op16 = RegisterAndNibbleOperand{RegisterId::A, 15};
    auto res16 = executeArithmeticOperation(inst, op16, defaultPc, memory, registers);
    auto effect16 = std::get<ExecutionEffect>(res16);
    EXPECT_EQ(effect16.registerWrite->value, 0x561234);
}

TEST_F(ArithmeticOperationsTest, SHIFTR_ArithmeticRightShiftPreservesSign) {
    DecodedInstruction inst{Opcode::SHIFTR, InstructionFormat::Format2, 0x0000, 2};

    registers.write(RegisterId::A, 0x800000);
    ResolvedOperand op = RegisterAndNibbleOperand{RegisterId::A, 0};

    auto res = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);
    EXPECT_EQ(effect.registerWrite->value, 0xC00000);

    ResolvedOperand op16 = RegisterAndNibbleOperand{RegisterId::A, 15};
    auto result16 = executeArithmeticOperation(inst, op16, defaultPc, memory, registers);
    auto effect16 = std::get<ExecutionEffect>(result16);
    EXPECT_EQ(effect16.registerWrite->value, 0xFFFF80);
}

TEST_F(ArithmeticOperationsTest, ShiftRejectsNibbleOutsideEncodedRange) {
    DecodedInstruction inst{Opcode::SHIFTL, InstructionFormat::Format2, 0x0000, 2};
    registers.write(RegisterId::A, 0x123456);
    ResolvedOperand operand = RegisterAndNibbleOperand{RegisterId::A, 16};

    auto result = executeArithmeticOperation(inst, operand, defaultPc, memory, registers);

    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(result));
    EXPECT_EQ(std::get<ExecuteFault>(result).code, ExecuteFaultCode::InvalidOperand);
    EXPECT_EQ(registers.read(RegisterId::A), 0x123456);
}

TEST_F(ArithmeticOperationsTest, TIX_IncrementsXAndSetsConditionCodeSimultaneously) {
    DecodedInstruction inst{Opcode::TIX, InstructionFormat::Format3, 0x0000, 3};

    registers.write(RegisterId::X, 9);
    auto res = executeArithmeticOperation(inst, ImmediateValue{10}, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    EXPECT_EQ(registers.read(RegisterId::X), 9);

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::X);
    EXPECT_EQ(effect.registerWrite->value, 10);
    EXPECT_EQ(effect.conditionCode, ConditionCode::Equal);
}

TEST_F(ArithmeticOperationsTest, TIXR_IncrementsXAndComparesAgainstRegister) {
    DecodedInstruction inst{Opcode::TIXR, InstructionFormat::Format2, 0x0000, 2};

    registers.write(RegisterId::X, 9);
    registers.write(RegisterId::T, 15);
    ResolvedOperand op = SingleRegisterOperand{RegisterId::T};

    auto res = executeArithmeticOperation(inst, op, defaultPc, memory, registers);
    auto effect = std::get<ExecutionEffect>(res);

    EXPECT_EQ(effect.registerWrite->target, RegisterId::X);
    EXPECT_EQ(effect.registerWrite->value, 10);
    EXPECT_EQ(effect.conditionCode, ConditionCode::Less);
    EXPECT_EQ(registers.read(RegisterId::X), 9);
    EXPECT_EQ(registers.read(RegisterId::T), 15);
}

TEST_F(ArithmeticOperationsTest, IncompatibleOperandReturnsInvalidOperandFault) {
    DecodedInstruction inst{Opcode::ADD, InstructionFormat::Format3, 0x0000, 3};
    auto res = executeArithmeticOperation(inst, NoOperand{}, defaultPc, memory, registers);
    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(res));
    EXPECT_EQ(std::get<ExecuteFault>(res).code, ExecuteFaultCode::InvalidOperand);
}

TEST_F(ArithmeticOperationsTest, MemoryReadAtMemoryLimitThrowsAndConvertsToFault) {
    DecodedInstruction inst{Opcode::ADD, InstructionFormat::Format3, 0x0000, 3};
    auto res =
        executeArithmeticOperation(inst,
                                   MemoryAddress{static_cast<std::uint32_t>(memory.size() - 1)},
                                   defaultPc,
                                   memory,
                                   registers);
    ASSERT_TRUE(std::holds_alternative<ExecuteFault>(res));
    EXPECT_EQ(std::get<ExecuteFault>(res).code, ExecuteFaultCode::MemoryAccessOutOfRange);
}
