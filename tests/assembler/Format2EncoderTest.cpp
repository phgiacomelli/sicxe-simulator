#include "assembler/Format2Encoder.hpp"

#include "InstructionTestSupport.hpp"

#include <gtest/gtest.h>

namespace assembler {
namespace {
struct Format2Case {
    const char* source;
    const char* name;
    Format2Bytes bytes;
};
class Format2EncoderTest : public testing::TestWithParam<Format2Case> {};
TEST_P(Format2EncoderTest, EmitsManuallyExpectedBytes) {
    const auto& input = GetParam();
    const auto parsed = test::operand(input.source);
    const auto result = encodeFormat2(test::spec(input.name), parsed);
    ASSERT_TRUE(std::holds_alternative<Format2Bytes>(result));
    EXPECT_EQ(std::get<Format2Bytes>(result), input.bytes);
}
INSTANTIATE_TEST_SUITE_P(AllOpcodesAndBoundaries,
                         Format2EncoderTest,
                         testing::Values(Format2Case{"ADDR A,X", "ADDR", {0x90, 0x01}},
                                         Format2Case{"CLEAR SW", "CLEAR", {0xB4, 0x90}},
                                         Format2Case{"COMPR PC,SW", "COMPR", {0xA0, 0x89}},
                                         Format2Case{"DIVR L,B", "DIVR", {0x9C, 0x23}},
                                         Format2Case{"MULR S,T", "MULR", {0x98, 0x45}},
                                         Format2Case{"RMO X,A", "RMO", {0xAC, 0x10}},
                                         Format2Case{"SHIFTL A,1", "SHIFTL", {0xA4, 0x00}},
                                         Format2Case{"SHIFTL T,16", "SHIFTL", {0xA4, 0x5F}},
                                         Format2Case{"SHIFTR SW,0x10", "SHIFTR", {0xA8, 0x9F}},
                                         Format2Case{"SHIFTR PC,1", "SHIFTR", {0xA8, 0x80}},
                                         Format2Case{"SUBR B,S", "SUBR", {0x94, 0x34}},
                                         Format2Case{"TIXR L", "TIXR", {0xB8, 0x20}}));

TEST(Format2EncoderValidationTest, RejectsWrongFamilyFormatAndDescriptor) {
    const auto memory = test::operand("LDA #1");
    EXPECT_EQ(std::get<InstructionDiagnostic>(encodeFormat2(test::spec("LDA"), memory)).code,
              InstructionDiagnosticCode::InvalidFormat2Operand);
    auto operand = test::operand("CLEAR A");
    operand.form = InstructionForm::Format4;
    EXPECT_EQ(std::get<InstructionDiagnostic>(encodeFormat2(test::spec("CLEAR"), operand)).code,
              InstructionDiagnosticCode::InvalidFormat2Operand);
    operand = test::operand("CLEAR A");
    operand.data = TwoRegisterOperand{{InstructionRegister::A, {"A", {2, 8}}},
                                      {InstructionRegister::X, {"X", {2, 10}}}};
    EXPECT_EQ(std::get<InstructionDiagnostic>(encodeFormat2(test::spec("CLEAR"), operand)).code,
              InstructionDiagnosticCode::InvalidFormat2Operand);
    EXPECT_EQ(std::get<InstructionDiagnostic>(encodeFormat2(test::spec("ADDR"), operand)).code,
              InstructionDiagnosticCode::InstructionMismatch);
    auto spec = test::spec("CLEAR");
    spec.opcode = 0xFF;
    EXPECT_EQ(std::get<InstructionDiagnostic>(encodeFormat2(spec, test::operand("CLEAR A"))).code,
              InstructionDiagnosticCode::InstructionMismatch);
}

TEST(Format2EncoderValidationTest,
     RejectsManuallyConstructedInvalidRegistersAndShiftCountsAtTheirTokens) {
    for (std::uint8_t code : {6, 7, 10, 15, 255}) {
        auto operand = test::operand("RMO A,X");
        auto& pair = std::get<TwoRegisterOperand>(operand.data);
        pair.second.reg = static_cast<InstructionRegister>(code);
        const auto result = encodeFormat2(test::spec("RMO"), operand);
        ASSERT_TRUE(std::holds_alternative<InstructionDiagnostic>(result));
        EXPECT_EQ(std::get<InstructionDiagnostic>(result).code,
                  InstructionDiagnosticCode::InvalidRegister);
        EXPECT_EQ(std::get<InstructionDiagnostic>(result).source.position, (SourcePosition{2, 8}));
        pair.first.reg = pair.second.reg;
        pair.second.reg = InstructionRegister::X;
        EXPECT_EQ(std::get<InstructionDiagnostic>(encodeFormat2(test::spec("RMO"), operand))
                      .source.position,
                  (SourcePosition{2, 6}));
    }
    for (std::uint8_t count : {0, 17, 255}) {
        auto operand = test::operand("SHIFTL A,1");
        std::get<RegisterCountOperand>(operand.data).count = count;
        const auto result = encodeFormat2(test::spec("SHIFTL"), operand);
        EXPECT_EQ(std::get<InstructionDiagnostic>(result).code,
                  InstructionDiagnosticCode::InvalidShiftCount);
        EXPECT_EQ(std::get<InstructionDiagnostic>(result).source.position, (SourcePosition{2, 11}));
    }
}
} // namespace
} // namespace assembler
