#include "core/AddressResolver.hpp"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <variant>

namespace {
template <typename Operand> const Operand* resolvedAs(const ResolveResult& result) {
    const auto* resolvedOperand = std::get_if<ResolvedOperand>(&result);
    if (resolvedOperand == nullptr) {
        return nullptr;
    }
    return std::get_if<Operand>(resolvedOperand);
}

const ResolveFault* faultFrom(const ResolveResult& result) {
    return std::get_if<ResolveFault>(&result);
}

DecodedInstruction memoryInstruction(InstructionFormat format,
                                     std::uint32_t encodedOperand,
                                     bool n = true,
                                     bool i = true) {
    DecodedInstruction instruction{};
    instruction.opcode = Opcode::LDA;
    instruction.format = format;
    instruction.n = n;
    instruction.i = i;
    instruction.encodedOperand = encodedOperand;
    return instruction;
}
} // namespace

TEST(AddressResolverTest, ResolvesOneAndTwoRegisterOperands) {
    Memory memory(1);
    RegisterFile registers;

    DecodedInstruction clear{};
    clear.opcode = Opcode::CLEAR;
    clear.format = InstructionFormat::Format2;
    clear.r1 = 1;
    const auto clearResult = AddressResolver{}.resolve(clear, 0, memory, registers);
    const auto* clearOperands = resolvedAs<RegisterOperands>(clearResult);
    ASSERT_NE(clearOperands, nullptr);
    EXPECT_EQ(clearOperands->r1, RegisterId::X);
    EXPECT_FALSE(clearOperands->r2.has_value());
    EXPECT_FALSE(clearOperands->encodedNibble.has_value());

    DecodedInstruction addRegisters{};
    addRegisters.opcode = Opcode::ADDR;
    addRegisters.format = InstructionFormat::Format2;
    addRegisters.r1 = 0;
    addRegisters.r2 = 4;
    const auto addResult = AddressResolver{}.resolve(addRegisters, 0, memory, registers);
    const auto* addOperands = resolvedAs<RegisterOperands>(addResult);
    ASSERT_NE(addOperands, nullptr);
    EXPECT_EQ(addOperands->r1, RegisterId::A);
    ASSERT_TRUE(addOperands->r2.has_value());
    EXPECT_EQ(*addOperands->r2, RegisterId::S);
}

TEST(AddressResolverTest, ResolvesContractForEveryFormat2Opcode) {
    enum class OperandShape {
        OneRegister,
        TwoRegisters,
        RegisterAndNibble,
    };

    struct Case {
        Opcode opcode;
        OperandShape shape;
    };

    constexpr std::array cases{
        Case{Opcode::ADDR, OperandShape::TwoRegisters},
        Case{Opcode::CLEAR, OperandShape::OneRegister},
        Case{Opcode::COMPR, OperandShape::TwoRegisters},
        Case{Opcode::DIVR, OperandShape::TwoRegisters},
        Case{Opcode::MULR, OperandShape::TwoRegisters},
        Case{Opcode::RMO, OperandShape::TwoRegisters},
        Case{Opcode::SHIFTL, OperandShape::RegisterAndNibble},
        Case{Opcode::SHIFTR, OperandShape::RegisterAndNibble},
        Case{Opcode::SUBR, OperandShape::TwoRegisters},
        Case{Opcode::TIXR, OperandShape::OneRegister},
    };

    Memory memory(1);
    RegisterFile registers;
    for (const auto& testCase : cases) {
        DecodedInstruction instruction{};
        instruction.opcode = testCase.opcode;
        instruction.format = InstructionFormat::Format2;
        instruction.r1 = 0;
        instruction.r2 = testCase.shape == OperandShape::RegisterAndNibble ? 15 : 1;

        const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);
        const auto* operands = resolvedAs<RegisterOperands>(result);
        ASSERT_NE(operands, nullptr);
        EXPECT_EQ(operands->r1, RegisterId::A);

        if (testCase.shape == OperandShape::TwoRegisters) {
            ASSERT_TRUE(operands->r2.has_value());
            EXPECT_EQ(*operands->r2, RegisterId::X);
            EXPECT_FALSE(operands->encodedNibble.has_value());
        } else if (testCase.shape == OperandShape::RegisterAndNibble) {
            EXPECT_FALSE(operands->r2.has_value());
            ASSERT_TRUE(operands->encodedNibble.has_value());
            EXPECT_EQ(*operands->encodedNibble, 15);
        } else {
            EXPECT_FALSE(operands->r2.has_value());
            EXPECT_FALSE(operands->encodedNibble.has_value());
        }
    }
}

