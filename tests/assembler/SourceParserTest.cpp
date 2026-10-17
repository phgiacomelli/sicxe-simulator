#include "assembler/SourceParser.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace {
using namespace assembler;

void expectInvalid(const SourceParseResult& result,
                   AssemblyDiagnosticCode code,
                   SourcePosition position) {
    ASSERT_EQ(result.lines.size(), 1U);
    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.lines.front().kind, SourceLineKind::Invalid);
    EXPECT_FALSE(result.lines.front().statement.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, code);
    EXPECT_EQ(result.diagnostics.front().position, position);
    EXPECT_FALSE(result.diagnostics.front().message.empty());
}

TEST(SourceParserTest, EmptySourceHasNoPhysicalLines) {
    const auto result = SourceParser::parse("");
    EXPECT_TRUE(result.succeeded());
    EXPECT_TRUE(result.lines.empty());
}

TEST(SourceParserTest, PreservesBlankLinesWithoutAddingLineAfterFinalNewline) {
    const auto result = SourceParser::parse(" \t\n\n");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_EQ(result.lines[0],
              (SourceLine{1, " \t", SourceLineKind::Blank, std::nullopt, std::nullopt}));
    EXPECT_EQ(result.lines[1],
              (SourceLine{2, "", SourceLineKind::Blank, std::nullopt, std::nullopt}));
}

TEST(SourceParserTest, AcceptsLfCrLfAndCrWithPhysicalLineNumbers) {
    const auto result = SourceParser::parse("  LDA #1\r\n  ADD #2\r  RSUB\n");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 3U);
    EXPECT_EQ(result.lines[0].text, "  LDA #1");
    EXPECT_EQ(result.lines[1].text, "  ADD #2");
    EXPECT_EQ(result.lines[2].text, "  RSUB");
    for (std::size_t index = 0; index < result.lines.size(); ++index) {
        const auto& line = result.lines[index];
        EXPECT_EQ(line.number, index + 1);
        ASSERT_TRUE(line.statement.has_value());
        EXPECT_EQ(line.statement->operation.source.position, (SourcePosition{index + 1, 3}));
    }
}

TEST(SourceParserTest, PreservesCommentOnlyLinesAndIgnoresTheirQuotes) {
    const auto result = SourceParser::parse("\t. comment ' ;\n  ; other '\n");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_EQ(result.lines[0].kind, SourceLineKind::Comment);
    EXPECT_FALSE(result.lines[0].statement.has_value());
    EXPECT_EQ(result.lines[0].comment, (SourceField{". comment ' ;", SourcePosition{1, 2}}));
    EXPECT_EQ(result.lines[1].kind, SourceLineKind::Comment);
    EXPECT_EQ(result.lines[1].comment, (SourceField{"; other '", SourcePosition{2, 3}}));
}

TEST(SourceParserTest, KeepsOriginalFieldsAndNormalizesOnlyIdentifiers) {
    const auto result = SourceParser::parse("loop\tlda\t#Value ; keep");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 1U);
    const auto& line = result.lines.front();
    EXPECT_EQ(line.text, "loop\tlda\t#Value ; keep");
    ASSERT_TRUE(line.statement.has_value());
    const auto& statement = *line.statement;
    EXPECT_EQ(statement.label,
              (SourceIdentifier{SourceField{"loop", SourcePosition{1, 1}}, "LOOP"}));
    EXPECT_EQ(statement.operation,
              (SourceIdentifier{SourceField{"lda", SourcePosition{1, 6}}, "LDA"}));
    EXPECT_EQ(statement.operand, (SourceField{"#Value", SourcePosition{1, 10}}));
    EXPECT_EQ(line.comment, (SourceField{"; keep", SourcePosition{1, 17}}));
}

TEST(SourceParserTest, IndentedOperationHasNoLabelAndKeepsExtendedPrefix) {
    const auto result = SourceParser::parse("  +jsub @Target");
    ASSERT_TRUE(result.succeeded());
    const auto& statement = result.lines.front().statement.value();
    EXPECT_FALSE(statement.label.has_value());
    EXPECT_EQ(statement.operation,
              (SourceIdentifier{SourceField{"+jsub", SourcePosition{1, 3}}, "+JSUB"}));
    EXPECT_EQ(statement.operand, (SourceField{"@Target", SourcePosition{1, 9}}));
}

