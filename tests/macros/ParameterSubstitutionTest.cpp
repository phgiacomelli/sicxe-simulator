#include "macros/ParameterSubstitution.hpp"

#include "macros/DefinitionCapture.hpp"
#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace macro {
namespace {

SourceLine scanSingleLine(std::string_view text) {
    const auto lines = SourceScanner::scan(text).lines;
    return lines.empty() ? SourceLine{} : lines.front();
}

MacroDefinition captureSingleDefinition(std::string_view source) {
    const auto lines = SourceScanner::scan(source).lines;
    const auto result = DefinitionCapture::capture(lines, 0);

    return std::get<CapturedDefinition>(result).definition;
}

std::vector<SourceLine> substitute(std::string_view definitionSource, std::string_view callSource) {
    const auto definition = captureSingleDefinition(definitionSource);

    const auto prototypeResult = MacroArguments::parsePrototype(definition);

    const auto& prototype = std::get<MacroPrototype>(prototypeResult);

    const auto call = scanSingleLine(callSource);
    const auto bindingResult = MacroArguments::bind(prototype, call);

    const auto& arguments = std::get<BoundMacroArguments>(bindingResult);

    const auto templateResult = MacroTemplate::fromDefinition(definition);

    const auto& macroTemplate = std::get<MacroTemplate>(templateResult.value);

    return ParameterSubstitution::substitute(macroTemplate, arguments);
}

std::vector<std::string> lineTexts(const std::vector<SourceLine>& lines) {
    std::vector<std::string> result;
    result.reserve(lines.size());

    for (const auto& line : lines) {
        result.push_back(line.text);
    }

    return result;
}

TEST(ParameterSubstitutionTest, ReplacesLabelAndOperandParameters) {
    const auto output = substitute(" MCDEFN\n"
                                   "DEST Sample VALUE\n"
                                   "DEST LDA VALUE\n"
                                   " STA DEST\n"
                                   " MCEND\n",
                                   "RESULT Sample BUFFER\n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  "RESULT LDA BUFFER",
                  " STA RESULT",
              }));
}

TEST(ParameterSubstitutionTest, PreservesQuotedTextCommentsAndSicXeMarkers) {
    const auto output = substitute(" MCDEFN\n"
                                   " Sample VALUE\n"
                                   " LDA #VALUE\n"
                                   " +LDA @VALUE\n"
                                   " BYTE C'VALUE'\n"
                                   " LDA VALUE,X ; VALUE\n"
                                   " MCEND\n",
                                   " Sample BUFFER\n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  " LDA #BUFFER",
                  " +LDA @BUFFER",
                  " BYTE C'VALUE'",
                  " LDA BUFFER,X ; VALUE",
              }));
}

TEST(ParameterSubstitutionTest, PreservesParametersDeclaredByNestedMacro) {
    const auto output = substitute(" MCDEFN\n"
                                   " Sample OUTER\n"
                                   " LDA OUTER\n"
                                   " MCDEFN\n"
                                   " INNER OUTER,INNER_VALUE\n"
                                   " LDA OUTER\n"
                                   " STA INNER_VALUE\n"
                                   " MCEND\n"
                                   " MCEND\n",
                                   " Sample BUFFER\n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  " LDA BUFFER",
                  " MCDEFN",
                  " INNER OUTER,INNER_VALUE",
                  " LDA OUTER",
                  " STA INNER_VALUE",
                  " MCEND",
              }));
}

TEST(ParameterSubstitutionTest, ReplacesOuterParametersInsideNestedDefinitions) {
    const auto output = substitute(" MCDEFN\n"
                                   " Sample OUTER\n"
                                   " MCDEFN\n"
                                   " INNER LOCAL\n"
                                   " LDA OUTER\n"
                                   " STA LOCAL\n"
                                   " ADD OUTER\n"
                                   " MCEND\n"
                                   " MCEND\n",
                                   " Sample BUFFER\n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  " MCDEFN",
                  " INNER LOCAL",
                  " LDA BUFFER",
                  " STA LOCAL",
                  " ADD BUFFER",
                  " MCEND",
              }));
}

TEST(ParameterSubstitutionTest, MatchesIdentifiersCaseInsensitively) {
    const auto output = substitute(" MCDEFN\n"
                                   " Sample VALUE\n"
                                   " LDA value\n"
                                   " STA MYVALUE\n"
                                   " ADD VALUE\n"
                                   " MCEND\n",
                                   " Sample BUFFER\n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  " LDA BUFFER",
                  " STA MYVALUE",
                  " ADD BUFFER",
              }));
}