TEST(AddressResolverTest, PreservesEncodedShiftNibbleWithoutTreatingItAsARegister) {
    Memory memory(1);
    RegisterFile registers;
    DecodedInstruction instruction{};
    instruction.opcode = Opcode::SHIFTL;
    instruction.format = InstructionFormat::Format2;
    instruction.r1 = 2;
    instruction.r2 = 15;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operands = resolvedAs<RegisterOperands>(result);
    ASSERT_NE(operands, nullptr);
    EXPECT_EQ(operands->r1, RegisterId::L);
    EXPECT_FALSE(operands->r2.has_value());
    ASSERT_TRUE(operands->encodedNibble.has_value());
    EXPECT_EQ(*operands->encodedNibble, 15);
}

TEST(AddressResolverTest, ReportsInvalidFormat2Register) {
    Memory memory(1);
    RegisterFile registers;
    DecodedInstruction instruction{};
    instruction.opcode = Opcode::ADDR;
    instruction.format = InstructionFormat::Format2;
    instruction.r1 = 0;
    instruction.r2 = 6;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* fault = faultFrom(result);
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->code, ResolveFaultCode::InvalidRegister);
    EXPECT_EQ(fault->calculatedAddress, 6);
}

TEST(AddressResolverTest, ReportsInvalidFirstFormat2Register) {
    Memory memory(1);
    RegisterFile registers;
    DecodedInstruction instruction{};
    instruction.opcode = Opcode::CLEAR;
    instruction.format = InstructionFormat::Format2;
    instruction.r1 = 7;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* fault = faultFrom(result);
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->code, ResolveFaultCode::InvalidRegister);
    EXPECT_EQ(fault->calculatedAddress, 7);
}

TEST(AddressResolverTest, ResolvesRsubAsOperandless) {
    Memory memory(1);
    RegisterFile registers;
    DecodedInstruction instruction{};
    instruction.opcode = Opcode::RSUB;
    instruction.format = InstructionFormat::Format3;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    EXPECT_NE(resolvedAs<NoOperand>(result), nullptr);
}

TEST(AddressResolverTest, ResolvesSicDirectAndIndexedAddresses) {
    Memory memory(0x8000);
    RegisterFile registers;
    auto instruction = memoryInstruction(InstructionFormat::Sic, 0x1234, false, false);

    auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    const auto* direct = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(direct, nullptr);
    EXPECT_EQ(direct->address, 0x1234);

    registers.setX(0x10);
    instruction.x = true;
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    const auto* indexed = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(indexed, nullptr);
    EXPECT_EQ(indexed->address, 0x1244);
}

TEST(AddressResolverTest, ResolvesFormat3DirectAddress) {
    Memory memory(0x1000);
    RegisterFile registers;
    const auto instruction = memoryInstruction(InstructionFormat::Format3, 0xABC);

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operand = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->address, 0xABC);
}

TEST(AddressResolverTest, SignExtendsPositiveZeroAndNegativePcRelativeDisplacements) {
    Memory memory(0x1000);
    RegisterFile registers;

    auto instruction = memoryInstruction(InstructionFormat::Format3, 0x010);
    instruction.p = true;
    auto result = AddressResolver{}.resolve(instruction, 0x100, memory, registers);
    ASSERT_NE(resolvedAs<MemoryAddress>(result), nullptr);
    EXPECT_EQ(resolvedAs<MemoryAddress>(result)->address, 0x110);

    instruction.encodedOperand = 0;
    result = AddressResolver{}.resolve(instruction, 0x100, memory, registers);
    ASSERT_NE(resolvedAs<MemoryAddress>(result), nullptr);
    EXPECT_EQ(resolvedAs<MemoryAddress>(result)->address, 0x100);

    instruction.encodedOperand = 0xFFF;
    result = AddressResolver{}.resolve(instruction, 0x100, memory, registers);
    ASSERT_NE(resolvedAs<MemoryAddress>(result), nullptr);
    EXPECT_EQ(resolvedAs<MemoryAddress>(result)->address, 0x0FF);
}

TEST(AddressResolverTest, ResolvesBaseRelativeAddress) {
    Memory memory(0x1000);
    RegisterFile registers;
    registers.setB(0x200);
    auto instruction = memoryInstruction(InstructionFormat::Format3, 0x020);
    instruction.b = true;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operand = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->address, 0x220);
}

TEST(AddressResolverTest, ResolvesFormat4Address) {
    Memory memory(1U << 20);
    RegisterFile registers;
    const auto instruction = memoryInstruction(InstructionFormat::Format4, 0xABCDE);

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operand = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->address, 0xABCDE);
}

TEST(AddressResolverTest, AppliesIndexAfterPrimaryAddressCalculation) {
    Memory memory(256);
    RegisterFile registers;
    registers.setX(23);
    auto instruction = memoryInstruction(InstructionFormat::Format3, 100);
    instruction.x = true;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operand = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->address, 123);
}

