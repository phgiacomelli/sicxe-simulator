#include "core/RegisterFile.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>

TEST(RegisterFileTest, ResetClearsEveryArchitecturalRegister) {
    RegisterFile registers;
    registers.setA(1);
    registers.setX(2);
    registers.setL(3);
    registers.setB(4);
    registers.setS(5);
    registers.setT(6);
    registers.setF(7);
    registers.setPC(8);
    registers.setSW(9);

    registers.reset();

    EXPECT_EQ(registers.getA(), 0);
    EXPECT_EQ(registers.getX(), 0);
    EXPECT_EQ(registers.getL(), 0);
    EXPECT_EQ(registers.getB(), 0);
    EXPECT_EQ(registers.getS(), 0);
    EXPECT_EQ(registers.getT(), 0);
    EXPECT_EQ(registers.getF(), 0);
    EXPECT_EQ(registers.getPC(), 0);
    EXPECT_EQ(registers.getSW(), 0);
    EXPECT_EQ(registers.getConditionCode(), ConditionCode::Equal);
}

TEST(RegisterFileTest, MasksIntegerAndFloatingPointWritesToArchitecturalWidths) {
    RegisterFile registers;

    constexpr std::array integerRegisters{
        RegisterId::A,
        RegisterId::X,
        RegisterId::L,
        RegisterId::B,
        RegisterId::S,
        RegisterId::T,
        RegisterId::PC,
        RegisterId::SW,
    };
    for (const auto id : integerRegisters) {
        registers.write(id, 0xAB123456);
        EXPECT_EQ(registers.read(id), 0x123456);
    }

    registers.setF(0xAB123456789ABCULL);
    EXPECT_EQ(registers.getF(), 0x123456789ABCULL);
}

TEST(RegisterFileTest, ConvertsEveryDecoderRegisterCodeToTypedAccess) {
    RegisterFile registers;
    constexpr std::array codes{std::uint8_t{0},
                               std::uint8_t{1},
                               std::uint8_t{2},
                               std::uint8_t{3},
                               std::uint8_t{4},
                               std::uint8_t{5},
                               std::uint8_t{8},
                               std::uint8_t{9}};

    for (std::size_t index = 0; index < codes.size(); ++index) {
        const auto id = registerIdFromCode(codes[index]);
        ASSERT_TRUE(id.has_value());
        registers.write(*id, static_cast<std::uint32_t>(index + 1));
        EXPECT_EQ(registers.read(*id), index + 1);
    }

    EXPECT_FALSE(registerIdFromCode(6).has_value());
    EXPECT_FALSE(registerIdFromCode(7).has_value());
    EXPECT_FALSE(registerIdFromCode(10).has_value());
}

TEST(RegisterFileTest, StoresEveryConditionCodeAndPreservesOtherStatusBits) {
    RegisterFile registers;
    constexpr auto unrelatedBits = std::uint32_t{0xA5A5FC};
    registers.setSW(unrelatedBits);

    registers.setConditionCode(ConditionCode::Less);
    EXPECT_EQ(registers.getConditionCode(), ConditionCode::Less);
    EXPECT_EQ(registers.getSW() & ConditionCodeMask, 0b10);
    EXPECT_EQ(registers.getSW() & ~ConditionCodeMask, unrelatedBits & ~ConditionCodeMask);

    registers.setConditionCode(ConditionCode::Equal);
    EXPECT_EQ(registers.getConditionCode(), ConditionCode::Equal);
    EXPECT_EQ(registers.getSW() & ConditionCodeMask, 0b00);
    EXPECT_EQ(registers.getSW() & ~ConditionCodeMask, unrelatedBits & ~ConditionCodeMask);

    registers.setConditionCode(ConditionCode::Greater);
    EXPECT_EQ(registers.getConditionCode(), ConditionCode::Greater);
    EXPECT_EQ(registers.getSW() & ConditionCodeMask, 0b01);
    EXPECT_EQ(registers.getSW() & ~ConditionCodeMask, unrelatedBits & ~ConditionCodeMask);
}

TEST(RegisterFileTest, RejectsReservedConditionCodeEncoding) {
    RegisterFile registers;
    registers.setSW(0b11);

    EXPECT_THROW(registers.getConditionCode(), std::logic_error);
}

TEST(RegisterFileTest, RejectsUnknownTypedRegisterIdentifier) {
    RegisterFile registers;
    const auto unknown = static_cast<RegisterId>(6);

    EXPECT_THROW(registers.read(unknown), std::invalid_argument);
    EXPECT_THROW(registers.write(unknown, 1), std::invalid_argument);
}