TEST(SourceParserTest, AcceptsIdentifierDigitsAndUnderscoresAfterFirstLetter) {
    const auto result = SourceParser::parse("Loop_2 LDA #0");
    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.lines.front().statement->label->key, "LOOP_2");
}

TEST(SourceParserTest, TrimsOperandEdgesAndPreservesSpacesAroundCommas) {
    const auto result = SourceParser::parse("  ADDR \tA ,  X \t");
    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.lines.front().statement->operand,
              (SourceField{"A ,  X", SourcePosition{1, 9}}));
    for (const auto source : {"  ADDR A,X", "  ADDR A, X", "  LDA table ,X"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(SourceParser::parse(source).succeeded());
    }
}

TEST(SourceParserTest, PreservesSpacesSemicolonsCommasAndCaseInsideQuotes) {
    const auto result = SourceParser::parse("  BYTE C'Ab ;, Cd' ; after");
    ASSERT_TRUE(result.succeeded());
    const auto& line = result.lines.front();
    EXPECT_EQ(line.statement->operand, (SourceField{"C'Ab ;, Cd'", SourcePosition{1, 8}}));
    EXPECT_EQ(line.comment, (SourceField{"; after", SourcePosition{1, 20}}));
}

TEST(SourceParserTest, AcceptsEmptyQuotedDataAndLeavesItsMeaningToDataPackage) {
    for (const auto source : {"  BYTE C''", "  BYTE X''", "  BYTE X'0Z'"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(SourceParser::parse(source).succeeded());
    }
}

TEST(SourceParserTest, AcceptsNoOperandAndInlineCommentWithoutWhitespace) {
    const auto result = SourceParser::parse("  RSUB; return\n  LDA #1;value");
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_FALSE(result.lines[0].statement->operand.has_value());
    EXPECT_EQ(result.lines[0].comment, (SourceField{"; return", SourcePosition{1, 7}}));
    EXPECT_EQ(result.lines[1].statement->operand, (SourceField{"#1", SourcePosition{2, 7}}));
}

TEST(SourceParserTest, ReportsFirstInvalidLabelCharacter) {
    expectInvalid(SourceParser::parse("1LOOP LDA #1"),
                  AssemblyDiagnosticCode::InvalidLabel,
                  SourcePosition{1, 1});
    expectInvalid(SourceParser::parse("BAD-NAME LDA #1"),
                  AssemblyDiagnosticCode::InvalidLabel,
                  SourcePosition{1, 4});
    expectInvalid(SourceParser::parse("_LOOP LDA #1"),
                  AssemblyDiagnosticCode::InvalidLabel,
                  SourcePosition{1, 1});
    expectInvalid(SourceParser::parse("l\xC3\xA7 LDA #1"),
                  AssemblyDiagnosticCode::InvalidLabel,
                  SourcePosition{1, 2});
}

TEST(SourceParserTest, ReportsMissingOperationAtEndOfCode) {
    expectInvalid(SourceParser::parse("LABEL"),
                  AssemblyDiagnosticCode::MissingOperation,
                  SourcePosition{1, 6});
    const auto result = SourceParser::parse("LABEL \t; explanation");
    expectInvalid(result, AssemblyDiagnosticCode::MissingOperation, SourcePosition{1, 8});
    EXPECT_EQ(result.lines.front().comment, (SourceField{"; explanation", SourcePosition{1, 8}}));
}

TEST(SourceParserTest, DoesNotGuessUnindentedMnemonicAsOperation) {
    expectInvalid(SourceParser::parse("RSUB"),
                  AssemblyDiagnosticCode::MissingOperation,
                  SourcePosition{1, 5});
}

TEST(SourceParserTest, ReportsMalformedOperationAndBareExtendedPrefix) {
    expectInvalid(
        SourceParser::parse("  +"), AssemblyDiagnosticCode::InvalidOperation, SourcePosition{1, 3});
    expectInvalid(SourceParser::parse(" LDA! #1"),
                  AssemblyDiagnosticCode::InvalidOperation,
                  SourcePosition{1, 5});
    expectInvalid(SourceParser::parse("  123"),
                  AssemblyDiagnosticCode::InvalidOperation,
                  SourcePosition{1, 3});
}

TEST(SourceParserTest, ReportsUnclosedQuoteAtOpeningQuote) {
    for (const auto source : {"  BYTE C'ABC", "  BYTE C'A; not a comment"}) {
        SCOPED_TRACE(source);
        const auto result = SourceParser::parse(source);
        expectInvalid(result, AssemblyDiagnosticCode::UnterminatedQuote, SourcePosition{1, 9});
        EXPECT_FALSE(result.lines.front().comment.has_value());
    }
}

TEST(SourceParserTest, DoesNotCloseQuoteOnNextPhysicalLine) {
    const auto result = SourceParser::parse("  BYTE C'open\n  LDA #1");
    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().position, (SourcePosition{1, 9}));
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_EQ(result.lines[0].kind, SourceLineKind::Invalid);
    EXPECT_EQ(result.lines[1].kind, SourceLineKind::Statement);
}