TEST(AddressResolverTest, ResolvesEveryAddressingCombinationAcceptedByDecoder) {
    enum class ExpectedKind {
        Memory,
        Immediate,
    };

    struct Case {
        InstructionFormat format;
        std::uint32_t encodedOperand;
        bool n;
        bool i;
        bool x;
        bool b;
        bool p;
        ExpectedKind expectedKind;
        std::uint32_t expectedValue;
    };

    Memory memory(2048);
    RegisterFile registers;
    registers.setB(400);
    registers.setX(10);
    memory.writeWord(20, 1000);
    memory.writeWord(30, 1001);
    memory.writeWord(200, 1002);
    memory.writeWord(401, 1003);

    constexpr std::array cases{
        Case{InstructionFormat::Format3,
             50,
             true,
             true,
             false,
             false,
             false,
             ExpectedKind::Memory,
             50},
        Case{InstructionFormat::Format3,
             50,
             true,
             true,
             true,
             false,
             false,
             ExpectedKind::Memory,
             60},
        Case{InstructionFormat::Format3,
             10,
             true,
             true,
             false,
             false,
             true,
             ExpectedKind::Memory,
             210},
        Case{InstructionFormat::Format3,
             10,
             true,
             true,
             false,
             true,
             false,
             ExpectedKind::Memory,
             410},
        Case{InstructionFormat::Format4,
             500,
             true,
             true,
             false,
             false,
             false,
             ExpectedKind::Memory,
             500},
        Case{InstructionFormat::Format4,
             500,
             true,
             true,
             true,
             false,
             false,
             ExpectedKind::Memory,
             510},
        Case{InstructionFormat::Format3,
             20,
             true,
             false,
             false,
             false,
             false,
             ExpectedKind::Memory,
             1000},
        Case{InstructionFormat::Format3,
             0,
             true,
             false,
             false,
             false,
             true,
             ExpectedKind::Memory,
             1002},
        Case{InstructionFormat::Format3,
             1,
             true,
             false,
             false,
             true,
             false,
             ExpectedKind::Memory,
             1003},
        Case{InstructionFormat::Format4,
             30,
             true,
             false,
             false,
             false,
             false,
             ExpectedKind::Memory,
             1001},
        Case{InstructionFormat::Format3,
             50,
             false,
             true,
             false,
             false,
             false,
             ExpectedKind::Immediate,
             50},
        Case{InstructionFormat::Format3,
             10,
             false,
             true,
             false,
             false,
             true,
             ExpectedKind::Immediate,
             210},
        Case{InstructionFormat::Format3,
             10,
             false,
             true,
             false,
             true,
             false,
             ExpectedKind::Immediate,
             410},
        Case{InstructionFormat::Format4,
             500,
             false,
             true,
             false,
             false,
             false,
             ExpectedKind::Immediate,
             500},
    };

    for (const auto& testCase : cases) {
        auto instruction =
            memoryInstruction(testCase.format, testCase.encodedOperand, testCase.n, testCase.i);
        instruction.x = testCase.x;
        instruction.b = testCase.b;
        instruction.p = testCase.p;

        const auto result = AddressResolver{}.resolve(instruction, 200, memory, registers);
        if (testCase.expectedKind == ExpectedKind::Memory) {
            const auto* operand = resolvedAs<MemoryAddress>(result);
            ASSERT_NE(operand, nullptr);
            EXPECT_EQ(operand->address, testCase.expectedValue);
        } else {
            const auto* operand = resolvedAs<ImmediateValue>(result);
            ASSERT_NE(operand, nullptr);
            EXPECT_EQ(operand->value, testCase.expectedValue);
        }
    }
}

TEST(AddressResolverTest, ReturnsImmediateValueWithoutReadingFinalMemory) {
    Memory memory(32);
    RegisterFile registers;
    memory.writeWord(5, 0xABCDEF);
    const auto instruction = memoryInstruction(InstructionFormat::Format3, 5, false, true);

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operand = resolvedAs<ImmediateValue>(result);
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->value, 5);
}

TEST(AddressResolverTest, FollowsIndirectWordAsMemoryAddress) {
    Memory memory(64);
    RegisterFile registers;
    memory.writeWord(10, 50);
    const auto instruction = memoryInstruction(InstructionFormat::Format3, 10, true, false);

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* operand = resolvedAs<MemoryAddress>(result);
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->address, 50);
}

TEST(AddressResolverTest, ReportsIndirectPointerOutsideMemory) {
    Memory memory(64);
    RegisterFile registers;
    memory.writeWord(10, 64);
    const auto instruction = memoryInstruction(InstructionFormat::Format3, 10, true, false);

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* fault = faultFrom(result);
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->code, ResolveFaultCode::AddressOutOfRange);
    EXPECT_EQ(fault->calculatedAddress, 64);
}

