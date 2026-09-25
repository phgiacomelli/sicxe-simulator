#include "core/execution/ControlOperations.hpp"

#include "core/Memory.hpp"
#include "core/RegisterFile.hpp"

#include <array>
#include <gtest/gtest.h>
#include <variant>

class ControlOperationsTest : public ::testing::Test {
protected:
    using CC = ConditionCode;
    using Op = Opcode;

    static constexpr std::array AllConditionCodes{CC::Less, CC::Equal, CC::Greater};
    static constexpr std::array ConditionalJumps{Op::JEQ, Op::JGT, Op::JLT};
    static constexpr std::array AddressJumps{Op::J, Op::JEQ, Op::JGT, Op::JLT, Op::JSUB};

    Memory memory{0x100000};
    RegisterFile registers;
    std::uint32_t instructionAddress = 0x1000;
    std::uint32_t postFetchPc = 0x1003;
    std::uint32_t target = 0x2000;

    DecodedInstruction instruction(Opcode opcode) const {
        DecodedInstruction decoded{};
        decoded.opcode = opcode;
        decoded.format = InstructionFormat::Format3;
        decoded.instructionAddress = instructionAddress;
        decoded.length = 3;
        return decoded;
    }

    ExecuteResult execute(Opcode opcode, const ResolvedOperand& operand) const {
        return executeControlOperation(
            instruction(opcode), operand, postFetchPc, memory, registers);
    }

    ExecutionEffect executeEffect(Opcode opcode, const ResolvedOperand& operand) const {
        return std::get<ExecutionEffect>(execute(opcode, operand));
    }

    ExecuteFault executeFault(Opcode opcode, const ResolvedOperand& operand) const {
        return std::get<ExecuteFault>(execute(opcode, operand));
    }
};

TEST_F(ControlOperationsTest, J_AlwaysTransfersToResolvedAddress) {
    for (const auto conditionCode : AllConditionCodes) {
        registers.setConditionCode(conditionCode);

        const auto effect = executeEffect(Opcode::J, MemoryAddress{target});

        EXPECT_EQ(effect.nextPc, target);
        EXPECT_FALSE(effect.registerWrite.has_value());
        EXPECT_FALSE(effect.memoryWrite.has_value());
        EXPECT_FALSE(effect.conditionCode.has_value());
    }
}

TEST_F(ControlOperationsTest, J_AcceptsImmediateValueAsTargetAddress) {
    const auto effect = executeEffect(Opcode::J, ImmediateValue{target});

    EXPECT_EQ(effect.nextPc, target);
    EXPECT_FALSE(effect.registerWrite.has_value());
}

TEST_F(ControlOperationsTest, JEQ_TransfersOnlyWhenConditionCodeIsEqual) {
    registers.setConditionCode(ConditionCode::Equal);
    EXPECT_EQ(executeEffect(Opcode::JEQ, MemoryAddress{target}).nextPc, target);

    registers.setConditionCode(ConditionCode::Less);
    EXPECT_EQ(executeEffect(Opcode::JEQ, MemoryAddress{target}).nextPc, postFetchPc);

    registers.setConditionCode(ConditionCode::Greater);
    EXPECT_EQ(executeEffect(Opcode::JEQ, MemoryAddress{target}).nextPc, postFetchPc);
}

TEST_F(ControlOperationsTest, JGT_TransfersOnlyWhenConditionCodeIsGreater) {
    registers.setConditionCode(ConditionCode::Greater);
    EXPECT_EQ(executeEffect(Opcode::JGT, MemoryAddress{target}).nextPc, target);

    registers.setConditionCode(ConditionCode::Less);
    EXPECT_EQ(executeEffect(Opcode::JGT, MemoryAddress{target}).nextPc, postFetchPc);

    registers.setConditionCode(ConditionCode::Equal);
    EXPECT_EQ(executeEffect(Opcode::JGT, MemoryAddress{target}).nextPc, postFetchPc);
}

TEST_F(ControlOperationsTest, JLT_TransfersOnlyWhenConditionCodeIsLess) {
    registers.setConditionCode(ConditionCode::Less);
    EXPECT_EQ(executeEffect(Opcode::JLT, MemoryAddress{target}).nextPc, target);

    registers.setConditionCode(ConditionCode::Equal);
    EXPECT_EQ(executeEffect(Opcode::JLT, MemoryAddress{target}).nextPc, postFetchPc);

    registers.setConditionCode(ConditionCode::Greater);
    EXPECT_EQ(executeEffect(Opcode::JLT, MemoryAddress{target}).nextPc, postFetchPc);
}

TEST_F(ControlOperationsTest, ConditionalJumpsProduceNoOtherChangeWhetherTakenOrNot) {
    for (const auto opcode : ConditionalJumps) {
        for (const auto conditionCode : AllConditionCodes) {
            registers.setConditionCode(conditionCode);

            const auto effect = executeEffect(opcode, MemoryAddress{target});

            EXPECT_FALSE(effect.registerWrite.has_value());
            EXPECT_FALSE(effect.memoryWrite.has_value());
            EXPECT_FALSE(effect.conditionCode.has_value());
        }
    }
}

TEST_F(ControlOperationsTest, JSUB_StoresPostFetchPcInLAndTransfersToTarget) {
    registers.write(RegisterId::L, 0x00ABCD);

    const auto effect = executeEffect(Opcode::JSUB, MemoryAddress{target});

    EXPECT_EQ(effect.nextPc, target);
    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::L);
    EXPECT_EQ(effect.registerWrite->value, postFetchPc);
    EXPECT_FALSE(effect.memoryWrite.has_value());
    EXPECT_FALSE(effect.conditionCode.has_value());
}

