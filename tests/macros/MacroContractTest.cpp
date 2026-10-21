#include "macros/MacroProcessor.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace macro {
namespace {

// Unevaluated expression: package 1 declares expand(), package 6 implements it.
static_assert(std::is_same_v<decltype(expand(std::string_view{})), MacroResult>);

TEST(MacroContractTest, FailureCarriesOwnedDiagnosticsAndNoSuccessPayload) {
    MacroResult result;
    {
        std::string message = "Argumento invalido.";
        std::string name = "OUTER";
        result = MacroFailure{
            {{MacroDiagnosticCode::InvalidArgument, {7, 13}, message, {{name, {1, 2}, {7, 3}}}}}};
        message.assign("changed");
        name.assign("changed");
    }
    const auto* failure = std::get_if<MacroFailure>(&result);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(std::get_if<MacroSuccess>(&result), nullptr);
    ASSERT_EQ(failure->diagnostics.size(), 1U);
    const auto& diagnostic = failure->diagnostics.front();
    EXPECT_EQ(diagnostic.code, MacroDiagnosticCode::InvalidArgument);
    EXPECT_EQ(diagnostic.position, (SourcePosition{7, 13}));
    EXPECT_EQ(diagnostic.message, "Argumento invalido.");
    ASSERT_EQ(diagnostic.calls.size(), 1U);
    EXPECT_EQ(diagnostic.calls.front(), (ExpansionFrame{"OUTER", {1, 2}, {7, 3}}));
}

TEST(MacroContractTest, SuccessOwnsExpandedTextAndOriginsWithAnOrderedCallChain) {
    MacroResult result;
    {
        std::string text = ". copied\n LDA #7\n";
        std::string outer = "OUTER";
        std::string inner = "INNER";
        result = MacroSuccess{
            text,
            {{1, {3, 1}, {}}, {2, {5, 1}, {{outer, {1, 2}, {12, 3}}, {inner, {4, 2}, {6, 2}}}}}};
        text.assign("changed");
        outer.assign("changed");
        inner.assign("changed");
    }
    const auto* success = std::get_if<MacroSuccess>(&result);
    ASSERT_NE(success, nullptr);
    EXPECT_EQ(std::get_if<MacroFailure>(&result), nullptr);
    EXPECT_EQ(success->expandedText, ". copied\n LDA #7\n");
    ASSERT_EQ(success->origins.size(), 2U);
    EXPECT_EQ(success->origins[0], (SourceOrigin{1, {3, 1}, {}}));
    EXPECT_EQ(success->origins[1].expandedLine, 2U);
    EXPECT_EQ(success->origins[1].source, (SourcePosition{5, 1}));
    EXPECT_EQ(success->origins[1].calls,
              (std::vector<ExpansionFrame>{{"OUTER", {1, 2}, {12, 3}}, {"INNER", {4, 2}, {6, 2}}}));
    auto copy = result;
    std::get<MacroSuccess>(copy).origins[1].calls[0].macroName = "CHANGED";
    EXPECT_EQ(success->origins[1].calls[0].macroName, "OUTER");
}

} // namespace
} // namespace macro
