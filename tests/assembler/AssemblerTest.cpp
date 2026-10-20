#include "assembler/Assembler.hpp"

#include "assembler/ListingWriter.hpp"
#include "assembler/PassOne.hpp"
#include "assembler/PassTwo.hpp"
#include "assembler/RawImageWriter.hpp"

#include <gtest/gtest.h>

namespace assembler {
namespace {

TEST(AssemblerTest, AssemblesInstructionsAtFixedStartWithEntryAndOriginalListing) {
    const auto result = assemble("PROG START 0x100\n"
                                 "ENTRY LDA #5\n"
                                 " ADD #7\n"
                                 " END ENTRY\n");
    const auto* success = std::get_if<AssemblySuccess>(&result);
    ASSERT_NE(success, nullptr);
    EXPECT_EQ(success->loadAddress, 0x100U);
    EXPECT_EQ(success->initialPc, 0x100U);
    EXPECT_EQ(success->bytes, (std::vector<std::uint8_t>{0x01, 0, 5, 0x19, 0, 7}));
    ASSERT_EQ(success->listing.size(), 4U);
    EXPECT_EQ(success->listing[1],
              (AssemblyListingLine{2, "ENTRY LDA #5", 0x100, {0x01, 0, 5}, 0}));
    ASSERT_EQ(success->symbols.size(), 2U);
    EXPECT_EQ(success->symbols[1], (AssemblySymbol{"ENTRY", 0x100, {2, 1}}));
}

TEST(AssemblerTest, RendersListingWithSourceCommentsReservationsAndSymbols) {
    const auto result = assemble(". demo\n"
                                 "PROG START 0x100\n"
                                 "ENTRY LDA #5 ; load\n"
                                 "BUF RESB 2\n"
                                 " END ENTRY\n");
    const auto* success = std::get_if<AssemblySuccess>(&result);
    ASSERT_NE(success, nullptr);
    EXPECT_EQ(success->bytes, (std::vector<std::uint8_t>{0x01, 0, 5, 0, 0}));
    EXPECT_EQ(ListingWriter::render(*success),
              "LOAD 000100  ENTRY 000100  SIZE 5 bytes\n"
              "LINE\tADDRESS\tOBJECT/RESERVE\tSOURCE\n"
              "1\t\t\t. demo\n"
              "2\t000100\t\tPROG START 0x100\n"
              "3\t000100\t01 00 05\tENTRY LDA #5 ; load\n"
              "4\t000103\tRESERVE 2\tBUF RESB 2\n"
              "5\t\t\t END ENTRY\n"
              "\nSYMBOL\tADDRESS\tDEFINITION\n"
              "PROG\t000100\t2:1\n"
              "ENTRY\t000100\t3:1\n"
              "BUF\t000103\t4:1\n");
}

TEST(AssemblerTest, EncodesForwardWordAsciiAndRawBytesAndZerosOnlyTheImageReservation) {
    const auto result = assemble(" START 0x20\n"
                                 "PTR WORD VALUE\n"
                                 "TEXT BYTE C'Ab ;'\n"
                                 "RAW BYTE X'00ff80'\n"
                                 "GAP RESW 1\n"
                                 "VALUE WORD -1\n"
                                 " END PTR\n");
    const auto* program = std::get_if<AssemblySuccess>(&result);
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(program->bytes,
              (std::vector<std::uint8_t>{
                  0, 0, 0x2D, 0x41, 0x62, 0x20, 0x3B, 0, 0xFF, 0x80, 0, 0, 0, 0xFF, 0xFF, 0xFF}));
    EXPECT_EQ(program->listing[4].reservedBytes, 3U);
    EXPECT_TRUE(program->listing[4].bytes.empty());
    EXPECT_EQ(program->listing[5].address, 0x2DU);
}

TEST(AssemblerTest, MaintainsBaseInSourceOrderAndNoBaseDisablesFallback) {
    const std::string prefix = " BASE FAR\n LDA FAR\n NOBASE\n";
    const std::string suffix = "GAP RESB 4096\nFAR WORD 7\n END\n";
    const auto active = assemble(prefix + " +LDA FAR\n" + suffix);
    const auto* program = std::get_if<AssemblySuccess>(&active);
    ASSERT_NE(program, nullptr);
    EXPECT_EQ((std::vector<std::uint8_t>{program->bytes.begin(), program->bytes.begin() + 7}),
              (std::vector<std::uint8_t>{0x03, 0x40, 0, 0x03, 0x10, 0x10, 0x07}));
    const auto disabled = assemble(prefix + " LDA FAR\n" + suffix);
    const auto* failure = std::get_if<AssemblyFailure>(&disabled);
    ASSERT_NE(failure, nullptr);
    ASSERT_EQ(failure->diagnostics.size(), 1U);
    EXPECT_EQ(failure->diagnostics[0].code, AssemblyDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(failure->diagnostics[0].position, (SourcePosition{4, 6}));
}

TEST(AssemblerTest, BaseZeroIsActiveAndDoesNotLeakIntoAnotherAssembly) {
    const std::string body = " LDA TARGET\n RESB 4092\nTARGET WORD 7\n END\n";
    const auto active = assemble(" BASE 0\n" + body);
    const auto* program = std::get_if<AssemblySuccess>(&active);
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(program->bytes[1], 0x4F);
    EXPECT_EQ(program->bytes[2], 0xFF);
    EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(assemble(body)));
}

TEST(AssemblerTest, SyntaxSemanticAndLateEncodingFailuresExposeOnlyDiagnostics) {
    struct Example {
        const char* source;
        AssemblyDiagnosticCode code;
        SourcePosition position;
    };
    const Example examples[] = {
        {"BROKEN", AssemblyDiagnosticCode::MissingOperation, {1, 7}},
        {"ITEM WORD 0\nITEM WORD 1\n END\n", AssemblyDiagnosticCode::DuplicateSymbol, {2, 1}},
        {" LDA MISSING\n END\n", AssemblyDiagnosticCode::UndefinedSymbol, {1, 6}},
        {" ADDF #1\n END\n", AssemblyDiagnosticCode::InvalidOperation, {1, 2}},
        {" LDA #1\n LDA FAR\n RESB 4096\nFAR WORD 0\n END\n",
         AssemblyDiagnosticCode::ValueOutOfRange,
         {2, 6}},
    };
    for (const auto& example : examples) {
        SCOPED_TRACE(example.source);
        const auto result = assemble(example.source);
        const auto* failure = std::get_if<AssemblyFailure>(&result);
        ASSERT_NE(failure, nullptr);
        ASSERT_FALSE(failure->diagnostics.empty());
        EXPECT_EQ(failure->diagnostics[0].code, example.code);
        EXPECT_EQ(failure->diagnostics[0].position, example.position);
    }
}

TEST(AssemblerTest, RejectsUnsupportedFeaturesAndInvalidInstructionOperands) {
    for (const auto* source : {" ORG 0\n END",
                               " EQU 1\n END",
                               " LTORG\n END",
                               " LDA =5\n END",
                               " LDA #VALUE,X\nVALUE WORD 1\n END",
                               " SHIFTL A,0\n END",
                               " CLEAR F\n END",
                               " BYTE X'F'\n END",
                               " RESW -1\n END"}) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(assemble(source)));
    }
}