TEST(ParameterSubstitutionTest, ReplacesParameterWithEmptyArgument) {
    const auto output = substitute(" MCDEFN\n"
                                   " Sample VALUE\n"
                                   " LDA VALUE\n"
                                   " MCEND\n",
                                   " Sample \n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  " LDA ",
              }));
}

TEST(ParameterSubstitutionTest, PreservesThreeNestedScopeLevels) {
    const auto output = substitute(" MCDEFN\n"
                                   " SAMPLE OUTER_ARG\n"
                                   " MCDEFN\n"
                                   " INNER INNER_ARG\n"
                                   " LDA OUTER_ARG\n"
                                   " MCDEFN\n"
                                   " DEEP DEEP_ARG\n"
                                   " LDA OUTER_ARG\n"
                                   " STA INNER_ARG\n"
                                   " ADD DEEP_ARG\n"
                                   " MCEND\n"
                                   " MCEND\n"
                                   " MCEND\n",
                                   " SAMPLE BUFFER\n");

    EXPECT_EQ(lineTexts(output),
              (std::vector<std::string>{
                  " MCDEFN",
                  " INNER INNER_ARG",
                  " LDA BUFFER",
                  " MCDEFN",
                  " DEEP DEEP_ARG",
                  " LDA BUFFER",
                  " STA INNER_ARG",
                  " ADD DEEP_ARG",
                  " MCEND",
                  " MCEND",
              }));
}

TEST(ParameterSubstitutionTest, RemovesOmittedLabelBeforeBindingANestedCall) {
    const auto output = substitute(" MCDEFN\nDEST Outer VALUE\nDEST Inner VALUE\nDEST\n MCEND\n",
                                   " Outer BUFFER\n");
    ASSERT_EQ(output.size(), 2U);
    EXPECT_EQ(output[0].text, " Inner BUFFER");
    EXPECT_FALSE(output[0].label);
    EXPECT_EQ(output[0].operation, (SourceField{"Inner", {3, 2}}));
    EXPECT_EQ(output[0].operand, (SourceField{"BUFFER", {3, 8}}));
    const auto bound = MacroArguments::bind(scanSingleLine(" Inner ARG"), output[0]);
    ASSERT_TRUE(std::holds_alternative<BoundMacroArguments>(bound));
    EXPECT_EQ(std::get<BoundMacroArguments>(bound).values, (std::vector<std::string>{"BUFFER"}));
    EXPECT_EQ(output[1].kind, SourceLineKind::Blank);
    EXPECT_FALSE(output[1].label);
    EXPECT_TRUE(output[1].text.empty());
}

TEST(ParameterSubstitutionTest, RemovesEmptyOperandBeforeBindingANoArgumentCall) {
    const auto output =
        substitute(" MCDEFN\n Outer VALUE\n Inner VALUE ; keep\n MCEND\n", " Outer\n");
    ASSERT_EQ(output.size(), 1U);
    EXPECT_EQ(output[0].text, " Inner  ; keep");
    EXPECT_FALSE(output[0].operand);
    EXPECT_EQ(output[0].comment, (SourceField{"; keep", {3, 9}}));
    const auto bound = MacroArguments::bind(scanSingleLine(" Inner"), output[0]);
    ASSERT_TRUE(std::holds_alternative<BoundMacroArguments>(bound));
    EXPECT_TRUE(std::get<BoundMacroArguments>(bound).values.empty());
}

TEST(ParameterSubstitutionTest, DoesNotMatchInsideNumericOrMalformedIdentifierTokens) {
    const auto output = substitute(" MCDEFN\n Sample XAB,VALUE\n LDA #0xAB\n ADD #1VALUE\n"
                                   " STA _VALUE\n LDA VALUE\n MCEND\n",
                                   " Sample WRONG,BUFFER\n");
    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{" LDA #0xAB", " ADD #1VALUE", " STA _VALUE", " LDA BUFFER"}));
}

