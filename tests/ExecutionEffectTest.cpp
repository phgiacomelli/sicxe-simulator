#include "core/ExecutionEffect.hpp"

#include <gtest/gtest.h>
#include <variant>

TEST(ExecutionEffectTest, StartsWithSequentialPcAndNoArchitecturalWrites) {
    const ExecutionEffect effect{0x001003};

    EXPECT_EQ(effect.nextPc, 0x001003);
    EXPECT_FALSE(effect.registerWrite.has_value());
    EXPECT_FALSE(effect.memoryWrite.has_value());
    EXPECT_FALSE(effect.conditionCode.has_value());
}

TEST(ExecutionEffectTest, RepresentsRegisterAndConditionCodeChangesTogether) {
    ExecutionEffect effect{0x001003};
    effect.registerWrite = RegisterWrite{RegisterId::X, 0xAB123456};
    effect.conditionCode = ConditionCode::Greater;

    ASSERT_TRUE(effect.registerWrite.has_value());
    EXPECT_EQ(effect.registerWrite->target, RegisterId::X);
    EXPECT_EQ(effect.registerWrite->value, 0x123456);
    EXPECT_EQ(effect.conditionCode, ConditionCode::Greater);
}

TEST(ExecutionEffectTest, DistinguishesByteAndWordMemoryWrites) {
    ExecutionEffect byteEffect{3};
    byteEffect.memoryWrite = ByteWrite{10, 0xAB};
    ASSERT_TRUE(byteEffect.memoryWrite.has_value());
    ASSERT_NE(std::get_if<ByteWrite>(&*byteEffect.memoryWrite), nullptr);
    EXPECT_EQ(std::get<ByteWrite>(*byteEffect.memoryWrite).value, 0xAB);

    ExecutionEffect wordEffect{3};
    wordEffect.memoryWrite = WordWrite{20, 0xAB123456};
    ASSERT_TRUE(wordEffect.memoryWrite.has_value());
    ASSERT_NE(std::get_if<WordWrite>(&*wordEffect.memoryWrite), nullptr);
    EXPECT_EQ(std::get<WordWrite>(*wordEffect.memoryWrite).value, 0x123456);
}

TEST(ExecutionEffectTest, RepresentsStructuredExecutionFaultsWithoutPartialEffects) {
    const ExecuteResult result = ExecuteFault{
        ExecuteFaultCode::MemoryAccessOutOfRange,
        0x001000,
        0x100000,
    };

    const auto* fault = std::get_if<ExecuteFault>(&result);
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->code, ExecuteFaultCode::MemoryAccessOutOfRange);
    EXPECT_EQ(fault->instructionAddress, 0x001000);
    EXPECT_EQ(fault->offendingValue, 0x100000);
}

TEST(ExecutionEffectTest, ComputesPostFetchPcWithoutArchitecturalWrap) {
    EXPECT_EQ(computePostFetchPc(0x001000, 3), 0x001003);
    EXPECT_EQ(computePostFetchPc(WordMask - 4, 4), WordMask);
    EXPECT_FALSE(computePostFetchPc(WordMask, 1).has_value());
    EXPECT_FALSE(computePostFetchPc(0x001000, 0).has_value());
}
