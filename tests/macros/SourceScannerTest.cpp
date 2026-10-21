#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>
#include <string>

namespace macro {
namespace {

TEST(SourceScannerTest, PreservesFieldsWithoutValidatingAssemblyIdentifiers) {
    const auto result = SourceScanner::scan("LOOP.SER\tlda\t#Value extra");
    ASSERT_EQ(result.lines.size(), 1U);
    const auto& line = result.lines.front();
    EXPECT_EQ(line.number, 1U);
    EXPECT_EQ(line.text, "LOOP.SER\tlda\t#Value extra");
    EXPECT_EQ(line.kind, SourceLineKind::Code);
    EXPECT_EQ(line.label, (SourceField{"LOOP.SER", {1, 1}}));
    EXPECT_EQ(line.operation, (SourceField{"lda", {1, 10}}));
    EXPECT_EQ(line.operand, (SourceField{"#Value extra", {1, 14}}));
    EXPECT_FALSE(line.comment);
    EXPECT_FALSE(line.openQuote);
    EXPECT_FALSE(line.terminated);
}

TEST(SourceScannerTest, SplitsPhysicalLinesWithoutInventingAFinalLine) {
    EXPECT_TRUE(SourceScanner::scan("").lines.empty());
    const auto result = SourceScanner::scan("\t LDA #1\r\nLABEL\r \t\n\n END");
    ASSERT_EQ(result.lines.size(), 5U);
    EXPECT_EQ(result.lines[0].operation, (SourceField{"LDA", {1, 3}}));
    EXPECT_EQ(result.lines[1].label, (SourceField{"LABEL", {2, 1}}));
    EXPECT_FALSE(result.lines[1].operation);
    EXPECT_EQ(result.lines[2].text, " \t");
    EXPECT_EQ(result.lines[2].kind, SourceLineKind::Blank);
    EXPECT_EQ(result.lines[3].kind, SourceLineKind::Blank);
    EXPECT_FALSE(result.lines[3].label);
    EXPECT_EQ(result.lines[4].operation, (SourceField{"END", {5, 2}}));
    for (std::size_t index = 0; index < result.lines.size(); ++index) {
        EXPECT_EQ(result.lines[index].number, index + 1);
        EXPECT_EQ(result.lines[index].terminated, index < 4);
    }
    const auto terminated = SourceScanner::scan(" LDA #1\n\n");
    ASSERT_EQ(terminated.lines.size(), 2U);
    EXPECT_TRUE(terminated.lines[1].terminated);
    EXPECT_EQ(terminated.lines[1].kind, SourceLineKind::Blank);
}

TEST(SourceScannerTest, SeparatesCommentsOnlyOutsideQuotedText) {
    const auto result = SourceScanner::scan("\t. MCEND ; ' comment\n"
                                            " ; MCDEFN '\n"
                                            "DEST INCR C'A,B; C',#7 ; prototype '\n"
                                            " LDA #1;inline\n"
                                            " BYTE C''; empty\n");
    ASSERT_EQ(result.lines.size(), 5U);
    EXPECT_EQ(result.lines[0].kind, SourceLineKind::Comment);
    EXPECT_EQ(result.lines[0].comment, (SourceField{". MCEND ; ' comment", {1, 2}}));
    EXPECT_FALSE(result.lines[0].operation);
    EXPECT_FALSE(result.lines[0].openQuote);
    EXPECT_EQ(result.lines[1].kind, SourceLineKind::Comment);
    EXPECT_EQ(result.lines[1].comment, (SourceField{"; MCDEFN '", {2, 2}}));
    EXPECT_FALSE(result.lines[1].openQuote);
    EXPECT_EQ(result.lines[2].operand, (SourceField{"C'A,B; C',#7", {3, 11}}));
    EXPECT_EQ(result.lines[2].comment, (SourceField{"; prototype '", {3, 24}}));
    EXPECT_FALSE(result.lines[2].openQuote);
    EXPECT_EQ(result.lines[3].operand, (SourceField{"#1", {4, 6}}));
    EXPECT_EQ(result.lines[3].comment, (SourceField{";inline", {4, 8}}));
    EXPECT_EQ(result.lines[4].operand, (SourceField{"C''", {5, 7}}));
    EXPECT_EQ(result.lines[4].comment, (SourceField{"; empty", {5, 10}}));
}

TEST(SourceScannerTest, RecognizesDirectivesOnlyAsExactOperationTokens) {
    const auto result = SourceScanner::scan(" mCdEfN ; begin\n"
                                            " mcEND\n"
                                            "MCDEFN WORD 1\n"
                                            " LDA MCEND\n"
                                            " BYTE C'MCDEFN;MCEND'\n"
                                            " . MCEND\n"
                                            " +MCDEFN\n"
                                            " MCEND_EXTRA\n"
                                            "MCEND\n"
                                            "LABEL MCEND unexpected\n");
    ASSERT_EQ(result.lines.size(), 10U);
    EXPECT_EQ(result.lines[0].directive(), MacroDirective::DefinitionBegin);
    EXPECT_EQ(result.lines[1].directive(), MacroDirective::DefinitionEnd);
    for (std::size_t index = 2; index < 9; ++index)
        EXPECT_EQ(result.lines[index].directive(), MacroDirective::None) << index;
    // Recognition is lexical; package 2 will reject the extra label/operand.
    EXPECT_EQ(result.lines[9].directive(), MacroDirective::DefinitionEnd);
    EXPECT_EQ(result.lines[0].operation, (SourceField{"mCdEfN", {1, 2}}));
}

TEST(SourceScannerTest, NormalizesOnlyLineEndingsWithoutExpandingOrDroppingComments) {
    const auto result =
        SourceScanner::scan(" . keep\r\n MCDEFN ; begin\rDEST INCR A , B\n MCEND\r\n tail  ");
    EXPECT_EQ(result.normalizedText(),
              " . keep\n MCDEFN ; begin\nDEST INCR A , B\n MCEND\n tail  ");
    EXPECT_EQ(SourceScanner::scan("").normalizedText(), "");
    EXPECT_EQ(SourceScanner::scan("\r\n\r\n").normalizedText(), "\n\n");
    EXPECT_EQ(SourceScanner::scan("  \r").normalizedText(), "  \n");
}

TEST(SourceScannerTest, KeepsUnclosedQuotesAsLexicalInformationWithoutRejectingTheLine) {
    const auto result = SourceScanner::scan(" BYTE C'open; not a comment\n RSUB ; '\n");
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_EQ(result.lines[0].kind, SourceLineKind::Code);
    EXPECT_EQ(result.lines[0].openQuote, (SourcePosition{1, 8}));
    EXPECT_EQ(result.lines[0].operand, (SourceField{"C'open; not a comment", {1, 7}}));
    EXPECT_FALSE(result.lines[0].comment);
    EXPECT_FALSE(result.lines[1].openQuote);
    EXPECT_EQ(result.lines[1].operation, (SourceField{"RSUB", {2, 2}}));
    EXPECT_EQ(result.lines[1].comment, (SourceField{"; '", {2, 7}}));
}

TEST(SourceScannerTest, PreservesIncompleteAndInvalidAssemblyForTheResponsibleConsumer) {
    const auto result = SourceScanner::scan("LABEL ; missing operation\n"
                                            " BAD! #1 extra\n"
                                            "1BAD LDA #VALUE\n"
                                            " SHIFTR RP\n"
                                            " CALL A,,C'D,E',NAME=VALUE\n");
    ASSERT_EQ(result.lines.size(), 5U);
    EXPECT_EQ(result.lines[0].kind, SourceLineKind::Code);
    EXPECT_EQ(result.lines[0].label, (SourceField{"LABEL", {1, 1}}));
    EXPECT_FALSE(result.lines[0].operation);
    EXPECT_EQ(result.lines[0].comment, (SourceField{"; missing operation", {1, 7}}));
    EXPECT_EQ(result.lines[1].operation, (SourceField{"BAD!", {2, 2}}));
    EXPECT_EQ(result.lines[1].operand, (SourceField{"#1 extra", {2, 7}}));
    EXPECT_EQ(result.lines[2].label, (SourceField{"1BAD", {3, 1}}));
    EXPECT_EQ(result.lines[3].operand, (SourceField{"RP", {4, 9}}));
    // No argument splitting or keyword validation: those belong to package 3.
    EXPECT_EQ(result.lines[4].operand, (SourceField{"A,,C'D,E',NAME=VALUE", {5, 7}}));
}

TEST(SourceScannerTest, PreservesAddressingMarkersAndWhitespaceWithoutNormalizingFields) {
    const auto result = SourceScanner::scan("  +jsub @PTR ,X  \t\n ADDR A ,  X\n");
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_EQ(result.lines[0].text, "  +jsub @PTR ,X  \t");
    EXPECT_FALSE(result.lines[0].label);
    EXPECT_EQ(result.lines[0].operation, (SourceField{"+jsub", {1, 3}}));
    EXPECT_EQ(result.lines[0].operand, (SourceField{"@PTR ,X", {1, 9}}));
    EXPECT_EQ(result.lines[1].operand, (SourceField{"A ,  X", {2, 7}}));
}

TEST(SourceScannerTest, CountsByteColumnsRatherThanVisualOrUnicodeColumns) {
    const auto result = SourceScanner::scan("r\xC3\xB3t\tLDA\t#1");
    ASSERT_EQ(result.lines.size(), 1U);
    EXPECT_EQ(result.lines[0].label, (SourceField{"r\xC3\xB3t", {1, 1}}));
    EXPECT_EQ(result.lines[0].operation, (SourceField{"LDA", {1, 6}}));
    EXPECT_EQ(result.lines[0].operand, (SourceField{"#1", {1, 10}}));
}

TEST(SourceScannerTest, DoesNotGuessAnUnindentedOperation) {
    const auto result = SourceScanner::scan("MCDEFN\n RSUB;done\n");
    ASSERT_EQ(result.lines.size(), 2U);
    EXPECT_EQ(result.lines[0].label, (SourceField{"MCDEFN", {1, 1}}));
    EXPECT_FALSE(result.lines[0].operation);
    EXPECT_EQ(result.lines[0].directive(), MacroDirective::None);
    EXPECT_FALSE(result.lines[1].operand);
    EXPECT_EQ(result.lines[1].comment, (SourceField{";done", {2, 6}}));
}

TEST(SourceScannerTest, OwnsAllTextAndHasNoStateAcrossInvocations) {
    SourceScanResult first;
    {
        std::string source = "DEST INCR VAL ; keep";
        first = SourceScanner::scan(source);
        source.assign("changed");
    }
    const auto second = SourceScanner::scan(" MCEND\n");
    ASSERT_EQ(first.lines.size(), 1U);
    EXPECT_EQ(first.lines[0].text, "DEST INCR VAL ; keep");
    EXPECT_EQ(first.lines[0].label, (SourceField{"DEST", {1, 1}}));
    EXPECT_EQ(first.lines[0].operation, (SourceField{"INCR", {1, 6}}));
    EXPECT_EQ(first.lines[0].operand, (SourceField{"VAL", {1, 11}}));
    EXPECT_EQ(first.lines[0].comment, (SourceField{"; keep", {1, 15}}));
    ASSERT_EQ(second.lines.size(), 1U);
    EXPECT_EQ(second.lines[0].number, 1U);
    EXPECT_FALSE(second.lines[0].label);
    EXPECT_TRUE(SourceScanner::scan("").lines.empty());
}

TEST(SourceScannerTest, PreservesEmbeddedNullBytesWithoutCStringTruncation) {
    const std::string source{" BYTE C'A\0B'\r\n", 14};
    const auto result = SourceScanner::scan(source);
    ASSERT_EQ(result.lines.size(), 1U);
    EXPECT_EQ(result.lines[0].operand, (SourceField{std::string{"C'A\0B'", 6}, {1, 7}}));
    EXPECT_FALSE(result.lines[0].openQuote);
    EXPECT_EQ(result.normalizedText(), (std::string{" BYTE C'A\0B'\n", 13}));
}

} // namespace
} // namespace macro
