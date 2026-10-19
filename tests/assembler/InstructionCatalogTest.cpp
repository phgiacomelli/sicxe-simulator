#include "assembler/InstructionCatalog.hpp"

#include "InstructionTestSupport.hpp"

#include <gtest/gtest.h>
#include <set>
#include <type_traits>
#include <vector>

namespace assembler {
namespace {
TEST(InstructionCatalogTest, Exact38InstructionsOpcodesFormatsSizesAndPatterns) {
    const auto catalog = instructionCatalog();
    ASSERT_EQ(catalog.size(), 38u);
    std::set<std::string_view> names;
    std::set<std::uint8_t> opcodes;
    std::size_t format2 = 0;
    for (const auto& expected : test::ExpectedIsa) {
        SCOPED_TRACE(expected.name);
        const auto& actual = test::spec(expected.name);
        EXPECT_EQ(actual.mnemonic, expected.name);
        EXPECT_EQ(actual.opcode, expected.opcode);
        EXPECT_EQ(actual.opcode, static_cast<std::uint8_t>(expected.vmOpcode));
        EXPECT_EQ(actual.formats, expected.formats);
        EXPECT_EQ(actual.operands, expected.operands);
        EXPECT_EQ(actual.size(), expected.formats == InstructionFormats::Format2 ? 2 : 3);
        EXPECT_EQ(actual.supportsExtended(), expected.formats == InstructionFormats::Format34);
        format2 += actual.formats == InstructionFormats::Format2;
        names.insert(actual.mnemonic);
        opcodes.insert(actual.opcode);
    }
    EXPECT_EQ(names.size(), 38u);
    EXPECT_EQ(opcodes.size(), 38u);
    EXPECT_EQ(format2, 10u);
    EXPECT_EQ(catalog.size() - format2, 28u); // Includes RSUB, restricted to format 3.
}

TEST(InstructionCatalogTest, LookupIsCaseInsensitiveStableAndReadOnly) {
    static_assert(std::is_const_v<std::remove_reference_t<decltype(instructionCatalog()[0])>>);
    const auto before = instructionCatalog();
    const auto snapshot = std::vector<InstructionSpec>(before.begin(), before.end());
    for (const auto& expected : test::ExpectedIsa) {
        auto lower = std::string{expected.name};
        for (auto& c : lower) {
            c = static_cast<char>(c - 'A' + 'a');
        }
        EXPECT_EQ(&test::spec(lower), &test::spec(expected.name));
        if (expected.formats == InstructionFormats::Format34) {
            EXPECT_EQ(&test::spec("+" + lower), &test::spec(expected.name));
        }
    }
    EXPECT_EQ(std::vector<InstructionSpec>(before.begin(), before.end()), snapshot);
}

TEST(InstructionCatalogTest, RejectsDirectivesUnsupportedAndUnknownInstructionsAtSourcePosition) {
    for (const auto* name : {"START",
                             "END",
                             "BYTE",
                             "WORD",
                             "RESB",
                             "RESW",
                             "BASE",
                             "NOBASE",
                             "HALT",
                             "FIX",
                             "LDF",
                             "COPY",
                             "EQU",
                             "",
                             "++LDA"}) {
        SCOPED_TRACE(name);
        const auto result = findInstruction(name, {9, 4});
        ASSERT_TRUE(std::holds_alternative<InstructionDiagnostic>(result));
        const auto& failure = std::get<InstructionDiagnostic>(result);
        EXPECT_EQ(failure.code, InstructionDiagnosticCode::UnknownInstruction);
        EXPECT_EQ(failure.source.position, (SourcePosition{9, 4}));
        EXPECT_EQ(failure.source.text, name);
    }
}

TEST(InstructionCatalogTest, RejectsExtendedFormatForEveryFormat2AndRsub) {
    for (const auto& expected : test::ExpectedIsa) {
        if (expected.formats == InstructionFormats::Format34) {
            continue;
        }
        const auto result = findInstruction("+" + std::string{expected.name}, {2, 2});
        ASSERT_TRUE(std::holds_alternative<InstructionDiagnostic>(result));
        EXPECT_EQ(std::get<InstructionDiagnostic>(result).code,
                  InstructionDiagnosticCode::InvalidExtendedFormat);
        EXPECT_EQ(std::get<InstructionDiagnostic>(result).source.position, (SourcePosition{2, 2}));
    }
    const auto source = test::line("+rSuB");
    const auto result = findInstruction(source.statement->operation);
    EXPECT_EQ(std::get<InstructionDiagnostic>(result).source.text, "+rSuB");
}
} // namespace
} // namespace assembler
