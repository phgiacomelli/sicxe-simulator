#include "linker/LinkPassTwo.hpp"

#include <array>
#include <gtest/gtest.h>
#include <utility>

namespace linker {
namespace {

ObjectModule module(std::string name, std::uint32_t length) {
    ObjectModule result;
    result.header.name = std::move(name);
    result.header.length = length;
    return result;
}

TEST(LinkPassTwoTest, CombinesTextAppliesKnownTermsOnceAndKeepsResidualCoefficient) {
    auto main = module("MAIN", 9);
    main.references.push_back({"EXT", {"main.obj", 2, 1}});
    main.text.push_back({0, {0xAF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02}, {"main.obj", 3, 1}});
    main.text.push_back({7, {0x55, 0x66}, {"main.obj", 4, 1}});
    main.modifications = {
        {1, 5, ModificationSign::Add, "EXT", {"main.obj", 5, 1}},
        {1, 5, ModificationSign::Add, "EXT", {"main.obj", 6, 1}},
        {4, 6, ModificationSign::Add, "EXT", {"main.obj", 7, 1}},
        {4, 6, ModificationSign::Subtract, "EXT", {"main.obj", 8, 1}},
    };
    auto external = module("EXTSEC", 1);
    external.definitions.push_back({"EXT", 0, {"ext.obj", 2, 1}});
    external.text.push_back({0, {0x00}, {"ext.obj", 3, 1}});
    const std::array modules{main, external};

    const auto first = LinkPassOne::run(modules);
    ASSERT_TRUE(first.succeeded());
    const auto result = LinkPassTwo::run(modules, first);

    ASSERT_TRUE(result.succeeded());
    const auto& value = *result.intermediate;
    ASSERT_EQ(value.text.size(), 10U);
    EXPECT_EQ(value.totalLength, 10U);
    EXPECT_EQ(value.entryOffset, 0U);
    EXPECT_EQ(value.text[0], 0xAF);
    EXPECT_EQ(value.text[1], 0x00);
    EXPECT_EQ(value.text[2], 0x00);
    EXPECT_EQ(value.text[3], 0x13); // initial 1 + EXT(9) + EXT(9)
    EXPECT_EQ(value.text[4], 0x00);
    EXPECT_EQ(value.text[5], 0x00);
    EXPECT_EQ(value.text[6], 0x02); // +EXT-EXT cancels without changing addend
    EXPECT_EQ(value.text[7], 0x55);
    EXPECT_EQ(value.text[8], 0x66);
    EXPECT_EQ(value.text[9], 0x00);
    ASSERT_EQ(value.residuals.size(), 1U);
    EXPECT_EQ(value.residuals[0].offset, 1U);
    EXPECT_EQ(value.residuals[0].halfBytes, 5U);
    EXPECT_EQ(value.residuals[0].loadAddressCoefficient, 2);
    EXPECT_EQ(value.residuals[0].valueBeforeLoad, 0x13);
}

TEST(LinkPassTwoTest, ShiftsPatchLocationsBySectionBaseAndPreservesHoles) {
    auto firstModule = module("FIRST", 4);
    firstModule.text.push_back({2, {0x01, 0x02}, {"first.obj", 2, 1}});
    auto secondModule = module("SECOND", 4);
    secondModule.text.push_back({1, {0xA0, 0x00, 0x03}, {"second.obj", 3, 1}});
    secondModule.modifications.push_back(
        {1, 5, ModificationSign::Add, "SECOND", {"second.obj", 4, 1}});
    const std::array modules{firstModule, secondModule};

    const auto first = LinkPassOne::run(modules);
    ASSERT_TRUE(first.succeeded());
    const auto result = LinkPassTwo::run(modules, first);

    ASSERT_TRUE(result.succeeded());
    const auto& text = result.intermediate->text;
    EXPECT_FALSE(text[0].has_value());
    EXPECT_FALSE(text[1].has_value());
    EXPECT_EQ(text[2], 0x01);
    EXPECT_EQ(text[3], 0x02);
    EXPECT_FALSE(text[4].has_value());
    EXPECT_EQ(text[5], 0xA0); // format 4 flag nibble retained
    EXPECT_EQ(text[6], 0x00);
    EXPECT_EQ(text[7], 0x07); // add SECOND section base (four) exactly once
    ASSERT_EQ(result.intermediate->residuals.size(), 1U);
    EXPECT_EQ(result.intermediate->residuals[0].offset, 5U);
}

TEST(LinkPassTwoTest, UndefinedGlobalFailsWithoutPublishingPatchedIntermediate) {
    auto input = module("MAIN", 3);
    input.references.push_back({"MISS", {"main.obj", 2, 1}});
    input.text.push_back({0, {0x00, 0x00, 0x05}, {"main.obj", 3, 1}});
    input.modifications.push_back({0, 6, ModificationSign::Add, "MISS", {"main.obj", 4, 1}});
    const std::array modules{input};
    const auto first = LinkPassOne::run(modules);
    ASSERT_TRUE(first.succeeded());

    const auto result = LinkPassTwo::run(modules, first);

    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.intermediate.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::UndeclaredReference);
}

TEST(LinkPassTwoTest, RejectsModificationsOverUninitializedHoles) {
    auto input = module("MAIN", 4);
    input.text.push_back({0, {0xAA}, {"main.obj", 2, 1}});
    input.modifications.push_back({0, 5, ModificationSign::Add, "MAIN", {"main.obj", 3, 1}});
    const std::array modules{input};
    const auto first = LinkPassOne::run(modules);

    EXPECT_FALSE(first.succeeded()); // first-pass structural validation catches the hole
    const auto result = LinkPassTwo::run(modules, first);
    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.intermediate.has_value());
    EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::MissingText);
}

