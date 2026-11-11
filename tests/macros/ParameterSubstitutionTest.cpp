
#include "macros/ParameterSubstitution.hpp"

#include "macros/DefinitionCapture.hpp"
#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
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

std::vector<SourceLine> substitute(
    std::string_view definitionSource,
    std::string_view callSource) {

    const auto definition = captureSingleDefinition(definitionSource);

    const auto prototypeResult =
        MacroArguments::parsePrototype(definition);

    const auto& prototype = std::get<MacroPrototype>(prototypeResult);

    const auto call = scanSingleLine(callSource);
    const auto bindingResult = MacroArguments::bind(prototype, call);

    const auto& arguments =
        std::get<BoundMacroArguments>(bindingResult);

    const auto templateResult =
        MacroTemplate::fromDefinition(definition);

    const auto& macroTemplate =
        std::get<MacroTemplate>(templateResult.value);

    return ParameterSubstitution::substitute(
        macroTemplate,
        arguments);
}

std::vector<std::string> lineTexts(
    const std::vector<SourceLine>& lines) {

    std::vector<std::string> result;
    result.reserve(lines.size());

    for (const auto& line : lines) {
        result.push_back(line.text);
    }

    return result;
}

TEST(ParameterSubstitutionTest, ReplacesLabelAndOperandParameters) {
    const auto output = substitute(
        " MCDEFN\n"
        "DEST Sample VALUE\n"
        "DEST LDA VALUE\n"
        " STA DEST\n"
        " MCEND\n",
        "RESULT Sample BUFFER\n");

    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{
            "RESULT LDA BUFFER",
            " STA RESULT",
        }));
}

TEST(ParameterSubstitutionTest, PreservesQuotedTextCommentsAndSicXeMarkers) {
    const auto output = substitute(
        " MCDEFN\n"
        " Sample VALUE\n"
        " LDA #VALUE\n"
        " +LDA @VALUE\n"
        " BYTE C'VALUE'\n"
        " LDA VALUE,X ; VALUE\n"
        " MCEND\n",
        " Sample BUFFER\n");

    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{
            " LDA #BUFFER",
            " +LDA @BUFFER",
            " BYTE C'VALUE'",
            " LDA BUFFER,X ; VALUE",
        }));
}

TEST(ParameterSubstitutionTest, PreservesParametersDeclaredByNestedMacro) {
    const auto output = substitute(
        " MCDEFN\n"
        " Sample OUTER\n"
        " LDA OUTER\n"
        " MCDEFN\n"
        " INNER OUTER,INNER_VALUE\n"
        " LDA OUTER\n"
        " STA INNER_VALUE\n"
        " MCEND\n"
        " MCEND\n",
        " Sample BUFFER\n");

    EXPECT_EQ(
        lineTexts(output),
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
    const auto output = substitute(
        " MCDEFN\n"
        " Sample OUTER\n"
        " MCDEFN\n"
        " INNER LOCAL\n"
        " LDA OUTER\n"
        " STA LOCAL\n"
        " ADD OUTER\n"
        " MCEND\n"
        " MCEND\n",
        " Sample BUFFER\n");

    EXPECT_EQ(
        lineTexts(output),
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
    const auto output = substitute(
        " MCDEFN\n"
        " Sample VALUE\n"
        " LDA value\n"
        " STA MYVALUE\n"
        " ADD VALUE\n"
        " MCEND\n",
        " Sample BUFFER\n");

    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{
            " LDA BUFFER",
            " STA MYVALUE",
            " ADD BUFFER",
        }));
}

TEST(ParameterSubstitutionTest, ReplacesParameterWithEmptyArgument) {
    const auto output = substitute(
        " MCDEFN\n"
        " Sample VALUE\n"
        " LDA VALUE\n"
        " MCEND\n",
        " Sample \n");

    EXPECT_EQ(
        lineTexts(output),
        (std::vector<std::string>{
            " LDA ",
        }));
}

TEST(ParameterSubstitutionTest, PreservesThreeNestedScopeLevels) {
    const auto output = substitute(
        " MCDEFN\n"
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

    EXPECT_EQ(
        lineTexts(output),
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

} // namespace
} // namespace macro
