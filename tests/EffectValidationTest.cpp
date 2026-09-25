#include "core/EffectValidation.hpp"

#include "core/Memory.hpp"

#include <gtest/gtest.h>

class EffectValidationTest : public ::testing::Test {
protected:
    static constexpr std::uint32_t MemorySize = 0x100000;

    Memory memory{MemorySize};
    std::uint32_t postFetchPc = 0x1003;

    std::optional<CycleFault> validate(const ExecutionEffect& effect) const {
        return validateEffect(effect, postFetchPc, memory);
    }

    void expectFault(const ExecutionEffect& effect,
                     CycleFaultCode code,
                     std::optional<std::int64_t> offendingValue = std::nullopt) const {
        const auto fault = validate(effect);
        ASSERT_TRUE(fault.has_value());
        EXPECT_EQ(fault->code, code);
        EXPECT_EQ(fault->offendingValue, offendingValue);
    }
};

TEST_F(EffectValidationTest, AcceptsSequentialEffectWithoutWrites) {
    EXPECT_FALSE(validate(ExecutionEffect{postFetchPc}).has_value());
}

TEST_F(EffectValidationTest, AcceptsRegisterAndConditionCodeTogether) {
    ExecutionEffect effect{postFetchPc};
    effect.registerWrite = RegisterWrite{RegisterId::X, 0x000010};
    effect.conditionCode = ConditionCode::Greater;

    EXPECT_FALSE(validate(effect).has_value());
}

TEST_F(EffectValidationTest, AcceptsJumpToFirstAndLastAddress) {
    EXPECT_FALSE(validate(ExecutionEffect{0}).has_value());
    EXPECT_FALSE(validate(ExecutionEffect{MemorySize - 1}).has_value());
}

TEST_F(EffectValidationTest, RejectsNextPcAtOrAfterMemorySize) {
    expectFault(ExecutionEffect{MemorySize}, CycleFaultCode::NextPcOutOfRange, MemorySize);
    expectFault(ExecutionEffect{WordMask}, CycleFaultCode::NextPcOutOfRange, WordMask);
}

TEST_F(EffectValidationTest, RejectsNextPcBeyondArchitecturalWidthEvenInLargerMemory) {
    const Memory largeMemory{0x1000010};
    const ExecutionEffect effect{0x1000000};

    const auto fault = validateEffect(effect, postFetchPc, largeMemory);

    ASSERT_TRUE(fault.has_value());
    EXPECT_EQ(fault->code, CycleFaultCode::NextPcOutOfRange);
    EXPECT_EQ(fault->offendingValue, 0x1000000);
}

TEST_F(EffectValidationTest, AcceptsWordWriteEndingAtLastByte) {
    ExecutionEffect effect{postFetchPc};
    effect.memoryWrite = WordWrite{MemorySize - 3, 0x123456};

    EXPECT_FALSE(validate(effect).has_value());
}

TEST_F(EffectValidationTest, RejectsWordWriteCrossingMemoryEnd) {
    for (const std::uint32_t address : {MemorySize - 2, MemorySize - 1, MemorySize}) {
        ExecutionEffect effect{postFetchPc};
        effect.memoryWrite = WordWrite{address, 0x123456};

        expectFault(effect, CycleFaultCode::MemoryWriteOutOfRange, address);
    }
}

TEST_F(EffectValidationTest, AcceptsByteWriteAtLastByteAndRejectsAfterIt) {
    ExecutionEffect valid{postFetchPc};
    valid.memoryWrite = ByteWrite{MemorySize - 1, 0xAB};
    EXPECT_FALSE(validate(valid).has_value());

    ExecutionEffect invalid{postFetchPc};
    invalid.memoryWrite = ByteWrite{MemorySize, 0xAB};
    expectFault(invalid, CycleFaultCode::MemoryWriteOutOfRange, MemorySize);
}

TEST_F(EffectValidationTest, RejectsMemoryWriteWithAddressNearUnsignedLimit) {
    ExecutionEffect effect{postFetchPc};
    effect.memoryWrite = WordWrite{0xFFFFFFFF, 0x000001};

    expectFault(effect, CycleFaultCode::MemoryWriteOutOfRange, 0xFFFFFFFF);
}

TEST_F(EffectValidationTest, RegisterWriteToPcIsTheDestinationOfTheTransfer) {
    ExecutionEffect effect{postFetchPc};
    effect.registerWrite = RegisterWrite{RegisterId::PC, 0x002000};

    EXPECT_FALSE(validate(effect).has_value());
    EXPECT_EQ(finalPc(effect), 0x002000U);
}

TEST_F(EffectValidationTest, FinalPcIsNextPcWhenNoRegisterWriteTargetsPc) {
    ExecutionEffect effect{0x003000};
    EXPECT_EQ(finalPc(effect), 0x003000U);

    effect.registerWrite = RegisterWrite{RegisterId::A, 0x000005};
    EXPECT_EQ(finalPc(effect), 0x003000U);
}

TEST_F(EffectValidationTest, RejectsRegisterWriteToPcOutsideMemory) {
    ExecutionEffect effect{postFetchPc};
    effect.registerWrite = RegisterWrite{RegisterId::PC, 0x200000};

    expectFault(effect, CycleFaultCode::NextPcOutOfRange, 0x200000);
}

TEST_F(EffectValidationTest, RegisterWriteToPcDoesNotHideNextPcOutsideMemory) {
    postFetchPc = MemorySize;
    ExecutionEffect effect{postFetchPc};
    effect.registerWrite = RegisterWrite{RegisterId::PC, 0};

    expectFault(effect, CycleFaultCode::NextPcOutOfRange, MemorySize);
}

TEST_F(EffectValidationTest, RejectsRegisterWriteToPcCombinedWithAnotherNextPc) {
    ExecutionEffect effect{0x002000};
    effect.registerWrite = RegisterWrite{RegisterId::PC, 0x003000};

    expectFault(effect, CycleFaultCode::ConflictingEffect);
}

TEST_F(EffectValidationTest, AcceptsRegisterWriteToSwAloneButNotWithConditionCode) {
    ExecutionEffect alone{postFetchPc};
    alone.registerWrite = RegisterWrite{RegisterId::SW, 0x000100};
    EXPECT_FALSE(validate(alone).has_value());

    ExecutionEffect conflicting{postFetchPc};
    conflicting.registerWrite = RegisterWrite{RegisterId::SW, 0x000100};
    conflicting.conditionCode = ConditionCode::Less;
    expectFault(conflicting, CycleFaultCode::ConflictingEffect);
}

TEST_F(EffectValidationTest, RejectsUnknownRegisterIdentifier) {
    for (const std::uint8_t code : {6, 7, 10, 255}) {
        ExecutionEffect effect{postFetchPc};
        effect.registerWrite = RegisterWrite{static_cast<RegisterId>(code), 0x000001};

        expectFault(effect, CycleFaultCode::InvalidRegisterWrite, code);
    }
}

TEST_F(EffectValidationTest, RejectsUnknownConditionCode) {
    ExecutionEffect effect{postFetchPc};
    effect.conditionCode = static_cast<ConditionCode>(3);

    expectFault(effect, CycleFaultCode::InvalidConditionCode, 3);
}

TEST_F(EffectValidationTest, DoesNotModifyMemoryWhileValidating) {
    ExecutionEffect effect{postFetchPc};
    effect.memoryWrite = WordWrite{0x100, 0xABCDEF};

    ASSERT_FALSE(validate(effect).has_value());

    EXPECT_EQ(memory.readWord(0x100), 0U);
}
