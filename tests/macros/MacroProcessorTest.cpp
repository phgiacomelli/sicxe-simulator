#include "macros/MacroProcessor.hpp"

#include <gtest/gtest.h>
#include <string>
#include <variant>

namespace {

macro::MacroSuccess success(std::string_view source) {
    auto result = macro::expand(source);
    EXPECT_TRUE(std::holds_alternative<macro::MacroSuccess>(result));
    if (auto* value = std::get_if<macro::MacroSuccess>(&result))
        return *value;
    return {};
}

macro::MacroDiagnostic failure(std::string_view source, macro::MacroDiagnosticCode code) {
    auto result = macro::expand(source);
    EXPECT_TRUE(std::holds_alternative<macro::MacroFailure>(result));
    if (auto* value = std::get_if<macro::MacroFailure>(&result)) {
        EXPECT_EQ(value->diagnostics.size(), 1u);
        if (!value->diagnostics.empty()) {
            EXPECT_EQ(value->diagnostics.front().code, code);
            return value->diagnostics.front();
        }
    }
    return {};
}

TEST(MacroProcessorTest, CopiesOpaqueAssemblyAndNormalizesOnlyTerminators) {
    const std::string source = "; hello\r\n  +UNKNOWN a=b,'open\rDUP WORD 1\nDUP WORD 2";
    const auto result = success(source);
    EXPECT_EQ(result.expandedText, "; hello\n  +UNKNOWN a=b,'open\nDUP WORD 1\nDUP WORD 2");
    ASSERT_EQ(result.origins.size(), 4u);
    EXPECT_TRUE(result.origins[0].calls.empty());
    EXPECT_EQ(result.origins[3].expandedLine, 4u);
    EXPECT_TRUE(success("").origins.empty());
}

TEST(MacroProcessorTest, ExpandsSimpleCallsWithLabelOmissionsAndInlineComment) {
    const auto result = success("  MCDEFN ; structural\n; ignored\nL Put A,B\nL WORD A\n  ADD B\n  "
                                "MCEND\nHERE put 7 ; invocation\n  Put ,8\n");
    EXPECT_EQ(result.expandedText, "; invocation\nHERE WORD 7\n  ADD \n WORD \n  ADD 8\n");
    ASSERT_EQ(result.origins.size(), 5u);
    EXPECT_EQ(result.origins[0].source.line, 7u);
    EXPECT_EQ(result.origins[0].source.column, 12u);
    EXPECT_EQ(result.origins[1].source.line, 4u);
    ASSERT_EQ(result.origins[1].calls.size(), 1u);
    EXPECT_EQ(result.origins[1].calls[0].invocation.line, 7u);
}

TEST(MacroProcessorTest, DoesNotCollectDefinitionsAheadOfTime) {
    EXPECT_EQ(
        success("  Later 1\n  MCDEFN\n  Later A\n  WORD A\n  MCEND\n  Later 2\n").expandedText,
        "  Later 1\n  WORD 2\n");
}

TEST(MacroProcessorTest, ExpandsDepthFirstAndRecordsEntireCallChain) {
    const auto result =
        success("  MCDEFN\n  Leaf A\n  WORD A\n  MCEND\n  MCDEFN\n  Middle A\n  Leaf A\n  WORD 2\n "
                " MCEND\n  MCDEFN\n  Outer\n  Middle 1\n  WORD 3\n  MCEND\n  Outer\n  WORD 4\n");
    EXPECT_EQ(result.expandedText, "  WORD 1\n  WORD 2\n  WORD 3\n  WORD 4\n");
    ASSERT_EQ(result.origins.size(), 4u);
    EXPECT_EQ(result.origins[0].calls.size(), 3u);
    EXPECT_EQ(result.origins[1].calls.size(), 2u);
    EXPECT_EQ(result.origins[2].calls.size(), 1u);
    EXPECT_EQ(result.origins[3].calls.size(), 0u);
    EXPECT_EQ(result.origins[0].source.line, 3u);
    EXPECT_EQ(result.origins[0].calls[2].invocation.line, 7u);
}

TEST(MacroProcessorTest, InstallsNestedDefinitionsWhenReachedAndFreezesOuterValues) {
    const auto result =
        success("  MCDEFN\n  Outer P\n  Inner 0\n  MCDEFN\n  Inner Q\n  ADD P\n  LDA Q\n  MCEND\n  "
                "Inner 7\n  MCEND\n  Inner 1\n  Outer Q\n  Inner 8\n  Outer Z\n  Inner 9\n");
    EXPECT_EQ(result.expandedText,
              "  Inner 1\n  Inner 0\n  ADD Q\n  LDA 7\n  ADD Q\n  LDA 8\n  ADD Q\n  LDA 0\n  ADD "
              "Z\n  LDA 7\n  ADD Z\n  LDA 9\n");
}

TEST(MacroProcessorTest, RedefinitionCannotChangeAnExpansionAlreadyStarted) {
    EXPECT_EQ(success("  MCDEFN\n  Same A\n  WORD A\n  MCDEFN\n  Same B\n  BYTE B\n  MCEND\n  WORD "
                      "A\n  MCEND\n  Same 1\n  Same 2\n")
                  .expandedText,
              "  WORD 1\n  WORD 1\n  BYTE 2\n");
}

TEST(MacroProcessorTest, UsesOneSerialPerCallAndResumesTheOuterSerial) {
    EXPECT_EQ(
        success("  MCDEFN\n  Inner\nJ.SER WORD 2\n  MCEND\n  MCDEFN\n  Outer\n  J LOOP.SER\n  "
                "Inner\nLOOP.SER WORD 1\n  MCEND\n  Outer\n  Outer\n")
            .expandedText,
        "  J LOOP_M1\nJ_M2 WORD 2\nLOOP_M1 WORD 1\n  J LOOP_M3\nJ_M4 WORD 2\nLOOP_M3 WORD 1\n");
}

TEST(MacroProcessorTest, TransfersGeneratedOccurrencesThroughPositionalAndLabelFormals) {
    EXPECT_EQ(
        success("  MCDEFN\n  Inner A\nA WORD 1\n  MCEND\n  MCDEFN\nL Labelled\nL WORD 2\n  MCEND\n "
                " MCDEFN\n  Outer\n  Inner LOOP.SER\nNEXT.SER Labelled\n  MCEND\n  Outer\n")
            .expandedText,
        "LOOP_M1 WORD 1\nNEXT_M1 WORD 2\n");
}

TEST(MacroProcessorTest, RetainsGeneratedIdentityInVersionedCaptures) {
    EXPECT_EQ(
        success("  MCDEFN\n  Save P\n  MCDEFN\n  Saved Q\nP WORD Q\n  MCEND\n  MCEND\n  MCDEFN\n  "
                "Outer\n  Save LOOP.SER\n  MCEND\n  Outer\n  Saved 7\n  Outer\n  Saved 8\n")
            .expandedText,
        "LOOP_M1 WORD 7\nLOOP_M4 WORD 8\n");
}

TEST(MacroProcessorTest, EqualSpellingDoesNotGrantIdentityToLiteralArgument) {
    failure("  MCDEFN\n  Inner A,B\nB WORD 1\n  MCEND\n  MCDEFN\n  Outer\n  Inner "
            "LOOP.SER,LOOP_M1\n  MCEND\n  Outer\n",
            macro::MacroDiagnosticCode::DuplicateLabel);
}

TEST(MacroProcessorTest, PreservesOccurrenceOffsetsAcrossSeveralReplacementsAndForwarding) {
    EXPECT_EQ(
        success("  MCDEFN\n  Sink A,B\nB WORD 1\n  MCEND\n  MCDEFN\n  Relay A,B\n  Sink A,B\n  "
                "MCEND\n  MCDEFN\n  Outer\n  Relay LONGNAME.SER,N.SER\n  MCEND\n  Outer\n")
            .expandedText,
        "N_M1 WORD 1\n");
}

TEST(MacroProcessorTest, DiagnosesInvalidDefinitionsAndDelimitersWithoutPartialSuccess) {
    using C = macro::MacroDiagnosticCode;
    failure("  WORD 1\n  MCEND\n", C::UnexpectedEnd);
    failure("  MCDEFN\n", C::MissingPrototype);
    failure("  MCDEFN\n  Foo\n", C::UnterminatedDefinition);
    failure("  MCDEFN BAD\n  Foo\n  MCEND\n", C::InvalidPrototype);
    failure("  MCDEFN\n  LDA\n  MCEND\n", C::ReservedName);
    failure("  MCDEFN\n  Foo A,A\n  MCEND\n", C::InvalidPrototype);
}

TEST(MacroProcessorTest, RejectsInvalidArgumentsAndLateErrorsWithContext) {
    using C = macro::MacroDiagnosticCode;
    const std::string prefix = "  MCDEFN\n  Foo A\n  WORD A\n  MCEND\n  Foo 1\n";
    failure(prefix + "  Foo 1,2\n", C::InvalidArgument);
    failure(prefix + "  Foo A=1\n", C::InvalidArgument);
    failure(prefix + "  Foo 'open\n", C::InvalidArgument);
    failure(prefix + "L Foo 1\n", C::InvalidArgument);
    auto error =
        failure(prefix + "  MCDEFN\n  Bar\n  Foo 1,2\n  MCEND\n  Bar\n", C::InvalidArgument);
    ASSERT_EQ(error.calls.size(), 1u);
    EXPECT_EQ(error.calls[0].macroName, "Bar");
    EXPECT_EQ(error.position.line, 8u);
}

TEST(MacroProcessorTest, DetectsDirectAndIndirectRecursion) {
    using C = macro::MacroDiagnosticCode;
    auto direct = failure("  MCDEFN\n  Foo\n  foo\n  MCEND\n  Foo\n", C::RecursiveExpansion);
    EXPECT_EQ(direct.calls.size(), 2u);
    auto indirect =
        failure("  MCDEFN\n  Foo\n  Bar\n  MCEND\n  MCDEFN\n  Bar\n  Foo\n  MCEND\n  Foo\n",
                C::RecursiveExpansion);
    EXPECT_EQ(indirect.calls.size(), 3u);
}

TEST(MacroProcessorTest, EnforcesDepthLimitWithoutReportingAFalseCycle) {
    std::string source;
    for (int i = 0; i < 129; ++i) {
        source += "  MCDEFN\n  M" + std::to_string(i) + "\n";
        source += i == 128 ? "  WORD 1\n" : "  M" + std::to_string(i + 1) + "\n";
        source += "  MCEND\n";
    }
    auto error = failure(source + "  M0\n", macro::MacroDiagnosticCode::ExpansionLimitExceeded);
    EXPECT_EQ(error.calls.size(), 129u);
}

TEST(MacroProcessorTest, EnforcesGeneratedLineBudgetIncludingNonFinalLines) {
    std::string source = "  MCDEFN\n  Leaf\n";
    for (int i = 0; i < 1000; ++i)
        source += "  WORD 1\n";
    source += "  MCEND\n  MCDEFN\n  Many\n";
    for (int i = 0; i < 100; ++i)
        source += "  Leaf\n";
    source += "  MCEND\n  Many\n";
    failure(source, macro::MacroDiagnosticCode::ExpansionLimitExceeded);
}

TEST(MacroProcessorTest, ResetsAllStateBetweenSources) {
    const std::string source = "  MCDEFN\n  Foo\nL.SER WORD 1\n  MCEND\n  Foo\n";
    EXPECT_EQ(success(source), success(source));
    EXPECT_EQ(success("  Foo\n").expandedText, "  Foo\n");
}

TEST(MacroProcessorTest, KeepsTemplatesAssociatedWithTheirOwnedDefinitionVersions) {
    std::string source;
    std::string expected;
    for (int i = 0; i < 200; ++i) {
        source += "  MCDEFN\n  Same\n  WORD " + std::to_string(i) + "\n  MCEND\n  Same\n";
        expected += "  WORD " + std::to_string(i) + "\n";
    }
    EXPECT_EQ(success(source).expandedText, expected);
}

TEST(MacroProcessorTest, KeepsIdentityThroughThreeLevelsOfCapturedBindings) {
    EXPECT_EQ(success("  MCDEFN\n  Save P\n  MCDEFN\n  Middle Q\n  MCDEFN\n  Inner R\nP WORD R\n  "
                      "ADD Q\n  MCEND\n  MCEND\n  MCEND\n  MCDEFN\n  Outer\n  Save LOOP.SER\n  "
                      "MCEND\n  Outer\n  Middle 7\n  Inner 8\n")
                  .expandedText,
              "LOOP_M1 WORD 8\n  ADD 7\n");
}

TEST(MacroProcessorTest, LiteralCapturedAlongsideGeneratedValueRetainsItsOwnIdentity) {
    failure("  MCDEFN\n  Save P,Q\n  MCDEFN\n  Saved\nQ WORD 1\n  MCEND\n  MCEND\n  MCDEFN\n  "
            "Outer\n  Save LOOP.SER,LOOP_M1\n  MCEND\n  Outer\n  Saved\n",
            macro::MacroDiagnosticCode::DuplicateLabel);
}

TEST(MacroProcessorTest, RejectsGeneratedCollisionsBeforeAndAfterAnExpansion) {
    const std::string definition = "  MCDEFN\n  Foo\n  J LOOP.SER\n  MCEND\n";
    failure("LOOP_M1 WORD 1\n" + definition + "  Foo\n",
            macro::MacroDiagnosticCode::DuplicateLabel);
    failure(definition + "  Foo\nLOOP_M1 WORD 1\n", macro::MacroDiagnosticCode::DuplicateLabel);
}

TEST(MacroProcessorTest, AcceptsExactlyTheDepthLimit) {
    std::string source;
    for (int i = 0; i < 128; ++i) {
        source += "  MCDEFN\n  M" + std::to_string(i) + "\n";
        source += i == 127 ? "  WORD 1\n" : "  M" + std::to_string(i + 1) + "\n";
        source += "  MCEND\n";
    }
    const auto result = success(source + "  M0");
    EXPECT_EQ(result.expandedText, "  WORD 1\n");
    ASSERT_EQ(result.origins.size(), 1u);
    EXPECT_EQ(result.origins[0].calls.size(), 128u);
}

TEST(MacroProcessorTest, RepositionsInheritedIdentityAfterLocalMarkerReplacement) {
    EXPECT_EQ(
        success("  MCDEFN\n  Sink A,B\nB WORD 1\n  MCEND\n  MCDEFN\n  Relay P\n  Sink "
                "LONGNAME.SER,P\n  MCEND\n  MCDEFN\n  Outer\n  Relay N.SER\n  MCEND\n  Outer\n")
            .expandedText,
        "N_M1 WORD 1\n");
}

TEST(MacroProcessorTest, AcceptsExactlyTheGeneratedLineLimitAndCopiesFurtherOriginalLines) {
    std::string source = "  MCDEFN\n  Leaf\n";
    for (int i = 0; i < 1000; ++i)
        source += "  WORD 1\n";
    source += "  MCEND\n";
    for (int i = 0; i < 100; ++i)
        source += "  Leaf\n";
    const auto result = success(source + "  WORD 2");
    ASSERT_EQ(result.origins.size(), 100001u);
    EXPECT_EQ(result.origins.back().expandedLine, 100001u);
    EXPECT_TRUE(result.origins.back().calls.empty());
    EXPECT_TRUE(result.expandedText.ends_with("  WORD 1\n  WORD 2"));
}

} // namespace
