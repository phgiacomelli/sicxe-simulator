#include "macros/MacroTable.hpp"

#include "macros/DefinitionCapture.hpp"
#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>
#include <type_traits>

namespace macro {
namespace {

MacroDefinition definition(std::string_view source) {
    const auto lines = SourceScanner::scan(source).lines;
    return std::get<CapturedDefinition>(DefinitionCapture::capture(lines, 0)).definition;
}

TEST(MacroTableTest, FindsNamesWithoutCaseDistinctionsAndPreservesOriginalSpelling) {
    MacroTable table;
    EXPECT_EQ(table.find("Sample"), nullptr);
    table.define(definition(" MCDEFN\n sAmPlE_2 P\n LDA P\n MCEND"));
    const auto found = table.find("SAMPLE_2");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found, table.find("sample_2"));
    EXPECT_EQ(found, table.find("sAmPlE_2"));
    EXPECT_EQ(found->name(), (SourceField{"sAmPlE_2", {2, 2}}));
    EXPECT_EQ(table.find("Sample"), nullptr);
    EXPECT_EQ(table.find(""), nullptr);
}

TEST(MacroTableTest, RedefinesWithLargerSmallerAndEmptyBodiesWithoutChangingHeldVersions) {
    MacroTable table;
    table.define(definition(" MCDEFN\n Sample P\n LDA P\n MCEND"));
    const auto original = table.find("sample");
    ASSERT_NE(original, nullptr);
    const auto originalPrototype = original->prototype();
    const auto originalBody = original->body();

    table.define(definition(" MCDEFN\n SAMPLE Q\n LDA Q\n ADD #1\n STA Q\n MCEND"));
    const auto larger = table.find("Sample");
    ASSERT_NE(larger, nullptr);
    EXPECT_NE(larger, original);
    EXPECT_EQ(larger->body().size(), 3U);

    table.define(definition(" MCDEFN\n sample\n RSUB\n MCEND"));
    const auto smaller = table.find("Sample");
    ASSERT_NE(smaller, nullptr);
    EXPECT_EQ(smaller->body().size(), 1U);
    EXPECT_EQ(smaller->body()[0].text, " RSUB");

    table.define(definition(" MCDEFN\n SaMpLe\n MCEND"));
    ASSERT_NE(table.find("Sample"), nullptr);
    EXPECT_TRUE(table.find("sample")->body().empty());
    EXPECT_EQ(original->prototype(), originalPrototype);
    EXPECT_EQ(original->body(), originalBody);
    EXPECT_EQ(larger->body().size(), 3U);
    EXPECT_EQ(larger->prototype().operand, (SourceField{"Q", {2, 9}}));
    EXPECT_EQ(smaller->body()[0].text, " RSUB");
}

TEST(MacroTableTest, CapturingAndInstallingOuterDoesNotInstallInnerDefinitions) {
    const auto outer = definition(" MCDEFN\n Outer\n MCDEFN\n Inner\n MCEND\n MCEND");
    MacroTable table;
    EXPECT_EQ(table.find("Outer"), nullptr);
    EXPECT_EQ(table.find("Inner"), nullptr);
    table.define(outer);
    ASSERT_NE(table.find("outer"), nullptr);
    EXPECT_EQ(table.find("inner"), nullptr);
    ASSERT_EQ(table.find("Outer")->body().size(), 3U);
    EXPECT_EQ(table.find("Outer")->body()[0].directive(), MacroDirective::DefinitionBegin);

    // Only an explicit later installation makes the inner definition visible.
    const auto captured = DefinitionCapture::capture(outer.body(), 0);
    const auto* inner = std::get_if<CapturedDefinition>(&captured);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(table.find("inner"), nullptr);
    table.define(inner->definition);
    ASSERT_NE(table.find("inner"), nullptr);
    EXPECT_TRUE(table.find("inner")->body().empty());
}

TEST(MacroTableTest, HandlesOwnImmutableDataBeyondTableLifetimeAndTablesAreIndependent) {
    static_assert(std::is_const_v<MacroTable::Definition::element_type>);
    MacroTable::Definition saved;
    {
        MacroTable table;
        auto captured = definition(" MCDEFN\n Sample\n LDA #1\n MCEND");
        table.define(captured);
        saved = table.find("Sample");
        // Replacing the caller's value cannot mutate the installed version.
        captured = definition(" MCDEFN\n Other\n MCEND");
        EXPECT_EQ(table.find("Other"), nullptr);
        EXPECT_EQ(MacroTable{}.find("Sample"), nullptr);
    }
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(saved->name().text, "Sample");
    ASSERT_EQ(saved->body().size(), 1U);
    EXPECT_EQ(saved->body()[0].text, " LDA #1");
}

} // namespace
} // namespace macro
