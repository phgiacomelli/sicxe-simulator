#include "macros/DefinitionCapture.hpp"

#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>
#include <string>

namespace macro {
namespace {

void expectFailure(std::string_view source, MacroDiagnosticCode code, SourcePosition position) {
    const auto lines = SourceScanner::scan(source).lines;
    const auto result = DefinitionCapture::capture(lines, 0);
    const auto* failure = std::get_if<MacroDiagnostic>(&result);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->code, code);
    EXPECT_EQ(failure->position, position);
    EXPECT_FALSE(failure->message.empty());
    EXPECT_TRUE(failure->calls.empty());
}

TEST(DefinitionCaptureTest, CapturesPrototypeAndBodyWithoutParsingFormalsOrAssembly) {
    const auto lines = SourceScanner::scan(" MCDEFN\n"
                                           "DEST Incr VAL,OTHER\n"
                                           "LOOP.SER LDA #VAL\n"
                                           " SHIFTR RP\n"
                                           " BAD! C'open\n"
                                           " MCEND\n"
                                           " Incr COUNT\n")
                           .lines;
    const auto result = DefinitionCapture::capture(lines, 0);
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->nextLine, 6U);
    const auto& definition = captured->definition;
    EXPECT_EQ(definition.begin(), lines[0]);
    EXPECT_EQ(definition.prototype(), lines[1]);
    EXPECT_EQ(definition.name(), (SourceField{"Incr", {2, 6}}));
    EXPECT_EQ(definition.body(), (std::vector<SourceLine>{lines[2], lines[3], lines[4]}));
    EXPECT_EQ(definition.end(), lines[5]);
}

TEST(DefinitionCaptureTest, AcceptsEmptyBodyAndNoFinalTerminator) {
    const auto lines = SourceScanner::scan(" mcdefn\r\n Empty\r mcend").lines;
    const auto result = DefinitionCapture::capture(lines, 0);
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_TRUE(captured->definition.body().empty());
    EXPECT_FALSE(captured->definition.end().terminated);
    EXPECT_EQ(captured->nextLine, lines.size());
}

TEST(DefinitionCaptureTest, ClosesThreeLevelsAtTheirMatchingEndsAndPreservesInnerSkeletons) {
    const auto lines = SourceScanner::scan(" LDA #0\n"
                                           " MCDEFN\n Outer P\n"
                                           " MCDEFN ; inner begin\n"
                                           " . gap before inner prototype\n\n"
                                           "LBL Inner Q ; inner prototype\n"
                                           " MCDEFN\n Deep R\n MCEND ; deep end\n"
                                           " LDA P\n MCEND ; inner end\n"
                                           " STA P\n MCEND\n Outer VALUE\n")
                           .lines;
    const auto result = DefinitionCapture::capture(lines, 1);
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->nextLine, 14U);
    EXPECT_EQ(captured->definition.end(), lines[13]);
    EXPECT_EQ(captured->definition.body(),
              (std::vector<SourceLine>{lines.begin() + 3, lines.begin() + 13}));

    // A later consumer can capture from the saved skeleton with original positions.
    const auto innerResult = DefinitionCapture::capture(captured->definition.body(), 0);
    const auto* inner = std::get_if<CapturedDefinition>(&innerResult);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->definition.prototype(), lines[6]);
    EXPECT_EQ(inner->definition.end(), lines[11]);
    EXPECT_EQ(inner->nextLine, 9U);
}

TEST(DefinitionCaptureTest, SkipsOnlyBlankAndCommentLinesWhenLookingForPrototype) {
    const auto lines = SourceScanner::scan("\tMCDEFN ; begin '\r\n"
                                           "\r\n . MCEND '\r ; MCDEFN\n"
                                           "\tSample P ; prototype '\n"
                                           "\n . MCDEFN\n ; MCEND\n"
                                           " MCEND ; end '\n")
                           .lines;
    const auto result = DefinitionCapture::capture(lines, 0);
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->definition.prototype(), lines[4]);
    EXPECT_EQ(captured->definition.body(), (std::vector<SourceLine>{lines[5], lines[6], lines[7]}));
    EXPECT_EQ(captured->definition.begin().comment, lines[0].comment);
    EXPECT_EQ(captured->definition.end().comment, lines[8].comment);
    EXPECT_EQ(captured->nextLine, 9U);
}

TEST(DefinitionCaptureTest, DoesNotCountDirectiveNamesInOtherFieldsOrComments) {
    const auto lines = SourceScanner::scan(" MCDEFN\n Sample\n"
                                           "MCEND LDA MCDEFN\n"
                                           " BYTE C'MCEND;MCDEFN'\n"
                                           " +MCDEFN\n"
                                           " MCEND_EXTRA\n"
                                           " MCEND\n")
                           .lines;
    const auto result = DefinitionCapture::capture(lines, 0);
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->definition.body().size(), 4U);
    EXPECT_EQ(captured->nextLine, 7U);
}

TEST(DefinitionCaptureTest, OwnsCapturedTextFieldsAndPhysicalPositions) {
    const auto result = [] {
        std::string source = " . leading\r\n\t MCDEFN\r\n\tSample P\n LDA #P  \n MCEND";
        auto scan = SourceScanner::scan(source);
        auto captured = DefinitionCapture::capture(scan.lines, 1);
        source.assign("changed");
        scan.lines.clear();
        return captured;
    }();
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->definition.begin().operation->position, (SourcePosition{2, 3}));
    EXPECT_EQ(captured->definition.name(), (SourceField{"Sample", {3, 2}}));
    EXPECT_EQ(captured->definition.prototype().operand, (SourceField{"P", {3, 9}}));
    ASSERT_EQ(captured->definition.body().size(), 1U);
    EXPECT_EQ(captured->definition.body()[0].number, 4U);
    EXPECT_EQ(captured->definition.body()[0].text, " LDA #P  ");
    EXPECT_EQ(captured->definition.body()[0].operand, (SourceField{"#P", {4, 6}}));
    EXPECT_EQ(captured->definition.end().number, 5U);
}

