#include "assembler/OperandValue.hpp"

#include "assembler/SourceParser.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <string_view>

namespace {
using namespace assembler;

void expectParseDiagnostic(std::string_view token,
                           SourcePosition position,
                           OperandDiagnosticCode expectedCode) {
    const auto result = parseValue(token, position);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(result));
    const auto& diagnostic = std::get<OperandDiagnostic>(result);
    EXPECT_EQ(diagnostic.code, expectedCode);
    EXPECT_EQ(diagnostic.token, token);
    EXPECT_EQ(diagnostic.position, position);
    EXPECT_FALSE(diagnostic.message.empty());
}

TEST(OperandValueTest, ParsesDecimalZeroPositiveNegativeAndLimits) {
    const SourcePosition pos{3, 10};
    {
        const auto zero = parseValue("0", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(zero));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(zero)),
                  (IntegerOperand{0, IntegerBase::Decimal, false, "0", pos}));
    }
    {
        const auto minusZero = parseValue("-0", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(minusZero));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(minusZero)),
                  (IntegerOperand{0, IntegerBase::Decimal, true, "-0", pos}));
    }
    {
        const auto plusValue = parseValue("+4095", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(plusValue));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(plusValue)),
                  (IntegerOperand{4095, IntegerBase::Decimal, false, "+4095", pos}));
    }
    {
        const auto minusValue = parseValue("-8388608", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(minusValue));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(minusValue)),
                  (IntegerOperand{-8388608, IntegerBase::Decimal, true, "-8388608", pos}));
    }
    {
        const auto maxInt = parseValue("9223372036854775807", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(maxInt));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(maxInt)).value,
                  std::numeric_limits<std::int64_t>::max());
    }
    {
        const auto minInt = parseValue("-9223372036854775808", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(minInt));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(minInt)).value,
                  std::numeric_limits<std::int64_t>::min());
    }
}

TEST(OperandValueTest, ParsesHexadecimalWithPrefixInBothCasesAndSigns) {
    const SourcePosition pos{4, 12};
    {
        const auto hexZero = parseValue("0x0", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(hexZero));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(hexZero)),
                  (IntegerOperand{0, IntegerBase::Hexadecimal, false, "0x0", pos}));
    }
    {
        const auto hexUpper = parseValue("0XFFFFF", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(hexUpper));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(hexUpper)),
                  (IntegerOperand{0xFFFFF, IntegerBase::Hexadecimal, false, "0XFFFFF", pos}));
    }
    {
        const auto hexMixed = parseValue("+0x1aBc", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(hexMixed));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(hexMixed)),
                  (IntegerOperand{0x1ABC, IntegerBase::Hexadecimal, false, "+0x1aBc", pos}));
    }
    {
        const auto hexNegative = parseValue("-0x800000", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(hexNegative));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(hexNegative)),
                  (IntegerOperand{-0x800000, IntegerBase::Hexadecimal, true, "-0x800000", pos}));
    }
    {
        const auto hexMax = parseValue("0x7FFFFFFFFFFFFFFF", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(hexMax));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(hexMax)).value,
                  std::numeric_limits<std::int64_t>::max());
    }
    {
        const auto hexMin = parseValue("-0x8000000000000000", pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(hexMin));
        EXPECT_EQ(std::get<IntegerOperand>(std::get<OperandValue>(hexMin)).value,
                  std::numeric_limits<std::int64_t>::min());
    }
}

TEST(OperandValueTest, ReportsIntegerOverflowAndInvalidIntegers) {
    const SourcePosition pos{5, 8};
    for (const std::string_view overflowing : {"9223372036854775808",
                                               "-9223372036854775809",
                                               "0x8000000000000000",
                                               "-0x8000000000000001",
                                               "99999999999999999999999999"}) {
        SCOPED_TRACE(overflowing);
        expectParseDiagnostic(overflowing, pos, OperandDiagnosticCode::IntegerOverflow);
    }

    for (const std::string_view malformed :
         {"+", "-", "0x", "-0x", "+0X", "0x1G", "12abc", "0FFh", "0b10", "0x10_0"}) {
        SCOPED_TRACE(malformed);
        expectParseDiagnostic(malformed, pos, OperandDiagnosticCode::InvalidInteger);
    }

    expectParseDiagnostic("", pos, OperandDiagnosticCode::EmptyOperand);
}