TEST(AddressResolverTest, ReportsIndirectWordCrossingMemoryEnd) {
    Memory memory(64);
    RegisterFile registers;
    const auto instruction = memoryInstruction(InstructionFormat::Format3, 62, true, false);

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* fault = faultFrom(result);
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->code, ResolveFaultCode::IndirectReadOutOfRange);
    EXPECT_EQ(fault->calculatedAddress, 62);
}

TEST(AddressResolverTest, ValidatesLowerAndUpperAddressLimitsWithoutWrapping) {
    Memory memory(64);
    RegisterFile registers;
    auto instruction = memoryInstruction(InstructionFormat::Format3, 0);
    instruction.p = true;
    instruction.encodedOperand = 0xFFF;

    auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    const auto* negativeFault = faultFrom(result);
    ASSERT_NE(negativeFault, nullptr);
    EXPECT_EQ(negativeFault->code, ResolveFaultCode::AddressOutOfRange);
    EXPECT_EQ(negativeFault->calculatedAddress, -1);

    instruction.p = false;
    instruction.x = true;
    registers.setX(63);
    instruction.encodedOperand = 0;
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(resolvedAs<MemoryAddress>(result), nullptr);
    EXPECT_EQ(resolvedAs<MemoryAddress>(result)->address, 63);

    registers.setX(64);
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    const auto* upperFault = faultFrom(result);
    ASSERT_NE(upperFault, nullptr);
    EXPECT_EQ(upperFault->code, ResolveFaultCode::AddressOutOfRange);
    EXPECT_EQ(upperFault->calculatedAddress, 64);
}

TEST(AddressResolverTest, ReportsBaseRelativeAddressOutsideMemory) {
    Memory memory(64);
    RegisterFile registers;
    registers.setB(63);
    auto instruction = memoryInstruction(InstructionFormat::Format3, 1);
    instruction.b = true;

    const auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);

    const auto* fault = faultFrom(result);
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->code, ResolveFaultCode::AddressOutOfRange);
    EXPECT_EQ(fault->calculatedAddress, 64);
}

TEST(AddressResolverTest, RejectsMalformedDecodedInstructionContracts) {
    Memory memory(1U << 20);
    RegisterFile registers;

    auto instruction = memoryInstruction(InstructionFormat::Sic, 0x8000, false, false);
    auto result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(faultFrom(result), nullptr);
    EXPECT_EQ(faultFrom(result)->code, ResolveFaultCode::InvalidInstruction);

    instruction = memoryInstruction(InstructionFormat::Format3, 0x1000);
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(faultFrom(result), nullptr);
    EXPECT_EQ(faultFrom(result)->code, ResolveFaultCode::InvalidInstruction);

    instruction = memoryInstruction(InstructionFormat::Format4, 0x100000);
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(faultFrom(result), nullptr);
    EXPECT_EQ(faultFrom(result)->code, ResolveFaultCode::InvalidInstruction);

    instruction = memoryInstruction(InstructionFormat::Format3, 0, false, false);
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(faultFrom(result), nullptr);
    EXPECT_EQ(faultFrom(result)->code, ResolveFaultCode::InvalidInstruction);

    instruction = memoryInstruction(InstructionFormat::Format4, 0);
    instruction.opcode = Opcode::RSUB;
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(faultFrom(result), nullptr);
    EXPECT_EQ(faultFrom(result)->code, ResolveFaultCode::InvalidInstruction);

    instruction = {};
    instruction.opcode = Opcode::LDA;
    instruction.format = InstructionFormat::Format2;
    result = AddressResolver{}.resolve(instruction, 0, memory, registers);
    ASSERT_NE(faultFrom(result), nullptr);
    EXPECT_EQ(faultFrom(result)->code, ResolveFaultCode::InvalidInstruction);
}

TEST(AddressResolverTest, DoesNotMutateRegistersOrMemoryOnSuccessOrFailure) {
    Memory memory(64);
    memory.writeWord(10, 50);
    RegisterFile registers;
    registers.setA(0x123456);
    registers.setX(7);
    registers.setB(9);
    registers.setSW(0x654321);

    auto instruction = memoryInstruction(InstructionFormat::Format3, 10, true, false);
    const auto success = AddressResolver{}.resolve(instruction, 0, memory, registers);
    EXPECT_NE(resolvedAs<MemoryAddress>(success), nullptr);

    instruction.encodedOperand = 63;
    const auto failure = AddressResolver{}.resolve(instruction, 0, memory, registers);
    EXPECT_NE(faultFrom(failure), nullptr);

    EXPECT_EQ(memory.readWord(10), 50);
    EXPECT_EQ(registers.getA(), 0x123456);
    EXPECT_EQ(registers.getX(), 7);
    EXPECT_EQ(registers.getB(), 9);
    EXPECT_EQ(registers.getSW(), 0x654321);
}