TEST(DefinitionCaptureTest, LeavesAllFormalValidationToPackageThree) {
    const auto lines = SourceScanner::scan(" MCDEFN\n"
                                           "BAD! Sample A,A,N=V,C'open\n"
                                           " MCEND\n")
                           .lines;
    const auto result = DefinitionCapture::capture(lines, 0);
    const auto* captured = std::get_if<CapturedDefinition>(&result);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->definition.prototype(), lines[1]);
    EXPECT_TRUE(captured->definition.prototype().openQuote);
}

TEST(DefinitionCaptureTest, DiagnosesIsolatedEndAtOperation) {
    expectFailure("\t mcEND ; comment", MacroDiagnosticCode::UnexpectedEnd, {1, 3});
}

TEST(DefinitionCaptureTest, DiagnosesMissingPrototypeAtBegin) {
    for (const auto source :
         {" MCDEFN", " MCDEFN\n\n . comment\n", " MCDEFN\n MCEND\n", " MCDEFN\n MCDEFN\n"}) {
        expectFailure(source, MacroDiagnosticCode::MissingPrototype, {1, 2});
    }
}

TEST(DefinitionCaptureTest, DoesNotSkipAnInvalidActivePrototype) {
    expectFailure(" MCDEFN\nLABEL ; missing operation\n Sample\n MCEND",
                  MacroDiagnosticCode::InvalidPrototype,
                  {2, 1});
}

TEST(DefinitionCaptureTest, RejectsInvalidMacroIdentifiersAtTheirStart) {
    for (const auto name : {"1NAME", "_NAME", "BAD!", "LOOP.SER", "+LDA", "N=V", "N\xC3\xA3me"}) {
        expectFailure(" MCDEFN\n\t " + std::string{name} + " A\n MCEND",
                      MacroDiagnosticCode::InvalidPrototype,
                      {2, 3});
    }
}

TEST(DefinitionCaptureTest, ReusesAssemblerReservedNamesInAnyCase) {
    for (const auto name : {"lda", "aDdR", "SHIFTR", "Start", "bYtE", "NOBASE"}) {
        expectFailure(" MCDEFN\n " + std::string{name} + "\n MCEND",
                      MacroDiagnosticCode::ReservedName,
                      {2, 2});
    }
}

TEST(DefinitionCaptureTest, RejectsExtraBeginFieldsAtTheirPhysicalPositions) {
    expectFailure("LABEL MCDEFN\n Sample\n MCEND", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectFailure(" MCDEFN EXTRA\n Sample\n MCEND", MacroDiagnosticCode::InvalidPrototype, {1, 9});
}

TEST(DefinitionCaptureTest, RejectsExtraEndAndNestedDelimiterFields) {
    expectFailure(" MCDEFN\n Sample\nLABEL MCEND", MacroDiagnosticCode::InvalidPrototype, {3, 1});
    expectFailure(" MCDEFN\n Sample\n MCEND EXTRA", MacroDiagnosticCode::InvalidPrototype, {3, 8});
    expectFailure(" MCDEFN\n Sample\n MCDEFN EXTRA\n Inner\n MCEND\n MCEND",
                  MacroDiagnosticCode::InvalidPrototype,
                  {3, 9});
    expectFailure(" MCDEFN\n Sample\n MCDEFN\n Inner\n MCEND EXTRA\n MCEND",
                  MacroDiagnosticCode::InvalidPrototype,
                  {5, 8});
}

TEST(DefinitionCaptureTest, DiagnosesUnclosedDefinitionAtOriginalBegin) {
    expectFailure(" MCDEFN\n Sample", MacroDiagnosticCode::UnterminatedDefinition, {1, 2});
    expectFailure(" MCDEFN\n Outer\n MCDEFN\n Inner\n MCEND\n",
                  MacroDiagnosticCode::UnterminatedDefinition,
                  {1, 2});
    expectFailure(
        " MCDEFN\n Outer\n MCDEFN\n Inner\n", MacroDiagnosticCode::UnterminatedDefinition, {1, 2});
}

TEST(DefinitionCaptureTest, DoesNotReadPastTheRequestedClosingEnd) {
    const auto lines = SourceScanner::scan(" MCDEFN\n Sample\n MCEND\n MCEND").lines;
    const auto first = DefinitionCapture::capture(lines, 0);
    const auto* captured = std::get_if<CapturedDefinition>(&first);
    ASSERT_NE(captured, nullptr);
    EXPECT_EQ(captured->nextLine, 3U);
    const auto next = DefinitionCapture::capture(lines, captured->nextLine);
    const auto* failure = std::get_if<MacroDiagnostic>(&next);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->code, MacroDiagnosticCode::UnexpectedEnd);
    EXPECT_EQ(failure->position, (SourcePosition{4, 2}));
}

TEST(DefinitionCaptureTest, HandlesMissingInputAndIncorrectCaptureStart) {
    expectFailure("", MacroDiagnosticCode::MissingPrototype, {1, 1});
    expectFailure(" LDA #1", MacroDiagnosticCode::InvalidPrototype, {1, 2});
    const auto lines = SourceScanner::scan("\n LDA #1").lines;
    const auto result = DefinitionCapture::capture(lines, lines.size());
    const auto* failure = std::get_if<MacroDiagnostic>(&result);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->code, MacroDiagnosticCode::MissingPrototype);
    EXPECT_EQ(failure->position, (SourcePosition{3, 1}));
}

} // namespace
} // namespace macro
