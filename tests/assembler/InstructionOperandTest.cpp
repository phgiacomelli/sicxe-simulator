#include "assembler/InstructionOperand.hpp"

#include "InstructionTestSupport.hpp"
#include "assembler/OperandValue.hpp"

#include <gtest/gtest.h>

namespace assembler {
namespace {
using Code = InstructionDiagnosticCode;

TEST(InstructionOperandTest, ParsesEveryInstructionFamilyAndSelectedSize) {
    for (const auto& expected : test::ExpectedIsa) {
        SCOPED_TRACE(expected.name);
        std::string suffix;
        switch (expected.operands) {
            case OperandPattern::None:
                break;
            case OperandPattern::OneRegister:
                suffix = " sW";
                break;
            case OperandPattern::TwoRegisters:
                suffix = " pC , x";
                break;
            case OperandPattern::RegisterAndCount:
                suffix = " t , 0x10";
                break;
            case OperandPattern::Memory:
                suffix = " TaRgEt , x";
                break;
        }
        const auto parsed = test::operand(std::string{expected.name} + suffix);
        EXPECT_EQ(parsed.size(), expected.formats == InstructionFormats::Format2 ? 2 : 3);
        if (expected.operands == OperandPattern::None) {
            EXPECT_TRUE(std::holds_alternative<NoInstructionOperand>(parsed.data));
        } else if (expected.operands == OperandPattern::OneRegister) {
            EXPECT_EQ(std::get<OneRegisterOperand>(parsed.data).first.reg, InstructionRegister::SW);
        } else if (expected.operands == OperandPattern::TwoRegisters) {
            const auto& pair = std::get<TwoRegisterOperand>(parsed.data);
            EXPECT_EQ(pair.first.reg, InstructionRegister::PC);
            EXPECT_EQ(pair.second.reg, InstructionRegister::X);
        } else if (expected.operands == OperandPattern::RegisterAndCount) {
            EXPECT_EQ(std::get<RegisterCountOperand>(parsed.data).count, 16);
        } else {
            const auto& memory = std::get<MemoryInstructionOperand>(parsed.data);
            EXPECT_EQ(memory.value.text, "TaRgEt");
            EXPECT_TRUE(memory.indexed);
            EXPECT_EQ(test::operand("+" + std::string{expected.name} + " #0xFFF").size(), 4);
        }
    }
}

TEST(InstructionOperandTest, MemoryModesKeepOwnedValueTokensAndBytePositionsForPackage3) {
    auto source = test::line("+LdA TaRgEt , x ; comment");
    const auto original = source;
    const auto result = parseInstructionOperand(test::spec("LDA"), source);
    ASSERT_TRUE(std::holds_alternative<InstructionOperand>(result));
    EXPECT_EQ(source, original);
    source = {};
    const auto& parsed = std::get<InstructionOperand>(result);
    EXPECT_EQ(parsed.form, InstructionForm::Format4);
    EXPECT_EQ(parsed.operation, (SourceField{"+LdA", {2, 2}}));
    const auto& memory = std::get<MemoryInstructionOperand>(parsed.data);
    EXPECT_EQ(memory.mode, InstructionAddressing::Simple);
    EXPECT_EQ(memory.kind, InstructionValueKind::Symbol);
    EXPECT_EQ(memory.value, (SourceField{"TaRgEt", {2, 7}}));
    const auto symbol = parseValue(memory.value);
    EXPECT_EQ(std::get<SymbolOperand>(std::get<OperandValue>(symbol)).name, "TARGET");

    for (const auto* text : {"#0x00f", "@Label", "42", "+12", "-12", "0xFFFFFFFFFFFFFFFFFFFF"}) {
        const auto item = test::operand(std::string{"LDA "} + text);
        const auto& value = std::get<MemoryInstructionOperand>(item.data);
        const bool prefix = text[0] == '#' || text[0] == '@';
        EXPECT_EQ(value.value.text, prefix ? text + 1 : text);
        EXPECT_EQ(value.value.position, (SourcePosition{2, prefix ? 7u : 6u}));
        EXPECT_EQ(value.mode,
                  text[0] == '#' ? InstructionAddressing::Immediate
                                 : (text[0] == '@' ? InstructionAddressing::Indirect
                                                   : InstructionAddressing::Simple));
        EXPECT_FALSE(value.indexed);
    }
}

TEST(InstructionOperandTest, RegisterAndCountTokensPreserveWhitespaceOffsetsAndSpelling) {
    const auto parsed = test::operand("sHiFtL pC , 0X10");
    const auto& shift = std::get<RegisterCountOperand>(parsed.data);
    EXPECT_EQ(shift.first.reg, InstructionRegister::PC);
    EXPECT_EQ(shift.first.source, (SourceField{"pC", {2, 9}}));
    EXPECT_EQ(shift.count, 16);
    EXPECT_EQ(shift.countSource, (SourceField{"0X10", {2, 14}}));
    EXPECT_EQ(test::operand("SHIFTR A,1").size(), 2);
}

TEST(InstructionOperandTest, RejectsInvalidRegisterAndArityAcrossAllFormat2Opcodes) {
    for (const auto& expected : test::ExpectedIsa) {
        if (expected.formats != InstructionFormats::Format2) {
            continue;
        }
        SCOPED_TRACE(expected.name);
        const auto name = std::string{expected.name};
        const auto suffix = expected.operands == OperandPattern::OneRegister
                                ? ""
                                : (expected.operands == OperandPattern::TwoRegisters ? ",X" : ",1");
        const auto invalid = test::parse(name + " F" + suffix);
        ASSERT_TRUE(std::holds_alternative<InstructionDiagnostic>(invalid));
        EXPECT_EQ(std::get<InstructionDiagnostic>(invalid).code, Code::InvalidRegister);
        EXPECT_EQ(std::get<InstructionDiagnostic>(invalid).source.position,
                  (SourcePosition{2, expected.name.size() + 3}));
        const auto wrongArity =
            test::parse(name + (expected.operands == OperandPattern::OneRegister ? " A,X" : " A"));
        EXPECT_EQ(std::get<InstructionDiagnostic>(wrongArity).code, Code::WrongOperandCount);
    }
}

TEST(InstructionOperandTest, FailingCallsDoNotAffectLaterParsingOrInput) {
    const auto input = test::line("CLEAR F");
    const auto copy = input;
    for (int i = 0; i < 3; ++i) {
        const auto failure = parseInstructionOperand(test::spec("CLEAR"), input);
        EXPECT_EQ(std::get<InstructionDiagnostic>(failure).code, Code::InvalidRegister);
        EXPECT_EQ(input, copy);
        const auto success = test::operand("CLEAR A");
        EXPECT_EQ(std::get<OneRegisterOperand>(success.data).first.reg, InstructionRegister::A);
    }
}

struct InvalidOperandCase {
    const char* source;
    Code code;
    std::size_t column;
};
class InvalidInstructionOperandTest : public testing::TestWithParam<InvalidOperandCase> {};
TEST_P(InvalidInstructionOperandTest, ReportsTypedFailureAndExactBytePosition) {
    const auto& input = GetParam();
    const auto result = test::parse(input.source);
    ASSERT_TRUE(std::holds_alternative<InstructionDiagnostic>(result)) << input.source;
    const auto& failure = std::get<InstructionDiagnostic>(result);
    EXPECT_EQ(failure.code, input.code) << input.source;
    EXPECT_EQ(failure.source.position, (SourcePosition{2, input.column})) << input.source;
    const auto shared = toAssemblyDiagnostic(failure);
    EXPECT_EQ(shared.position, failure.source.position);
    EXPECT_EQ(shared.message, failure.message);
}
INSTANTIATE_TEST_SUITE_P(
    OperandRules,
    InvalidInstructionOperandTest,
    testing::Values(InvalidOperandCase{"CLEAR", Code::MissingOperand, 7},
                    InvalidOperandCase{"ADDR", Code::MissingOperand, 6},
                    InvalidOperandCase{"SHIFTL", Code::MissingOperand, 8},
                    InvalidOperandCase{"LDA", Code::MissingOperand, 5},
                    InvalidOperandCase{"RSUB A", Code::UnexpectedOperand, 7},
                    InvalidOperandCase{"CLEAR F", Code::InvalidRegister, 8},
                    InvalidOperandCase{"TIXR f", Code::InvalidRegister, 7},
                    InvalidOperandCase{"ADDR F,A", Code::InvalidRegister, 7},
                    InvalidOperandCase{"SUBR A,F", Code::InvalidRegister, 9},
                    InvalidOperandCase{"SHIFTL F,1", Code::InvalidRegister, 9},
                    InvalidOperandCase{"CLEAR 0", Code::InvalidRegister, 8},
                    InvalidOperandCase{"CLEAR Z", Code::InvalidRegister, 8},
                    InvalidOperandCase{"CLEAR A,X", Code::WrongOperandCount, 9},
                    InvalidOperandCase{"TIXR A,X", Code::WrongOperandCount, 8},
                    InvalidOperandCase{"COMPR A", Code::WrongOperandCount, 8},
                    InvalidOperandCase{"SHIFTR A", Code::WrongOperandCount, 9},
                    InvalidOperandCase{"ADDR ,X", Code::WrongOperandCount, 7},
                    InvalidOperandCase{"ADDR A,", Code::WrongOperandCount, 9},
                    InvalidOperandCase{"RMO A,X,L", Code::WrongOperandCount, 8},
                    InvalidOperandCase{"SHIFTL A,0", Code::InvalidShiftCount, 11},
                    InvalidOperandCase{"SHIFTR A,17", Code::InvalidShiftCount, 11},
                    InvalidOperandCase{"SHIFTL A,-1", Code::InvalidShiftCount, 11},
                    InvalidOperandCase{"SHIFTL A,Label", Code::InvalidShiftCount, 11},
                    InvalidOperandCase{"SHIFTL A,1+1", Code::InvalidShiftCount, 11},
                    InvalidOperandCase{
                        "SHIFTL A,9999999999999999999999", Code::InvalidShiftCount, 11},
                    InvalidOperandCase{"LDA #Value ,X", Code::InvalidIndexing, 13},
                    InvalidOperandCase{"LDA @Value,X", Code::InvalidIndexing, 12},
                    InvalidOperandCase{"LDA Value,Y", Code::InvalidIndexing, 11},
                    InvalidOperandCase{"LDA Value,X,X", Code::InvalidIndexing, 11},
                    InvalidOperandCase{"LDA Value,", Code::InvalidIndexing, 11},
                    InvalidOperandCase{"LDA ,X", Code::MissingValue, 6},
                    InvalidOperandCase{"LDA #", Code::MissingValue, 7},
                    InvalidOperandCase{"LDA @", Code::MissingValue, 7},
                    InvalidOperandCase{"LDA =Value", Code::UnsupportedOperandSyntax, 6},
                    InvalidOperandCase{"LDA Value+1", Code::UnsupportedOperandSyntax, 6},
                    InvalidOperandCase{"LDA Value-1", Code::UnsupportedOperandSyntax, 6},
                    InvalidOperandCase{"LDA 2*Value", Code::UnsupportedOperandSyntax, 6},
                    InvalidOperandCase{"LDA (Value)", Code::UnsupportedOperandSyntax, 6},
                    InvalidOperandCase{"LDA +Value", Code::UnsupportedOperandSyntax, 6},
                    InvalidOperandCase{"LDA #@Value", Code::InvalidValue, 7},
                    InvalidOperandCase{"LDA #0x", Code::InvalidValue, 7},
                    InvalidOperandCase{"LDA 0xGG", Code::InvalidValue, 6},
                    InvalidOperandCase{"LDA #-1", Code::NegativeImmediate, 7},
                    InvalidOperandCase{"LDA #-0", Code::NegativeImmediate, 7},
                    InvalidOperandCase{"+RSUB", Code::InvalidExtendedFormat, 2},
                    InvalidOperandCase{"+CLEAR A", Code::InvalidExtendedFormat, 2}));

TEST(InstructionOperandTest, RejectsNonStatementsAndMismatchedSpecsWithoutChangingSharedTypes) {
    const SourceLine blank{3, "", SourceLineKind::Blank, {}, {}};
    const auto result = parseInstructionOperand(test::spec("LDA"), blank);
    EXPECT_EQ(std::get<InstructionDiagnostic>(result).code, Code::InvalidSourceLine);
    EXPECT_EQ(std::get<InstructionDiagnostic>(result).source.position, (SourcePosition{3, 1}));
    const auto mismatch = parseInstructionOperand(test::spec("ADDR"), test::line("CLEAR A"));
    EXPECT_EQ(std::get<InstructionDiagnostic>(mismatch).code, Code::InstructionMismatch);
    EXPECT_EQ(std::get<InstructionDiagnostic>(mismatch).source.position, (SourcePosition{2, 2}));
    const auto extended = parseInstructionOperand(test::spec("RSUB"), test::line("+RSUB"));
    EXPECT_EQ(std::get<InstructionDiagnostic>(extended).code, Code::InvalidExtendedFormat);
}
} // namespace
} // namespace assembler