TEST(AssemblerTest, EmptyImageIsRejectedButReservationsAndLastByteAreAllowed) {
    const auto empty = assemble(" START 0\n END\n");
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(empty));
    EXPECT_EQ(std::get<AssemblyFailure>(empty).diagnostics[0].code,
              AssemblyDiagnosticCode::EmptyProgram);
    const auto last = assemble(" START 0xFFFFF\nLAST BYTE X'FF'\n END LAST\n");
    ASSERT_TRUE(std::holds_alternative<AssemblySuccess>(last));
    EXPECT_EQ(std::get<AssemblySuccess>(last).bytes, (std::vector<std::uint8_t>{0xFF}));
    const auto full = assemble(" RESB 1048576\n END\n");
    ASSERT_TRUE(std::holds_alternative<AssemblySuccess>(full));
    EXPECT_EQ(std::get<AssemblySuccess>(full).bytes, std::vector<std::uint8_t>(1048576, 0));
    EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(assemble(" RESB 1048577\n END\n")));
}

TEST(AssemblerTest, SecondPassPreservesFirstPassAndRejectsAnInconsistentLayout) {
    auto first = PassOne::run(SourceParser::parse(" LDA #1\nBUF RESB 2\n END\n"));
    ASSERT_TRUE(first.succeeded());
    const auto originalLines = first.lines;
    const auto references = first.symbolTable.references();
    const auto symbols = first.symbols;
    EXPECT_TRUE(std::holds_alternative<AssemblySuccess>(PassTwo::run(first)));
    EXPECT_EQ(first.lines, originalLines);
    EXPECT_EQ(first.symbolTable.references(), references);
    EXPECT_EQ(first.symbols, symbols);
    first.programLength += 1;
    const auto mismatch = PassTwo::run(first);
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(mismatch));
    EXPECT_EQ(std::get<AssemblyFailure>(mismatch).diagnostics[0].code,
              AssemblyDiagnosticCode::LayoutMismatch);
    first = PassOne::run(SourceParser::parse(" BAD\n END\n"));
    EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(PassTwo::run(first)));
}

TEST(AssemblerTest, RawImageWriterRejectsGapsOverlapsMixedReservationAndMissingAddresses) {
    const std::vector<std::vector<AssemblyListingLine>> badListings{
        {{1, "", 1, {1}, 0}},
        {{1, "", 0, {1}, 0}, {2, "", 0, {2}, 0}},
        {{1, "", 0, {1}, 1}},
        {{1, "", std::nullopt, {1}, 0}},
        {{1, "", 0, {}, 3}},
    };
    for (const auto& listing : badListings)
        EXPECT_TRUE(
            std::holds_alternative<AssemblyDiagnostic>(RawImageWriter::build(0, 2, listing)));
    EXPECT_TRUE(std::holds_alternative<AssemblyDiagnostic>(RawImageWriter::build(0xFFFFF, 2, {})));
}

TEST(AssemblerTest, FormatsSourceDiagnosticWithoutReturningPartialListing) {
    const auto result = assemble(" LDA #1 extra\n END\n");
    ASSERT_TRUE(std::holds_alternative<AssemblyFailure>(result));
    const auto diagnostic =
        ListingWriter::diagnostics(std::get<AssemblyFailure>(result), "bad.asm");
    EXPECT_TRUE(diagnostic.starts_with("bad.asm:1:9: error: "));
    EXPECT_EQ(diagnostic.find("LOAD "), std::string::npos);
}

} // namespace
} // namespace assembler
