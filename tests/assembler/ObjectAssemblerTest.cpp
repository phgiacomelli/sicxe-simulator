#include "assembler/ObjectAssembler.hpp"

#include "assembler/Assembler.hpp"
#include "linker/ObjectRecords.hpp"

#include <gtest/gtest.h>

namespace {
using namespace assembler;
using Bytes = std::vector<std::uint8_t>;

linker::ObjectModule object(std::string_view source) {
    auto result = assembleObject(source);
    if (auto* error = std::get_if<AssemblyFailure>(&result)) {
        for (const auto& diagnostic : error->diagnostics)
            ADD_FAILURE() << diagnostic.position.line << ':' << diagnostic.position.column << ' '
                          << diagnostic.message;
        return {};
    }
    return std::get<linker::ObjectModule>(result);
}

Bytes textBytes(const linker::ObjectModule& module) {
    Bytes bytes;
    for (const auto& record : module.text)
        bytes.insert(bytes.end(), record.bytes.begin(), record.bytes.end());
    return bytes;
}

void fails(std::string_view source) {
    const auto result = assembleObject(source);
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(result));
    EXPECT_FALSE(std::get<AssemblyFailure>(result).diagnostics.empty());
}

TEST(ObjectAssemblerTest, EmitsNormalizedHeaderExportsReferencesEntryAndExactRecords) {
    const auto module = object("Main START 0x1000\n EXTDEF Entry\n EXTREF Other\nEntry +JSUB "
                               "Other\nValue WORD Entry+Other-4\n RESB 5\n END Entry\n");
    EXPECT_EQ(module.header.name, "MAIN");
    EXPECT_EQ(module.header.length, 12u);
    ASSERT_EQ(module.definitions.size(), 1u);
    EXPECT_EQ(module.definitions[0].name, "ENTRY");
    EXPECT_EQ(module.definitions[0].offset, 0u);
    ASSERT_EQ(module.references.size(), 1u);
    EXPECT_EQ(module.references[0].name, "OTHER");
    EXPECT_EQ(module.entry.offset, 0u);
    EXPECT_EQ(textBytes(module), (Bytes{0x4B, 0x10, 0, 0, 0xFF, 0xFF, 0xFC}));
    const auto rendered = linker::renderObject(module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    EXPECT_EQ(
        std::get<std::string>(rendered),
        "HMAIN  00000000000C\nDENTRY 000000\nROTHER \nT000000074B100000FFFFFC\nM00000105+OTHER "
        "\nM00000406+MAIN  \nM00000406+OTHER \nE000000\n");
    EXPECT_EQ(module.modifications[0].position.record, 4u);
}

TEST(ObjectAssemblerTest, EncodesLocalAndExternalFormat4AcrossAllAddressingModes) {
    const auto module =
        object("Main START 0x2000\n EXTREF Ext\nHere WORD 0\n +JSUB Here\n +LDA #Here\n +LDA "
               "@Ext\n +LDA Ext,X\n +LDA 0xABCDE\n +LDA #0xFFFFF\n END\n");
    EXPECT_EQ(textBytes(module),
              (Bytes{0, 0,    0,    0x4B, 0x10, 0,    0,    0x01, 0x10, 0,    0,    0x02, 0x10, 0,
                     0, 0x03, 0x90, 0,    0,    0x03, 0x1A, 0xBC, 0xDE, 0x01, 0x1F, 0xFF, 0xFF}));
    ASSERT_EQ(module.modifications.size(), 4u);
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(module.modifications[i].offset, 4u + 4u * i);
        EXPECT_EQ(module.modifications[i].halfBytes, 5u);
        EXPECT_EQ(module.modifications[i].sign, linker::ModificationSign::Add);
        EXPECT_EQ(module.modifications[i].symbol, i < 2 ? "MAIN" : "EXT");
    }
    EXPECT_FALSE(module.entry.offset);
}

TEST(ObjectAssemblerTest, LocalFormat4AddendContainsTheSectionOffset) {
    const auto module = object("Main START 0x4000\n RESB 100\nHere +LDA Here\n END Here\n");
    ASSERT_EQ(module.text.size(), 1u);
    EXPECT_EQ(module.text[0].offset, 100u);
    EXPECT_EQ(module.text[0].bytes, (Bytes{3, 0x10, 0, 100}));
    EXPECT_EQ(module.modifications[0].offset, 101u);
    EXPECT_EQ(module.entry.offset, 100u);
}

