#include "InstructionTestSupport.hpp"
#include "assembler/Format2Encoder.hpp"
#include "core/Decoder.hpp"
#include "core/Memory.hpp"

#include <array>
#include <gtest/gtest.h>
#include <string>

namespace assembler {
namespace {
struct RegisterCase {
    const char* name;
    std::uint8_t code;
};
constexpr std::array Registers{RegisterCase{"A", 0},
                               RegisterCase{"X", 1},
                               RegisterCase{"L", 2},
                               RegisterCase{"B", 3},
                               RegisterCase{"S", 4},
                               RegisterCase{"T", 5},
                               RegisterCase{"PC", 8},
                               RegisterCase{"SW", 9}};
class Format2DecoderTest : public testing::TestWithParam<test::ExpectedInstruction> {};
TEST_P(Format2DecoderTest, RoundTripsEveryValidRegisterAndCount) {
    const auto& expected = GetParam();
    if (expected.formats != InstructionFormats::Format2) {
        FAIL() << "Invalid test target";
    }
    for (const auto& first : Registers) {
        const auto possibilities =
            expected.operands == OperandPattern::OneRegister
                ? 1u
                : (expected.operands == OperandPattern::TwoRegisters ? Registers.size() : 16u);
        for (std::size_t i = 0; i < possibilities; ++i) {
            std::string source = std::string{expected.name} + " " + first.name;
            std::uint8_t low = 0;
            if (expected.operands == OperandPattern::TwoRegisters) {
                source += "," + std::string{Registers[i].name};
                low = Registers[i].code;
            } else if (expected.operands == OperandPattern::RegisterAndCount) {
                source += "," + std::to_string(i + 1);
                low = static_cast<std::uint8_t>(i);
            }
            SCOPED_TRACE(source);
            const auto result = encodeFormat2(test::spec(expected.name), test::operand(source));
            ASSERT_TRUE(std::holds_alternative<Format2Bytes>(result));
            const auto bytes = std::get<Format2Bytes>(result);
            EXPECT_EQ(bytes,
                      (Format2Bytes{expected.opcode,
                                    static_cast<std::uint8_t>((first.code << 4) | low)}));
            Memory memory{2};
            memory.writeByte(0, bytes[0]);
            memory.writeByte(1, bytes[1]);
            const auto decoded = Decoder{}.decode(memory, 0);
            ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(decoded));
            const auto& instruction = std::get<DecodedInstruction>(decoded);
            EXPECT_EQ(instruction.opcode, expected.vmOpcode);
            EXPECT_EQ(instruction.format, InstructionFormat::Format2);
            EXPECT_EQ(instruction.length, 2);
            EXPECT_EQ(instruction.r1, first.code);
            EXPECT_EQ(instruction.r2, low);
        }
    }
}
std::vector<test::ExpectedInstruction> format2Instructions() {
    std::vector<test::ExpectedInstruction> result;
    for (const auto& expected : test::ExpectedIsa) {
        if (expected.formats == InstructionFormats::Format2) {
            result.push_back(expected);
        }
    }
    return result;
}
INSTANTIATE_TEST_SUITE_P(AllTenOpcodes,
                         Format2DecoderTest,
                         testing::ValuesIn(format2Instructions()),
                         [](const auto& info) { return std::string{info.param.name}; });

TEST(InstructionCatalogDecoderTest, EveryMemoryOpcodeAndAdvertisedFormatMatchesDecoder) {
    for (const auto& expected : test::ExpectedIsa) {
        if (expected.formats == InstructionFormats::Format2) {
            continue;
        }
        SCOPED_TRACE(expected.name);
        const auto& spec = test::spec(expected.name);
        // Only hand-built compatibility probes: package 2 does not encode formats 3/4.
        Memory memory{4};
        memory.writeByte(0, static_cast<std::uint8_t>(spec.opcode | 3));
        const auto format3 = Decoder{}.decode(memory, 0);
        ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(format3));
        EXPECT_EQ(std::get<DecodedInstruction>(format3).opcode, expected.vmOpcode);
        EXPECT_EQ(std::get<DecodedInstruction>(format3).length, 3);
        memory.writeByte(1, 0x10);
        const auto format4 = Decoder{}.decode(memory, 0);
        if (spec.supportsExtended()) {
            ASSERT_TRUE(std::holds_alternative<DecodedInstruction>(format4));
            EXPECT_EQ(std::get<DecodedInstruction>(format4).opcode, expected.vmOpcode);
            EXPECT_EQ(std::get<DecodedInstruction>(format4).format, InstructionFormat::Format4);
            EXPECT_EQ(std::get<DecodedInstruction>(format4).length, 4);
        } else {
            ASSERT_EQ(expected.vmOpcode, Opcode::RSUB);
            ASSERT_TRUE(std::holds_alternative<DecodeFault>(format4));
            EXPECT_EQ(std::get<DecodeFault>(format4).code, DecodeFaultCode::InvalidOperandEncoding);
        }
    }
}
} // namespace
} // namespace assembler