TEST_F(ControlOperationsTest, JSUB_UsesReceivedPostFetchPcForAnyInstructionLength) {
    postFetchPc = 0x1004;

    const auto effect = executeEffect(Opcode::JSUB, MemoryAddress{target});

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->value, 0x1004U);
}

TEST_F(ControlOperationsTest, JSUB_IgnoresConditionCode) {
    for (const auto conditionCode : AllConditionCodes) {
        registers.setConditionCode(conditionCode);

        EXPECT_EQ(executeEffect(Opcode::JSUB, MemoryAddress{target}).nextPc, target);
    }
}

TEST_F(ControlOperationsTest, RSUB_TransfersToAddressStoredInL) {
    registers.write(RegisterId::L, 0x001234);

    const auto effect = executeEffect(Opcode::RSUB, NoOperand{});

    EXPECT_EQ(effect.nextPc, 0x001234U);
    EXPECT_FALSE(effect.registerWrite.has_value());
    EXPECT_FALSE(effect.memoryWrite.has_value());
    EXPECT_FALSE(effect.conditionCode.has_value());
}

TEST_F(ControlOperationsTest, RSUB_ReturnsToLAfterJSUBEffectIsCommitted) {
    const auto jsub = executeEffect(Opcode::JSUB, MemoryAddress{target});
    ASSERT_TRUE(jsub.registerWrite.has_value());
    registers.write(jsub.registerWrite->target, jsub.registerWrite->value);

    const auto rsub = executeEffect(Opcode::RSUB, NoOperand{});

    EXPECT_EQ(rsub.nextPc, postFetchPc);
}

TEST_F(ControlOperationsTest, ReservedTargetsAreForwardedForValidationByTheMachine) {
    const auto effect = executeEffect(Opcode::J, MemoryAddress{0x200000});

    EXPECT_EQ(effect.nextPc, 0x200000U);
}

TEST_F(ControlOperationsTest, JumpsRejectOperandsThatAreNotAnAddress) {
    for (const auto opcode : AddressJumps) {
        EXPECT_EQ(executeFault(opcode, NoOperand{}).code, ExecuteFaultCode::InvalidOperand);
        EXPECT_EQ(executeFault(opcode, SingleRegisterOperand{RegisterId::A}).code,
                  ExecuteFaultCode::InvalidOperand);
        EXPECT_EQ(executeFault(opcode, RegisterPairOperands{RegisterId::A, RegisterId::X}).code,
                  ExecuteFaultCode::InvalidOperand);
    }
}

TEST_F(ControlOperationsTest, ConditionalJumpsValidateOperandBeforeReadingConditionCode) {
    registers.setConditionCode(ConditionCode::Less);

    const auto fault = executeFault(Opcode::JEQ, NoOperand{});

    EXPECT_EQ(fault.code, ExecuteFaultCode::InvalidOperand);
    EXPECT_EQ(fault.instructionAddress, instructionAddress);
}

TEST_F(ControlOperationsTest, RSUB_RejectsAnyOperandOtherThanNone) {
    EXPECT_EQ(executeFault(Opcode::RSUB, MemoryAddress{target}).code,
              ExecuteFaultCode::InvalidOperand);
    EXPECT_EQ(executeFault(Opcode::RSUB, ImmediateValue{target}).code,
              ExecuteFaultCode::InvalidOperand);
}

TEST_F(ControlOperationsTest, ConditionalJumpsReportReservedConditionCodeAsFault) {
    registers.write(RegisterId::SW, 0x000003);

    for (const auto opcode : ConditionalJumps) {
        const auto fault = executeFault(opcode, MemoryAddress{target});

        EXPECT_EQ(fault.code, ExecuteFaultCode::InvalidOperand);
        EXPECT_EQ(fault.instructionAddress, instructionAddress);
        EXPECT_EQ(fault.offendingValue, 3);
    }
}

TEST_F(ControlOperationsTest, ReservedConditionCodeDoesNotAffectUnconditionalOperations) {
    registers.write(RegisterId::SW, 0x000003);
    registers.write(RegisterId::L, 0x000042);

    EXPECT_EQ(executeEffect(Opcode::J, MemoryAddress{target}).nextPc, target);
    EXPECT_EQ(executeEffect(Opcode::JSUB, MemoryAddress{target}).nextPc, target);
    EXPECT_EQ(executeEffect(Opcode::RSUB, NoOperand{}).nextPc, 0x000042U);
}

TEST_F(ControlOperationsTest, RejectsOpcodesOutsideTheControlFamily) {
    const auto fault = executeFault(Opcode::LDA, ImmediateValue{1});

    EXPECT_EQ(fault.code, ExecuteFaultCode::UnsupportedOpcode);
    EXPECT_EQ(fault.instructionAddress, instructionAddress);
}

TEST_F(ControlOperationsTest, DoesNotMutateMemoryOrRegisters) {
    registers.write(RegisterId::A, 0x111111);
    registers.write(RegisterId::L, 0x222222);
    registers.setConditionCode(ConditionCode::Equal);
    memory.writeWord(target, 0x333333);
    const auto swBefore = registers.read(RegisterId::SW);

    executeEffect(Opcode::JSUB, MemoryAddress{target});
    executeEffect(Opcode::JEQ, MemoryAddress{target});
    executeEffect(Opcode::RSUB, NoOperand{});

    EXPECT_EQ(registers.read(RegisterId::A), 0x111111U);
    EXPECT_EQ(registers.read(RegisterId::L), 0x222222U);
    EXPECT_EQ(registers.read(RegisterId::SW), swBefore);
    EXPECT_EQ(registers.read(RegisterId::PC), 0U);
    EXPECT_EQ(memory.readWord(target), 0x333333U);
}