TEST(SourceParserTest, ReportsExtraFieldOutsideQuotes) {
    expectInvalid(SourceParser::parse("  LDA #1 extra"),
                  AssemblyDiagnosticCode::UnexpectedField,
                  SourcePosition{1, 10});
    expectInvalid(SourceParser::parse("  BYTE C'A' X'01'"),
                  AssemblyDiagnosticCode::UnexpectedField,
                  SourcePosition{1, 13});
}

TEST(SourceParserTest, IgnoresUnmatchedQuoteInsideInlineComment) {
    const auto result = SourceParser::parse("  LDA #1 ; comment ' \n  RSUB");
    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.lines.front().comment, (SourceField{"; comment ' ", SourcePosition{1, 10}}));
}

TEST(SourceParserTest, ContinuesAfterInvalidLinesWithoutExposingPartialStatements) {
    const auto result = SourceParser::parse("BAD- LDA #1\nLABEL\n  BYTE C'open\n  LDA #5\n");
    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 4U);
    ASSERT_EQ(result.diagnostics.size(), 3U);
    EXPECT_EQ(result.diagnostics[0].code, AssemblyDiagnosticCode::InvalidLabel);
    EXPECT_EQ(result.diagnostics[0].position, (SourcePosition{1, 4}));
    EXPECT_EQ(result.diagnostics[1].code, AssemblyDiagnosticCode::MissingOperation);
    EXPECT_EQ(result.diagnostics[1].position, (SourcePosition{2, 6}));
    EXPECT_EQ(result.diagnostics[2].code, AssemblyDiagnosticCode::UnterminatedQuote);
    EXPECT_EQ(result.diagnostics[2].position, (SourcePosition{3, 9}));
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_EQ(result.lines[index].kind, SourceLineKind::Invalid);
        EXPECT_FALSE(result.lines[index].statement.has_value());
    }
    ASSERT_TRUE(result.lines.back().statement.has_value());
    EXPECT_EQ(result.lines.back().statement->operation.key, "LDA");
}

TEST(SourceParserTest, LeavesInstructionAndOperandSemanticsToLaterPackages) {
    for (const auto source : {"  ADDF #bad",
                              "  UNKNOWN undefined",
                              "  SHIFTL F,99",
                              "  LDA @table,X",
                              "  WORD A+1",
                              "  RSUB operand"}) {
        SCOPED_TRACE(source);
        const auto result = SourceParser::parse(source);
        EXPECT_TRUE(result.succeeded());
        ASSERT_EQ(result.lines.size(), 1U);
        EXPECT_EQ(result.lines.front().kind, SourceLineKind::Statement);
    }
}

TEST(SourceParserTest, ResultsOwnTheirTextAndInvocationsAreIndependent) {
    std::string source = "loop lda #Value";
    const auto first = SourceParser::parse(source);
    source.assign("changed");
    const auto invalid = SourceParser::parse("BAD-");
    const auto second = SourceParser::parse("  RSUB");
    ASSERT_TRUE(first.succeeded());
    EXPECT_EQ(first.lines.front().text, "loop lda #Value");
    EXPECT_EQ(first.lines.front().statement->label->key, "LOOP");
    EXPECT_EQ(first.lines.front().statement->operand->text, "#Value");
    EXPECT_FALSE(invalid.succeeded());
    EXPECT_TRUE(second.succeeded());
    EXPECT_EQ(second.lines.front().number, 1U);
}

} // namespace