TEST(ParameterSubstitutionTest, PreservesLocalSerialNamesAsOpaqueTokens) {
    const auto output = substitute(" MCDEFN\n Sample LOOP,SER\nLOOP.SER LDA #1\n J LOOP.SER\n"
                                   " LDA LOOP\n ADD SER\n MCEND\n",
                                   " Sample BUFFER,OTHER\n");
    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{"LOOP.SER LDA #1", " J LOOP.SER", " LDA BUFFER", " ADD OTHER"}));
}

TEST(ParameterSubstitutionTest, SubstitutesAFormalNamedXInArgumentsWithoutClassifyingTheOperation) {
    const auto output =
        substitute(" MCDEFN\n Outer VALUE,X\n Inner VALUE,X\n MCEND\n", " Outer BUFFER,COUNT\n");
    EXPECT_EQ(lineTexts(output), (std::vector<std::string>{" Inner BUFFER,COUNT"}));
}

MacroTemplate compileTemplate(std::string_view source) {
    return std::get<MacroTemplate>(
        MacroTemplate::fromDefinition(captureSingleDefinition(source)).value);
}

SubstitutionResult instantiate(const MacroTemplate& macroTemplate, std::string_view call) {
    const auto bound = MacroArguments::bind(macroTemplate.root().prototype, scanSingleLine(call));
    return ParameterSubstitution::instantiate(macroTemplate, std::get<BoundMacroArguments>(bound));
}

TEST(ParameterSubstitutionTest, GeneratedFieldsMatchTextAndPreservePhysicalSourceNumbers) {
    const auto output =
        substitute(" MCDEFN\r\nDEST Sample VALUE\r\nDEST\tLDA\t#VALUE,VALUE ; note '\r\n MCEND",
                   "RESULT_LONG Sample BUFFER_LONG\n");
    ASSERT_EQ(output.size(), 1U);
    const auto& line = output.front();
    EXPECT_EQ(line.text, "RESULT_LONG\tLDA\t#BUFFER_LONG,BUFFER_LONG ; note '");
    EXPECT_EQ(line.number, 3U);
    EXPECT_TRUE(line.terminated);
    auto expected = scanSingleLine(line.text);
    expected.number = line.number;
    expected.terminated = line.terminated;
    for (auto* field :
         {&expected.label, &expected.operation, &expected.operand, &expected.comment}) {
        if (*field)
            (*field)->position.line = line.number;
    }
    EXPECT_EQ(line, expected);
}

TEST(ParameterSubstitutionTest, NeverReplacesOperationsCommentsOrQuotedConstantPrefixes) {
    const auto output =
        substitute(" MCDEFN\n Sample C,X,VALUE\n VALUE VALUE\n BYTE C'VALUE'\n BYTE X'CA'\n"
                   " . VALUE\n LDA VALUE ; VALUE\n MCEND\n",
                   " Sample OTHER,COUNT,BUFFER\n");
    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{
            " VALUE BUFFER", " BYTE C'VALUE'", " BYTE X'CA'", " . VALUE", " LDA BUFFER ; VALUE"}));
}

TEST(ParameterSubstitutionTest, PreparesInnerShiftCountWithoutBindingItsOwnFormal) {
    const auto root = compileTemplate(
        " MCDEFN\n Scale RP\n MCDEFN\n Inner VALUE\n SHIFTR A,RP\n LDA VALUE\n MCEND\n MCEND\n");
    const auto outer = instantiate(root, " Scale 3");
    ASSERT_EQ(outer.definitions.size(), 1U);
    EXPECT_EQ(lineTexts(outer.lines),
              (std::vector<std::string>{
                  " MCDEFN", " Inner VALUE", " SHIFTR A,3", " LDA VALUE", " MCEND"}));
    const auto inner = instantiate(outer.definitions.front().macroTemplate, " Inner BUFFER");
    EXPECT_EQ(lineTexts(inner.lines), (std::vector<std::string>{" SHIFTR A,3", " LDA BUFFER"}));
}

TEST(ParameterSubstitutionTest, DoesNotReinterpretAnOuterActualAsAnInnerFormal) {
    const auto root =
        compileTemplate(" MCDEFN\n Outer P\n MCDEFN\n Inner Q\n ADD P\n LDA Q\n MCEND\n MCEND\n");
    const auto outer = instantiate(root, " Outer Q");
    ASSERT_EQ(outer.definitions.size(), 1U);
    const auto& prepared = outer.definitions.front();
    EXPECT_EQ(prepared.beginLine, 0U);
    EXPECT_EQ(prepared.nextLine, outer.lines.size());
    const auto capture = DefinitionCapture::capture(outer.lines, prepared.beginLine);
    ASSERT_TRUE(std::holds_alternative<CapturedDefinition>(capture));
    EXPECT_EQ(std::get<CapturedDefinition>(capture).nextLine, prepared.nextLine);
    const auto inner = instantiate(prepared.macroTemplate, " Inner 7");
    EXPECT_EQ(lineTexts(inner.lines), (std::vector<std::string>{" ADD Q", " LDA 7"}));
}

