#include "assembler/AssemblyResult.hpp"
#include "assembler/SourceParser.hpp"

#include <gtest/gtest.h>
#include <variant>

namespace {
using namespace assembler;

TEST(AssemblyContractTest, SuccessRepresentsFixedImageListingReservationsAndSymbols) {
    const auto parsed = SourceParser::parse("main LDA #5\nvalue RESW 1");
    ASSERT_TRUE(parsed.succeeded());
    const AssemblyResult result =
        AssemblySuccess{0x100,
                        0x100,
                        {0x01, 0x00, 0x05, 0x00, 0x00, 0x00},
                        {{1, parsed.lines[0].text, 0x100, {0x01, 0x00, 0x05}, 0},
                         {2, parsed.lines[1].text, 0x103, {}, 3}},
                        {{"MAIN", 0x100, {1, 1}}, {"VALUE", 0x103, {2, 1}}}};

    const auto* success = std::get_if<AssemblySuccess>(&result);
    ASSERT_NE(success, nullptr);
    EXPECT_EQ(success->loadAddress, 0x100U);
    EXPECT_EQ(success->initialPc, 0x100U);
    EXPECT_EQ(success->bytes, (std::vector<std::uint8_t>{0x01, 0, 5, 0, 0, 0}));
    ASSERT_EQ(success->listing.size(), 2U);
    EXPECT_EQ(success->listing[1].reservedBytes, 3U);
    EXPECT_TRUE(success->listing[1].bytes.empty());
    ASSERT_EQ(success->symbols.size(), 2U);
    EXPECT_EQ(success->symbols[1], (AssemblySymbol{"VALUE", 0x103, {2, 1}}));
    EXPECT_EQ(std::get_if<AssemblyFailure>(&result), nullptr);
}

TEST(AssemblyContractTest, FailureCarriesParserDiagnosticsWithoutSuccessPayload) {
    const auto parsed = SourceParser::parse("LABEL");
    ASSERT_FALSE(parsed.succeeded());
    const AssemblyResult result = AssemblyFailure{parsed.diagnostics};
    const auto* failure = std::get_if<AssemblyFailure>(&result);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->diagnostics, parsed.diagnostics);
    EXPECT_EQ(std::get_if<AssemblySuccess>(&result), nullptr);
}

} // namespace