TEST(OperandValueTest, RejectsArithmeticExpressionsCompoundTokensAndParentheses) {
    const SourcePosition pos{6, 11};
    for (const std::string_view expression : {"A+1",
                                              "A-B",
                                              "2*B",
                                              "1+2",
                                              "10-3",
                                              "8/2",
                                              "(A)",
                                              "(10)",
                                              "L(BUFFER)",
                                              "A(TARGET)",
                                              "+A",
                                              "-B",
                                              "0x10+4"}) {
        SCOPED_TRACE(expression);
        expectParseDiagnostic(expression, pos, OperandDiagnosticCode::UnsupportedExpression);
    }
}

TEST(OperandValueTest, ParsesValidSymbolsAndRejectsInvalidOrReservedNames) {
    const SourcePosition pos{7, 9};
    const auto symbolResult = parseValue("Table_1", pos);
    ASSERT_TRUE(std::holds_alternative<OperandValue>(symbolResult));
    const auto& symbol = std::get<SymbolOperand>(std::get<OperandValue>(symbolResult));
    EXPECT_EQ(symbol, (SymbolOperand{"TABLE_1", "Table_1", pos}));
    EXPECT_EQ(symbol.identifier(), (SourceIdentifier{SourceField{"Table_1", pos}, "TABLE_1"}));

    for (const std::string_view singleLetter : {"A", "B", "X"}) {
        SCOPED_TRACE(singleLetter);
        const auto parsed = parseValue(singleLetter, pos);
        ASSERT_TRUE(std::holds_alternative<OperandValue>(parsed));
        EXPECT_TRUE(std::holds_alternative<SymbolOperand>(std::get<OperandValue>(parsed)));
    }

    for (const std::string_view invalidSymbol : {"_LABEL", "=5", "#5", "@A", "A,X", "BAD$NAME"}) {
        SCOPED_TRACE(invalidSymbol);
        expectParseDiagnostic(invalidSymbol, pos, OperandDiagnosticCode::InvalidSymbolName);
    }

    for (const std::string_view reserved :
         {"LDA", "lda", "RSUB", "SHIFTR", "START", "end", "WORD", "BYTE", "RESB", "RESW", "BASE"}) {
        SCOPED_TRACE(reserved);
        expectParseDiagnostic(reserved, pos, OperandDiagnosticCode::ReservedSymbolName);
    }
}

TEST(OperandValueTest, EnforcesDirectiveSpecificSignAndRangeConstraints) {
    const SourcePosition pos{8, 10};

    EXPECT_EQ(std::get<std::int64_t>(parseKnownInteger("0", pos, Address20BitRange)), 0);
    EXPECT_EQ(std::get<std::int64_t>(parseKnownInteger("0xFFFFF", pos, Address20BitRange)),
              0xFFFFF);

    const auto negativeStart = parseKnownInteger("-1", pos, Address20BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(negativeStart));
    EXPECT_EQ(std::get<OperandDiagnostic>(negativeStart).code,
              OperandDiagnosticCode::NegativeValueNotAllowed);

    const auto negativeZeroStart = parseKnownInteger("-0", pos, Address20BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(negativeZeroStart));
    EXPECT_EQ(std::get<OperandDiagnostic>(negativeZeroStart).code,
              OperandDiagnosticCode::NegativeValueNotAllowed);

    const auto outOf20Bit = parseKnownInteger("0x100000", pos, Address20BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(outOf20Bit));
    EXPECT_EQ(std::get<OperandDiagnostic>(outOf20Bit).code, OperandDiagnosticCode::ValueOutOfRange);

    const auto symbolInStart = parseKnownInteger("START_ADDR", pos, Address20BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(symbolInStart));
    EXPECT_EQ(std::get<OperandDiagnostic>(symbolInStart).code,
              OperandDiagnosticCode::SymbolNotAllowed);

    EXPECT_EQ(std::get<std::int64_t>(parseKnownInteger("-0x800000", pos, Word24BitRange)),
              -0x800000);
    EXPECT_EQ(std::get<std::int64_t>(parseKnownInteger("0xFFFFFF", pos, Word24BitRange)), 0xFFFFFF);

    const auto belowWord = parseKnownInteger("-0x800001", pos, Word24BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(belowWord));
    EXPECT_EQ(std::get<OperandDiagnostic>(belowWord).code, OperandDiagnosticCode::ValueOutOfRange);

    const auto aboveWord = parseKnownInteger("0x1000000", pos, Word24BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(aboveWord));
    EXPECT_EQ(std::get<OperandDiagnostic>(aboveWord).code, OperandDiagnosticCode::ValueOutOfRange);
}

