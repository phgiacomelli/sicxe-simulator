#include "macros/MacroArguments.hpp"

#include "macros/DefinitionCapture.hpp"
#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace macro {
namespace {

SourceLine scanSingleLine(std::string_view text) {
    const auto lines = SourceScanner::scan(text).lines;
    return lines.empty() ? SourceLine{} : lines.front();
}

MacroDefinition captureSingleDefinition(std::string_view text) {
    const auto lines = SourceScanner::scan(text).lines;
    return std::get<CapturedDefinition>(DefinitionCapture::capture(lines, 0)).definition;
}

void expectPrototypeFailure(std::string_view source,
                            MacroDiagnosticCode code,
                            SourcePosition position) {
    const auto line = scanSingleLine(source);
    const auto result = MacroArguments::parsePrototype(line);
    const auto* failure = std::get_if<MacroDiagnostic>(&result);
    ASSERT_NE(failure, nullptr) << source;
    EXPECT_EQ(failure->code, code) << source;
    EXPECT_EQ(failure->position, position) << source;
    EXPECT_FALSE(failure->message.empty()) << source;
    EXPECT_TRUE(failure->calls.empty()) << source;
}

void expectCallFailure(std::string_view source, MacroDiagnosticCode code, SourcePosition position) {
    const auto line = scanSingleLine(source);
    const auto result = MacroArguments::parseCall(line);
    const auto* failure = std::get_if<MacroDiagnostic>(&result);
    ASSERT_NE(failure, nullptr) << source;
    EXPECT_EQ(failure->code, code) << source;
    EXPECT_EQ(failure->position, position) << source;
    EXPECT_FALSE(failure->message.empty()) << source;
    EXPECT_TRUE(failure->calls.empty()) << source;
}

void expectBindFailure(std::string_view prototypeSource,
                       std::string_view callSource,
                       MacroDiagnosticCode code,
                       SourcePosition position) {
    const auto prototypeLine = scanSingleLine(prototypeSource);
    const auto callLine = scanSingleLine(callSource);
    const auto result = MacroArguments::bind(prototypeLine, callLine);
    const auto* failure = std::get_if<MacroDiagnostic>(&result);
    ASSERT_NE(failure, nullptr) << prototypeSource << " -> " << callSource;
    EXPECT_EQ(failure->code, code) << prototypeSource << " -> " << callSource;
    EXPECT_EQ(failure->position, position) << prototypeSource << " -> " << callSource;
    EXPECT_FALSE(failure->message.empty()) << prototypeSource << " -> " << callSource;
    EXPECT_TRUE(failure->calls.empty()) << prototypeSource << " -> " << callSource;
}

TEST(MacroArgumentsTest, ParsesPrototypeWithLabelAndPositionalFormalsPreservingSpellingAndKeys) {
    const auto definition =
        captureSingleDefinition(" MCDEFN\nDest\tInCr_2\tVal_1 , Other_2 ; comment\n MCEND\n");
    const auto result = MacroArguments::parsePrototype(definition);
    const auto* prototype = std::get_if<MacroPrototype>(&result);
    ASSERT_NE(prototype, nullptr);
    EXPECT_EQ(prototype->macroName, (SourceField{"InCr_2", {2, 6}}));
    EXPECT_EQ(prototype->macroKey, "INCR_2");
    EXPECT_EQ(prototype->labelFormal, (SourceField{"Dest", {2, 1}}));
    EXPECT_EQ(prototype->positionalFormals,
              (std::vector<SourceField>{{"Val_1", {2, 13}}, {"Other_2", {2, 21}}}));
    ASSERT_EQ(prototype->formals.size(), 3U);
    EXPECT_EQ(prototype->formals[0],
              (MacroFormal{MacroFormalKind::Label, {"Dest", {2, 1}}, "DEST"}));
    EXPECT_EQ(prototype->formals[1],
              (MacroFormal{MacroFormalKind::Positional, {"Val_1", {2, 13}}, "VAL_1"}));
    EXPECT_EQ(prototype->formals[2],
              (MacroFormal{MacroFormalKind::Positional, {"Other_2", {2, 21}}, "OTHER_2"}));
    EXPECT_EQ(prototype->findFormal("dest"), 0U);
    EXPECT_EQ(prototype->findFormal("VAL_1"), 1U);
    EXPECT_EQ(prototype->findFormal("oThEr_2"), 2U);
    EXPECT_EQ(prototype->findFormal("Missing"), std::nullopt);
    EXPECT_EQ(prototype->findFormal("1BAD"), std::nullopt);

    const auto noParams = MacroArguments::parsePrototype(scanSingleLine(" Simple"));
    const auto* simple = std::get_if<MacroPrototype>(&noParams);
    ASSERT_NE(simple, nullptr);
    EXPECT_FALSE(simple->labelFormal.has_value());
    EXPECT_TRUE(simple->positionalFormals.empty());
    EXPECT_TRUE(simple->formals.empty());
}

TEST(MacroArgumentsTest, BindsCompletePositionalArgumentsAndLabelFormalInOrder) {
    const auto prototype =
        std::get<MacroPrototype>(MacroArguments::parsePrototype(scanSingleLine("DEST INCR A, B")));
    const auto result =
        MacroArguments::bind(prototype, scanSingleLine("MARCA incr COUNT , #1 ; call"));
    const auto* bound = std::get_if<BoundMacroArguments>(&result);
    ASSERT_NE(bound, nullptr);
    EXPECT_EQ(bound->macroName, (SourceField{"incr", {1, 7}}));
    EXPECT_EQ(bound->labelValue, std::optional<std::string>{"MARCA"});
    EXPECT_EQ(bound->positionalValues, (std::vector<std::string>{"COUNT", "#1"}));
    EXPECT_EQ(bound->values, (std::vector<std::string>{"MARCA", "COUNT", "#1"}));
    ASSERT_EQ(bound->arguments.size(), 3U);
    EXPECT_EQ(bound->arguments[0],
              (BoundMacroArgument{
                  {MacroFormalKind::Label, {"DEST", {1, 1}}, "DEST"},
                  "MARCA",
                  SourcePosition{1, 1},
              }));
    EXPECT_EQ(bound->arguments[1],
              (BoundMacroArgument{
                  {MacroFormalKind::Positional, {"A", {1, 11}}, "A"},
                  "COUNT",
                  SourcePosition{1, 12},
              }));
    EXPECT_EQ(bound->arguments[2],
              (BoundMacroArgument{
                  {MacroFormalKind::Positional, {"B", {1, 14}}, "B"},
                  "#1",
                  SourcePosition{1, 20},
              }));
    ASSERT_NE(bound->find("dest"), nullptr);
    EXPECT_EQ(bound->find("dest")->value, "MARCA");
    ASSERT_NE(bound->find("a"), nullptr);
    EXPECT_EQ(bound->find("a")->value, "COUNT");
    ASSERT_NE(bound->find("B"), nullptr);
    EXPECT_EQ(bound->find("B")->value, "#1");
    EXPECT_EQ(bound->find("missing"), nullptr);
    EXPECT_EQ(bound->find("BAD!"), nullptr);
}

TEST(MacroArgumentsTest, BindsEmptyCommaPositionsAndTrailingOmissionsToEmptyText) {
    const auto prototype =
        std::get<MacroPrototype>(MacroArguments::parsePrototype(scanSingleLine(" Sample P, Q, R")));

    const auto middleEmpty = MacroArguments::bind(prototype, scanSingleLine(" Sample A, , C"));
    const auto* middle = std::get_if<BoundMacroArguments>(&middleEmpty);
    ASSERT_NE(middle, nullptr);
    EXPECT_FALSE(middle->labelValue.has_value());
    EXPECT_EQ(middle->positionalValues, (std::vector<std::string>{"A", "", "C"}));
    EXPECT_EQ(middle->values, (std::vector<std::string>{"A", "", "C"}));
    EXPECT_EQ(middle->arguments[1].position, (SourcePosition{1, 12}));

    const auto leadingAndTrailingComma =
        MacroArguments::bind(prototype, scanSingleLine(" Sample , B,"));
    const auto* edges = std::get_if<BoundMacroArguments>(&leadingAndTrailingComma);
    ASSERT_NE(edges, nullptr);
    EXPECT_EQ(edges->values, (std::vector<std::string>{"", "B", ""}));
    EXPECT_EQ(edges->arguments[0].position, (SourcePosition{1, 9}));
    EXPECT_EQ(edges->arguments[1].position, (SourcePosition{1, 11}));
    EXPECT_EQ(edges->arguments[2].position, (SourcePosition{1, 13}));

    const auto fewerArgs = MacroArguments::bind(prototype, scanSingleLine(" Sample ONLY"));
    const auto* fewer = std::get_if<BoundMacroArguments>(&fewerArgs);
    ASSERT_NE(fewer, nullptr);
    EXPECT_EQ(fewer->values, (std::vector<std::string>{"ONLY", "", ""}));
    EXPECT_EQ(fewer->arguments[0].position, (SourcePosition{1, 9}));
    EXPECT_EQ(fewer->arguments[1].position, std::nullopt);
    EXPECT_EQ(fewer->arguments[2].position, std::nullopt);

    const auto singleComma = MacroArguments::bind(prototype, scanSingleLine(" Sample ,"));
    const auto* comma = std::get_if<BoundMacroArguments>(&singleComma);
    ASSERT_NE(comma, nullptr);
    EXPECT_EQ(comma->values, (std::vector<std::string>{"", "", ""}));
    EXPECT_EQ(comma->arguments[0].position, (SourcePosition{1, 9}));
    EXPECT_EQ(comma->arguments[1].position, (SourcePosition{1, 10}));
    EXPECT_EQ(comma->arguments[2].position, std::nullopt);

    const auto allOmitted = MacroArguments::bind(prototype, scanSingleLine(" Sample"));
    const auto* omitted = std::get_if<BoundMacroArguments>(&allOmitted);
    ASSERT_NE(omitted, nullptr);
    EXPECT_EQ(omitted->values, (std::vector<std::string>{"", "", ""}));
    for (const auto& argument : omitted->arguments) {
        EXPECT_TRUE(argument.value.empty());
        EXPECT_EQ(argument.position, std::nullopt);
    }
}

TEST(MacroArgumentsTest, HandlesLabelFormalFilledOmittedAndRejectsLabeledCallWithoutLabelFormal) {
    const auto withLabel =
        std::get<MacroPrototype>(MacroArguments::parsePrototype(scanSingleLine("DEST INCR VAL")));
    const auto omittedLabel = MacroArguments::bind(withLabel, scanSingleLine(" INCR COUNT"));
    const auto* boundOmitted = std::get_if<BoundMacroArguments>(&omittedLabel);
    ASSERT_NE(boundOmitted, nullptr);
    EXPECT_EQ(boundOmitted->labelValue, std::optional<std::string>{""});
    EXPECT_EQ(boundOmitted->values, (std::vector<std::string>{"", "COUNT"}));
    EXPECT_EQ(boundOmitted->arguments[0].position, std::nullopt);
    EXPECT_EQ(boundOmitted->arguments[1].position, (SourcePosition{1, 7}));

    const auto localLabelCall =
        MacroArguments::bind(withLabel, scanSingleLine("LOOP.SER INCR COUNT"));
    const auto* boundLocal = std::get_if<BoundMacroArguments>(&localLabelCall);
    ASSERT_NE(boundLocal, nullptr);
    EXPECT_EQ(boundLocal->labelValue, std::optional<std::string>{"LOOP.SER"});
    EXPECT_EQ(boundLocal->arguments[0].position, (SourcePosition{1, 1}));

    expectBindFailure(
        " INCR VAL", "MARCA INCR COUNT", MacroDiagnosticCode::InvalidArgument, {1, 1});
    expectBindFailure(
        " INCR VAL", "MARCA INCR C'open", MacroDiagnosticCode::InvalidArgument, {1, 1});
}

TEST(MacroArgumentsTest,
     PreservesSicXeMarkersQuotedSeparatorsAndSplitsIndexedOperandAsTwoArguments) {
    const auto prototype = std::get<MacroPrototype>(
        MacroArguments::parsePrototype(scanSingleLine(" Sample A, B, C, D, E")));
    const auto result = MacroArguments::bind(
        prototype, scanSingleLine(" Sample #7, @PTR, C'A,B;C=D', BUFFER,X ; inline comment"));
    const auto* bound = std::get_if<BoundMacroArguments>(&result);
    ASSERT_NE(bound, nullptr);
    EXPECT_EQ(bound->values, (std::vector<std::string>{"#7", "@PTR", "C'A,B;C=D'", "BUFFER", "X"}));

    // BUFFER,X is two positional arguments, so a single-parameter macro rejects ,X as excess.
    expectBindFailure(
        " LoadOne TARGET", " LoadOne BUFFER,X", MacroDiagnosticCode::InvalidArgument, {1, 17});
}

TEST(MacroArgumentsTest, RejectsDuplicateFormalsInPrototypeIgnoringCase) {
    expectPrototypeFailure(" Sample A, A", MacroDiagnosticCode::InvalidPrototype, {1, 12});
    expectPrototypeFailure(" Sample Val, val", MacroDiagnosticCode::InvalidPrototype, {1, 14});
    expectPrototypeFailure(" Sample A, B, a", MacroDiagnosticCode::InvalidPrototype, {1, 15});
    expectPrototypeFailure("DEST Sample DEST", MacroDiagnosticCode::InvalidPrototype, {1, 13});
    expectPrototypeFailure("Dest Sample A, dEsT", MacroDiagnosticCode::InvalidPrototype, {1, 16});
}

TEST(MacroArgumentsTest, RejectsKeywordAndDefaultSyntaxInPrototypeAndCall) {
    expectPrototypeFailure("DEST=1 Sample A", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure(" Sample A=1", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample A, B = 2", MacroDiagnosticCode::InvalidPrototype, {1, 12});
    expectPrototypeFailure(" Sample A=", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample =1", MacroDiagnosticCode::InvalidPrototype, {1, 9});

    expectCallFailure("DEST=L1 Sample A", MacroDiagnosticCode::InvalidArgument, {1, 1});
    expectCallFailure(" Sample A=1", MacroDiagnosticCode::InvalidArgument, {1, 9});
    expectCallFailure(" Sample X, B = 2", MacroDiagnosticCode::InvalidArgument, {1, 12});
    expectCallFailure(" Sample A=", MacroDiagnosticCode::InvalidArgument, {1, 9});
    expectCallFailure(" Sample =1", MacroDiagnosticCode::InvalidArgument, {1, 9});
    expectCallFailure(" Sample C'A'=1", MacroDiagnosticCode::InvalidArgument, {1, 9});

    expectBindFailure(
        "DEST Sample A, B", "DEST=L1 Sample 1, 2", MacroDiagnosticCode::InvalidArgument, {1, 1});
    expectBindFailure(
        " Sample A, B", " Sample A=1, 2", MacroDiagnosticCode::InvalidArgument, {1, 9});
    expectBindFailure(
        " Sample A, B", " Sample 1, B=2", MacroDiagnosticCode::InvalidArgument, {1, 12});
}

TEST(MacroArgumentsTest, RejectsExcessArgumentsAtFirstExtraArgumentPosition) {
    expectBindFailure(" NoArgs", " NoArgs 1", MacroDiagnosticCode::InvalidArgument, {1, 9});
    expectBindFailure(" NoArgs", " NoArgs ,", MacroDiagnosticCode::InvalidArgument, {1, 9});
    expectBindFailure(
        " TwoArgs A, B", " TwoArgs 1, 2, 3", MacroDiagnosticCode::InvalidArgument, {1, 16});
    expectBindFailure(
        " TwoArgs A, B", " TwoArgs 1, 2, 3, 4", MacroDiagnosticCode::InvalidArgument, {1, 16});
    expectBindFailure(
        " TwoArgs A, B", " TwoArgs 1, 2,", MacroDiagnosticCode::InvalidArgument, {1, 15});
    expectBindFailure(
        " TwoArgs A, B", " TwoArgs ,,", MacroDiagnosticCode::InvalidArgument, {1, 12});
    expectBindFailure(
        " OneArg A", " OneArg 1, EXTRA, BAD=2", MacroDiagnosticCode::InvalidArgument, {1, 12});
    expectBindFailure(
        " OneArg A", " OneArg BAD=2, EXTRA", MacroDiagnosticCode::InvalidArgument, {1, 9});
}

TEST(MacroArgumentsTest, RejectsInvalidPrototypeLinesIdentifiersEmptyFormalsAndUnclosedQuotes) {
    expectPrototypeFailure("", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure(" . comment", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure("LABEL ; missing op", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure(" MCDEFN", MacroDiagnosticCode::InvalidPrototype, {1, 2});
    expectPrototypeFailure(" MCEND", MacroDiagnosticCode::InvalidPrototype, {1, 2});
    expectPrototypeFailure(" 1BAD A", MacroDiagnosticCode::InvalidPrototype, {1, 2});
    expectPrototypeFailure(" LDA A", MacroDiagnosticCode::ReservedName, {1, 2});
    expectPrototypeFailure(" START A", MacroDiagnosticCode::ReservedName, {1, 2});

    expectPrototypeFailure("1BAD Sample A", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure("LOOP.SER Sample A", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure(" Sample ,A", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample A,,B", MacroDiagnosticCode::InvalidPrototype, {1, 11});
    expectPrototypeFailure(" Sample A,", MacroDiagnosticCode::InvalidPrototype, {1, 11});
    expectPrototypeFailure(" Sample 1A", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample #A", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample LOOP.SER", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample A B", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample C'A'", MacroDiagnosticCode::InvalidPrototype, {1, 9});
    expectPrototypeFailure(" Sample A, C'open", MacroDiagnosticCode::InvalidPrototype, {1, 13});

    // Validates the deferred prototype cases from DefinitionCaptureTest in left-to-right order.
    expectPrototypeFailure(
        "BAD! Sample A,A,N=V,C'open", MacroDiagnosticCode::InvalidPrototype, {1, 1});
    expectPrototypeFailure(
        " Sample A,A,N=V,C'open", MacroDiagnosticCode::InvalidPrototype, {1, 11});
    expectPrototypeFailure(" Sample A,N=V,C'open", MacroDiagnosticCode::InvalidPrototype, {1, 11});
    expectPrototypeFailure(" Sample A,C'open", MacroDiagnosticCode::InvalidPrototype, {1, 12});
}

TEST(MacroArgumentsTest, ParsesCallsSeparatelyAndPropagatesPrototypeErrorsInDirectBind) {
    const auto callResult =
        MacroArguments::parseCall(scanSingleLine("MARCA\tsAmPlE\t#7 , , C'A,B;C=D' ; comment"));
    const auto* call = std::get_if<MacroCall>(&callResult);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->macroName, (SourceField{"sAmPlE", {1, 7}}));
    EXPECT_EQ(call->macroKey, "SAMPLE");
    EXPECT_EQ(call->label, (SourceField{"MARCA", {1, 1}}));
    EXPECT_EQ(call->arguments,
              (std::vector<SourceField>{
                  {"#7", {1, 14}},
                  {"", {1, 19}},
                  {"C'A,B;C=D'", {1, 21}},
              }));

    const auto prototype = std::get<MacroPrototype>(
        MacroArguments::parsePrototype(scanSingleLine("DEST Sample P, Q, R, S")));
    const auto boundResult = MacroArguments::bind(prototype, *call);
    const auto* bound = std::get_if<BoundMacroArguments>(&boundResult);
    ASSERT_NE(bound, nullptr);
    EXPECT_EQ(bound->values, (std::vector<std::string>{"MARCA", "#7", "", "C'A,B;C=D'", ""}));

    expectBindFailure(
        " Sample A, A", " Sample 1, 2", MacroDiagnosticCode::InvalidPrototype, {1, 12});
}

TEST(MacroArgumentsTest, RejectsInvalidCallLinesMismatchedNamesAndUnclosedQuotes) {
    expectCallFailure("", MacroDiagnosticCode::InvalidArgument, {1, 1});
    expectCallFailure(" . comment", MacroDiagnosticCode::InvalidArgument, {1, 1});
    expectCallFailure("LABEL ; missing op", MacroDiagnosticCode::InvalidArgument, {1, 1});
    expectCallFailure(" MCDEFN", MacroDiagnosticCode::InvalidArgument, {1, 2});
    expectCallFailure(" MCEND", MacroDiagnosticCode::InvalidArgument, {1, 2});
    expectCallFailure(" +Sample A", MacroDiagnosticCode::InvalidArgument, {1, 2});
    expectCallFailure(" Sample C'open", MacroDiagnosticCode::InvalidArgument, {1, 10});
    expectCallFailure(" Sample A, C'open, B", MacroDiagnosticCode::InvalidArgument, {1, 13});

    expectBindFailure(" Sample A", " Other A", MacroDiagnosticCode::InvalidArgument, {1, 2});
    expectBindFailure(" Sample A", " Sample C'open", MacroDiagnosticCode::InvalidArgument, {1, 10});
}

TEST(MacroArgumentsTest, OwnsAllStringsAndPreservesEmbeddedNullBytesAcrossInvocations) {
    BoundMacroArguments bound;
    {
        std::string definitionSource = " MCDEFN\nDEST Sample A, B\n MCEND\n";
        std::string callSource{"MARCA Sample C'A\0B', #7\r\n", 25};
        auto definition = captureSingleDefinition(definitionSource);
        auto callLine = scanSingleLine(callSource);
        auto result = MacroArguments::bind(definition, callLine);
        definitionSource.assign("changed");
        callSource.assign("changed");
        ASSERT_TRUE(std::holds_alternative<BoundMacroArguments>(result));
        bound = std::get<BoundMacroArguments>(std::move(result));
    }
    EXPECT_EQ(bound.labelValue, std::optional<std::string>{"MARCA"});
    ASSERT_EQ(bound.positionalValues.size(), 2U);
    EXPECT_EQ(bound.positionalValues[0], (std::string{"C'A\0B'", 6}));
    EXPECT_EQ(bound.positionalValues[1], "#7");
    ASSERT_NE(bound.find("a"), nullptr);
    EXPECT_EQ(bound.find("a")->value, (std::string{"C'A\0B'", 6}));
}

TEST(MacroArgumentsTest, DoesNotClassifyAssemblerOperationsWhenParsingCalls) {
    // The engine selects active macros before parsing a call. Reserved-name
    // validation belongs to prototype definition, not to this lexical step.
    for (const auto name : {"lda", "START", "UnknownMacro"}) {
        const auto result =
            MacroArguments::parseCall(scanSingleLine(" " + std::string{name} + " #7"));
        const auto* call = std::get_if<MacroCall>(&result);
        ASSERT_NE(call, nullptr) << name;
        EXPECT_EQ(call->macroName.text, name);
        ASSERT_EQ(call->arguments.size(), 1U);
        EXPECT_EQ(call->arguments[0], (SourceField{"#7", {1, std::string{name}.size() + 3}}));
    }
    expectPrototypeFailure(" LDA A", MacroDiagnosticCode::ReservedName, {1, 2});
    expectPrototypeFailure(" START A", MacroDiagnosticCode::ReservedName, {1, 2});
    expectBindFailure(" Sample A", " LDA #7", MacroDiagnosticCode::InvalidArgument, {1, 2});
}

} // namespace
} // namespace macro