TEST(ObjectAssemblerTest, WordUsesSignedTermsKnownAddendsAndCancelsLocalDifferences) {
    const auto module =
        object("Main START 0x1000\n EXTREF Ext,Other\nFirst WORD 1\nLast WORD Last-First\n WORD "
               "Last+Last-First+Ext-Other-7\n WORD First-Last-Last\n END\n");
    EXPECT_EQ(textBytes(module), (Bytes{0, 0, 1, 0, 0, 3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFA}));
    ASSERT_EQ(module.modifications.size(), 4u);
    EXPECT_EQ(module.modifications[0].offset, 6u);
    EXPECT_EQ(module.modifications[0].symbol, "MAIN");
    EXPECT_EQ(module.modifications[1].symbol, "EXT");
    EXPECT_EQ(module.modifications[2].symbol, "OTHER");
    EXPECT_EQ(module.modifications[2].sign, linker::ModificationSign::Subtract);
    EXPECT_EQ(module.modifications[3].offset, 9u);
    EXPECT_EQ(module.modifications[3].sign, linker::ModificationSign::Subtract);
    for (const auto& modification : module.modifications)
        EXPECT_EQ(modification.halfBytes, 6u);
}

TEST(ObjectAssemblerTest, RepeatedLocalCoefficientsProduceRepeatedModifications) {
    const auto module =
        object("Main START 0\nHere WORD Here+Here\n WORD Here-Here-Here-Here\n END\n");
    ASSERT_EQ(module.modifications.size(), 4u);
    EXPECT_EQ(module.modifications[0].sign, linker::ModificationSign::Add);
    EXPECT_EQ(module.modifications[1].sign, linker::ModificationSign::Add);
    EXPECT_EQ(module.modifications[2].sign, linker::ModificationSign::Subtract);
    EXPECT_EQ(module.modifications[3].sign, linker::ModificationSign::Subtract);
}

TEST(ObjectAssemblerTest, SplitsTextAtThirtyBytesAndPreservesReservationHoles) {
    const auto module = object("Main START 0\n BYTE C'abcdefghijklmnopqrstuvwxyz0123456789'\n RESB "
                               "8\n BYTE X'ABCD'\n RESW 2\n END\n");
    EXPECT_EQ(module.header.length, 52u);
    ASSERT_EQ(module.text.size(), 3u);
    EXPECT_EQ(module.text[0].bytes.size(), 30u);
    EXPECT_EQ(module.text[1].offset, 30u);
    EXPECT_EQ(module.text[1].bytes.size(), 6u);
    EXPECT_EQ(module.text[2].offset, 44u);
    EXPECT_EQ(module.text[2].bytes, (Bytes{0xAB, 0xCD}));
    EXPECT_TRUE(module.modifications.empty());
}

TEST(ObjectAssemblerTest, ModificationMayCrossAdjacentTextRecords) {
    const auto module = object("Main START 0\n RESB 0\n BYTE C'abcdefghijklmnopqrstuvwxyz12'\nHere "
                               "+LDA Here\n WORD Here\n END\n");
    ASSERT_EQ(module.text.size(), 2u);
    EXPECT_EQ(module.text[0].bytes.size(), 30u);
    EXPECT_EQ(module.modifications[0].offset, 29u);
    EXPECT_TRUE(std::holds_alternative<std::string>(linker::renderObject(module)));
}

TEST(ObjectAssemblerTest, EmptyAndReservationOnlySectionsContainNoArtificialText) {
    const auto empty = object("Main START 0\n END\n");
    EXPECT_EQ(empty.header.length, 0u);
    EXPECT_TRUE(empty.text.empty());
    EXPECT_FALSE(empty.entry.offset);
    const auto reserved = object("Main START 0\n RESB 0x100000\n END\n");
    EXPECT_EQ(reserved.header.length, 0x100000u);
    EXPECT_TRUE(reserved.text.empty());
}

TEST(ObjectAssemblerTest, AcceptsLastByteAndExclusiveEndOfMemory) {
    const auto module = object("Main START 0xFFFFC\nHere +LDA Here\n END Here\n");
    EXPECT_EQ(module.header.length, 4u);
    EXPECT_EQ(textBytes(module), (Bytes{3, 0x10, 0, 0}));
    EXPECT_EQ(module.modifications[0].offset, 1u);
    EXPECT_EQ(object("Main START 0xFFFFF\nHere BYTE X'FF'\n END Here\n").header.length, 1u);
}

TEST(ObjectAssemblerTest, MatchesAbsoluteBytesForNonRelocatingInstructionsAndData) {
    const std::string source =
        "Main START 0x1000\nHere LDA #7\n ADD Here\n LDA @Here\n LDA Here,X\n CLEAR X\n +LDA "
        "#0xABCDE\n RSUB\n BYTE X'ABCD'\n WORD -2\n END Here\n";
    const auto module = object(source);
    const auto absolute = assemble(source);
    ASSERT_TRUE(std::holds_alternative<AssemblySuccess>(absolute));
    EXPECT_EQ(textBytes(module), std::get<AssemblySuccess>(absolute).bytes);
    EXPECT_TRUE(module.modifications.empty());
}