TEST(LinkPassTwoTest, RejectsDistinctModificationFieldsThatShareBytes) {
    auto input = module("MAIN", 4);
    input.text.push_back({0, {0, 0, 0, 0}, {"main.obj", 2, 1}});
    input.modifications = {
        {1, 6, ModificationSign::Add, "MAIN", {"main.obj", 3, 1}},
        {0, 5, ModificationSign::Add, "MAIN", {"main.obj", 4, 1}},
    };
    const std::array modules{input};
    const auto first = LinkPassOne::run(modules);
    ASSERT_TRUE(first.succeeded());
    const auto result = LinkPassTwo::run(modules, first);
    EXPECT_FALSE(result.intermediate);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, ObjectDiagnosticCode::InvalidModification);
    EXPECT_EQ(result.diagnostics.front().position.file, "main.obj");
}

TEST(LinkPassTwoTest, PreservesExactAddressesAndUsesModularWordsBeforeLoad) {
    auto main = module("MAIN", 12);
    main.references.push_back({"EXT", {}});
    main.text.push_back(
        {0, {0xAF, 0xFF, 0xFF, 0xB0, 0, 0, 0xCF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {}});
    main.modifications = {
        {0, 5, ModificationSign::Add, "EXT", {}},
        {3, 5, ModificationSign::Add, "MAIN", {}},
        {3, 5, ModificationSign::Add, "MAIN", {}},
        {3, 5, ModificationSign::Subtract, "EXT", {}},
        {6, 5, ModificationSign::Add, "EXT", {}},
        {6, 5, ModificationSign::Subtract, "MAIN", {}},
        {9, 6, ModificationSign::Add, "EXT", {}},
    };
    const std::array modules{main, module("EXT", 1)};
    const auto first = LinkPassOne::run(modules);
    ASSERT_TRUE(first.succeeded());
    const auto result = LinkPassTwo::run(modules, first);
    ASSERT_TRUE(result.succeeded());
    const auto& value = *result.intermediate;
    EXPECT_EQ(value.text[0], 0xA0);
    EXPECT_EQ(value.text[3], 0xBF);
    EXPECT_EQ(value.text[6], 0xC0);
    EXPECT_EQ(value.text[9], 0x00);
    EXPECT_EQ(value.text[10], 0x00);
    EXPECT_EQ(value.text[11], 0x0B);
    ASSERT_EQ(value.residuals.size(), 4U);
    EXPECT_EQ(value.residuals[0].valueBeforeLoad, 0x10000B);
    EXPECT_EQ(value.residuals[0].loadAddressCoefficient, 1);
    EXPECT_EQ(value.residuals[1].valueBeforeLoad, -12);
    EXPECT_EQ(value.residuals[1].loadAddressCoefficient, 1);
    EXPECT_EQ(value.residuals[2].valueBeforeLoad, 0x10000B);
    EXPECT_EQ(value.residuals[2].loadAddressCoefficient, 0);
    EXPECT_EQ(value.residuals[3].valueBeforeLoad, 0x0B);
}

TEST(LinkPassTwoTest, RejectsModulesInAnOrderThatDoesNotMatchTheLayout) {
    const std::array modules{module("FIRST", 4), module("SECOND", 4)};
    const auto first = LinkPassOne::run(modules);
    ASSERT_TRUE(first.succeeded());
    const std::array reversed{modules[1], modules[0]};
    const auto result = LinkPassTwo::run(reversed, first);
    EXPECT_FALSE(result.succeeded());
    EXPECT_FALSE(result.intermediate);
    EXPECT_FALSE(result.diagnostics.empty());
}

} // namespace
} // namespace linker