TEST(ParameterSubstitutionTest, PreservesCapturedValuesThroughThreeSeparateInstantiations) {
    const auto root = compileTemplate(" MCDEFN\n Outer P\n MCDEFN\n Inner Q\n MCDEFN\n Deep R\n"
                                      " ADD P\n SUB Q\n LDA R\n MCEND\n MCEND\n MCEND\n");
    const auto outer = instantiate(root, " Outer Q");
    ASSERT_EQ(outer.definitions.size(), 1U);
    const auto inner = instantiate(outer.definitions.front().macroTemplate, " Inner R");
    ASSERT_EQ(inner.definitions.size(), 1U);
    const auto deep = instantiate(inner.definitions.front().macroTemplate, " Deep 7");
    EXPECT_EQ(lineTexts(deep.lines), (std::vector<std::string>{" ADD Q", " SUB R", " LDA 7"}));
}

TEST(ParameterSubstitutionTest, PreparedTemplatesOwnImmutableIndependentCapturedVersions) {
    using Nested = std::variant_alternative_t<1, TemplateElement>;
    static_assert(std::is_const_v<Nested::element_type>);
    const auto first = [] {
        std::string source = " MCDEFN\n Outer P\n MCDEFN\n Inner Q\n LDA P\n ADD Q\n MCEND\n MCEND";
        auto root = compileTemplate(source);
        const auto output = instantiate(root, " Outer FIRST");
        const auto second = instantiate(root, " Outer SECOND");
        EXPECT_EQ(
            lineTexts(instantiate(second.definitions.front().macroTemplate, " Inner #2").lines),
            (std::vector<std::string>{" LDA SECOND", " ADD #2"}));
        source.assign("changed");
        return output;
    }();
    ASSERT_EQ(first.definitions.size(), 1U);
    const auto& prepared = first.definitions.front().macroTemplate;
    EXPECT_EQ(lineTexts(instantiate(prepared, " Inner #1").lines),
              (std::vector<std::string>{" LDA FIRST", " ADD #1"}));
    EXPECT_EQ(lineTexts(instantiate(prepared, " Inner #3").lines),
              (std::vector<std::string>{" LDA FIRST", " ADD #3"}));
}

TEST(ParameterSubstitutionTest, PropagatesNestedPrototypeDiagnosticsWithoutPartialTemplate) {
    const auto definition =
        captureSingleDefinition(" MCDEFN\n Outer P\n MCDEFN\n Inner A,A\n MCEND\n MCEND");
    const auto result = MacroTemplate::fromDefinition(definition);
    const auto* failure = std::get_if<MacroDiagnostic>(&result.value);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->code, MacroDiagnosticCode::InvalidPrototype);
    EXPECT_EQ(failure->position, (SourcePosition{4, 10}));
    EXPECT_TRUE(failure->calls.empty());
}

TEST(ParameterSubstitutionTest, EmptyBodiesAndNestedRangesHaveNoInventedLines) {
    const auto empty = instantiate(compileTemplate(" MCDEFN\n Empty\n MCEND"), " Empty");
    EXPECT_TRUE(empty.lines.empty());
    EXPECT_TRUE(empty.definitions.empty());
    const auto root =
        compileTemplate(" MCDEFN\n Outer\n LDA #1\n MCDEFN\n Inner\n MCEND\n STA VALUE\n MCEND");
    const auto outer = instantiate(root, " Outer");
    ASSERT_EQ(outer.definitions.size(), 1U);
    EXPECT_EQ(outer.definitions.front().beginLine, 1U);
    EXPECT_EQ(outer.definitions.front().nextLine, 4U);
    const auto inner = instantiate(outer.definitions.front().macroTemplate, " Inner");
    EXPECT_TRUE(inner.lines.empty());
    EXPECT_TRUE(inner.definitions.empty());
}

} // namespace
} // namespace macro