TEST(ObjectAssemblerTest, PreservesLocalBaseStateAndNoBaseDisablesIt) {
    const std::string prefix = "Main START 0x1000\n BASE Far\n LDA Far\n";
    const auto module = object(prefix + " RESB 4096\nFar WORD 1\n END\n");
    EXPECT_EQ(module.text[0].bytes, (Bytes{3, 0x40, 0}));
    EXPECT_TRUE(module.modifications.empty());
    fails("Main START 0\n BASE Far\n NOBASE\n LDA Far\n RESB 4096\nFar WORD 1\n END\n");
}

TEST(ObjectAssemblerTest, FailsOnLateEncodingErrorsWithoutReturningPartialObjects) {
    fails("Main START 0\n WORD 1\n LDA 4096\n END\n");
    fails("Main START 0\n WORD 1\n WORD 0xFFFFFF+1\n END\n");
    fails("Main START 0\n WORD 1\n WORD -0x800000-1\n END\n");
    fails("Main START 0\n WORD 1\n +LDA 0x100000\n END\n");
    fails("Main START 0xFFFFF\n WORD 1\n END\n");
    fails("Main START 0\n EXTREF Ext\n LDA Ext\n END\n");
    fails("Main START 0\n EXTREF Ext\n BASE Ext\n END\n");
}

TEST(ObjectAssemblerTest, PropagatesFirstPassDiagnosticsAndRejectsDivergentLayout) {
    const auto invalid = ObjectPassOne::run("Main START 0\n LDA Missing\n END\n");
    const auto failed = ObjectPassTwo::run(invalid);
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(failed));
    EXPECT_EQ(std::get<AssemblyFailure>(failed).diagnostics[0],
              toAssemblyDiagnostic(invalid.diagnostics[0]));
    auto pass = ObjectPassOne::run("Main START 0\n WORD 1\n END\n");
    ASSERT_TRUE(pass.succeeded());
    pass.lines[1].offset = 1;
    EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(ObjectPassTwo::run(pass)));
}

TEST(ObjectAssemblerTest, RoundTripsPayloadAndOwnsItsValuesBetweenCalls) {
    std::string source = "Main START 0\n EXTREF Ext\nHere +JSUB Ext\n WORD Here+Ext\n END Here\n";
    auto module = object(source);
    source.assign("destroyed");
    const auto rendered = linker::renderObject(module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    const auto parsed = linker::parseObject(std::get<std::string>(rendered));
    ASSERT_TRUE(std::holds_alternative<linker::ObjectModule>(parsed));
    EXPECT_EQ(linker::renderObject(std::get<linker::ObjectModule>(parsed)), rendered);
    EXPECT_EQ(module,
              object("Main START 0\n EXTREF Ext\nHere +JSUB Ext\n WORD Here+Ext\n END Here\n"));
    EXPECT_TRUE(object("Other START 0\n END\n").references.empty());
}

TEST(ObjectAssemblerTest, BaseZeroRemainsActiveAndPcAddressingTakesPrecedence) {
    const auto module =
        object("Main START 0\nHere WORD 1\n BASE Here\n LDA Here\n RESB 4096\n LDA Here\n END\n");
    ASSERT_EQ(module.text.size(), 2u);
    EXPECT_EQ(module.text[0].bytes, (Bytes{0, 0, 1, 3, 0x2F, 0xFA}));
    EXPECT_EQ(module.text[1].bytes, (Bytes{3, 0x40, 0}));
    EXPECT_TRUE(module.modifications.empty());
}

TEST(ObjectAssemblerTest, RejectsOutOfRangeResolvedTargetsAndInvalidSnapshots) {
    auto pass = ObjectPassOne::run("Main START 0\nHere +LDA Here\n END\n");
    ASSERT_TRUE(pass.succeeded());
    std::get<LocalSymbolReference>(pass.lines[1].memoryOperand->target).offset = 0x100000;
    const auto result = ObjectPassTwo::run(pass);
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(result));
    EXPECT_EQ(std::get<AssemblyFailure>(result).diagnostics[0].code,
              AssemblyDiagnosticCode::ValueOutOfRange);
    pass = ObjectPassOne::run("Main START 0\nHere +LDA Here\n END\n");
    pass.lines[1].located.source.statement.reset();
    EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(ObjectPassTwo::run(pass)));
}

} // namespace