TEST(OperandValueTest, SupportsTwoPassReferenceAndResolutionWorkflowWithSourceParser) {
    const auto parsed = SourceParser::parse("  WORD Target\n"
                                            "  WORD -16\n"
                                            "  WORD Missing\n"
                                            "target RESW 1\n");
    ASSERT_TRUE(parsed.succeeded());
    ASSERT_EQ(parsed.lines.size(), 4U);

    const auto firstOperand =
        std::get<OperandValue>(parseValue(*parsed.lines[0].statement->operand));
    const auto secondOperand =
        std::get<OperandValue>(parseValue(*parsed.lines[1].statement->operand));
    const auto thirdOperand =
        std::get<OperandValue>(parseValue(*parsed.lines[2].statement->operand));

    SymbolTable symbols;
    EXPECT_FALSE(referenceOperand(firstOperand, symbols, Word24BitRange).has_value());
    EXPECT_FALSE(referenceOperand(secondOperand, symbols, Word24BitRange).has_value());
    EXPECT_FALSE(referenceOperand(thirdOperand, symbols, Word24BitRange).has_value());

    EXPECT_FALSE(symbols.define(*parsed.lines[3].statement->label, 0x1009).has_value());

    const auto resolvedFirst = resolveOperand(firstOperand, symbols, Word24BitRange);
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(resolvedFirst));
    EXPECT_EQ(std::get<std::int64_t>(resolvedFirst), 0x1009);

    const auto resolvedSecond = resolveOperand(secondOperand, symbols, Word24BitRange);
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(resolvedSecond));
    EXPECT_EQ(std::get<std::int64_t>(resolvedSecond), -16);

    const auto resolvedThird = resolveOperand(thirdOperand, symbols, Word24BitRange);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(resolvedThird));
    EXPECT_EQ(std::get<OperandDiagnostic>(resolvedThird).code,
              OperandDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(std::get<OperandDiagnostic>(resolvedThird).position, (SourcePosition{3, 8}));
}

TEST(OperandValueTest, ConvertsErrorsToFinalAssemblyDiagnosticsWithoutSuccessPayload) {
    const SourcePosition position{5, 8};
    const auto overflow = parseValue("9223372036854775808", position);
    ASSERT_TRUE(std::holds_alternative<OperandDiagnostic>(overflow));
    const auto assemblyDiagnostic = toAssemblyDiagnostic(std::get<OperandDiagnostic>(overflow));
    EXPECT_EQ(assemblyDiagnostic.code, AssemblyDiagnosticCode::IntegerOverflow);
    EXPECT_EQ(assemblyDiagnostic.position, position);
    EXPECT_FALSE(assemblyDiagnostic.message.empty());

    const AssemblyResult result = AssemblyFailure{{assemblyDiagnostic}};
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(result));
    EXPECT_EQ(std::get<AssemblyFailure>(result).diagnostics.front(), assemblyDiagnostic);
}

} // namespace
