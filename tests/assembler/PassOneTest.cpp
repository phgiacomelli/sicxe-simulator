#include "assembler/PassOne.hpp"

#include "assembler/LocatedLine.hpp"
#include "assembler/SourceParser.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <variant>
#include <vector>

namespace {
using namespace assembler;

PassOneResult runSource(std::string_view source) {
    const auto parsed = SourceParser::parse(source);
    return PassOne::run(parsed);
}

TEST(PassOneTest, SequentialInstructionAddressesAndLabelsAcrossFormats234AndRsub) {
    const auto result = runSource("       START 0x1000\n"
                                  "FIRST  CLEAR A\n"
                                  "STEP1  LDA   #5\n"
                                  "STEP2  +JSUB CALLEE\n"
                                  "STEP3  ADDR  A,X\n"
                                  "CALLEE RSUB\n"
                                  "       END   FIRST\n");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.startAddress, 0x1000U);
    EXPECT_EQ(result.initialPc, 0x1000U);
    // 2 (CLEAR) + 3 (LDA) + 4 (+JSUB) + 2 (ADDR) + 3 (RSUB) = 14 bytes (0x0E)
    EXPECT_EQ(result.programLength, 14U);

    ASSERT_EQ(result.lines.size(), 7U);

    EXPECT_EQ(result.lines[0].category, LocatedLineCategory::Directive);
    EXPECT_EQ(result.lines[0].directive, LocatedDirectiveKind::Start);
    EXPECT_EQ(result.lines[0].address, 0x1000U);
    EXPECT_EQ(result.lines[0].sizeBytes, 0U);
    EXPECT_EQ(result.lines[0].listingAddress(), std::optional<std::uint32_t>{0x1000U});

    EXPECT_EQ(result.lines[1].category, LocatedLineCategory::Instruction);
    EXPECT_EQ(result.lines[1].format, LocatedInstructionFormat::Format2);
    EXPECT_EQ(result.lines[1].mnemonic, "CLEAR");
    EXPECT_EQ(result.lines[1].opcode, std::optional<std::uint8_t>{0xB4});
    EXPECT_EQ(result.lines[1].address, 0x1000U);
    EXPECT_EQ(result.lines[1].sizeBytes, 2U);

    EXPECT_EQ(result.lines[2].category, LocatedLineCategory::Instruction);
    EXPECT_EQ(result.lines[2].format, LocatedInstructionFormat::Format3);
    EXPECT_EQ(result.lines[2].mnemonic, "LDA");
    EXPECT_EQ(result.lines[2].opcode, std::optional<std::uint8_t>{0x00});
    EXPECT_EQ(result.lines[2].address, 0x1002U);
    EXPECT_EQ(result.lines[2].sizeBytes, 3U);

    EXPECT_EQ(result.lines[3].category, LocatedLineCategory::Instruction);
    EXPECT_EQ(result.lines[3].format, LocatedInstructionFormat::Format4);
    EXPECT_EQ(result.lines[3].mnemonic, "JSUB");
    EXPECT_TRUE(result.lines[3].extended);
    EXPECT_EQ(result.lines[3].opcode, std::optional<std::uint8_t>{0x48});
    EXPECT_EQ(result.lines[3].address, 0x1005U);
    EXPECT_EQ(result.lines[3].sizeBytes, 4U);

    EXPECT_EQ(result.lines[4].category, LocatedLineCategory::Instruction);
    EXPECT_EQ(result.lines[4].format, LocatedInstructionFormat::Format2);
    EXPECT_EQ(result.lines[4].mnemonic, "ADDR");
    EXPECT_EQ(result.lines[4].opcode, std::optional<std::uint8_t>{0x90});
    EXPECT_EQ(result.lines[4].address, 0x1009U);
    EXPECT_EQ(result.lines[4].sizeBytes, 2U);

    EXPECT_EQ(result.lines[5].category, LocatedLineCategory::Instruction);
    EXPECT_EQ(result.lines[5].format, LocatedInstructionFormat::Format3);
    EXPECT_EQ(result.lines[5].mnemonic, "RSUB");
    EXPECT_FALSE(result.lines[5].extended);
    EXPECT_EQ(result.lines[5].opcode, std::optional<std::uint8_t>{0x4C});
    EXPECT_EQ(result.lines[5].address, 0x100BU);
    EXPECT_EQ(result.lines[5].sizeBytes, 3U);

    EXPECT_EQ(result.lines[6].category, LocatedLineCategory::Directive);
    EXPECT_EQ(result.lines[6].directive, LocatedDirectiveKind::End);
    EXPECT_EQ(result.lines[6].address, 0x100EU);
    EXPECT_EQ(result.lines[6].sizeBytes, 0U);
    EXPECT_EQ(result.lines[6].listingAddress(), std::nullopt);

    const std::vector<AssemblySymbol> expectedSymbols = {
        {"FIRST", 0x1000U, SourcePosition{2, 1}},
        {"STEP1", 0x1002U, SourcePosition{3, 1}},
        {"STEP2", 0x1005U, SourcePosition{4, 1}},
        {"STEP3", 0x1009U, SourcePosition{5, 1}},
        {"CALLEE", 0x100BU, SourcePosition{6, 1}},
    };
    EXPECT_EQ(result.symbols, expectedSymbols);
    EXPECT_EQ(result.symbolTable.symbols(), expectedSymbols);
    EXPECT_FALSE(result.symbolTable.hasPendingReferences());
}

TEST(PassOneTest, ForwardReferencePreservesFixedInstructionSizeAndLayout) {
    const auto result = runSource("      START 0x2000\n"
                                  "      J     LATER\n"
                                  "      +JSUB FAR_TARGET\n"
                                  "      LDA   @LATER\n"
                                  "      LDCH  LATER,X\n"
                                  "LATER CLEAR X\n"
                                  "FAR_TARGET RSUB\n"
                                  "      END\n");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.startAddress, 0x2000U);
    EXPECT_EQ(result.initialPc, 0x2000U);
    // 3 + 4 + 3 + 3 + 2 + 3 = 18 bytes (0x12)
    EXPECT_EQ(result.programLength, 18U);

    EXPECT_EQ(result.lines[1].address, 0x2000U);
    EXPECT_EQ(result.lines[1].sizeBytes, 3U);
    EXPECT_EQ(result.lines[2].address, 0x2003U);
    EXPECT_EQ(result.lines[2].sizeBytes, 4U);
    EXPECT_EQ(result.lines[3].address, 0x2007U);
    EXPECT_EQ(result.lines[3].sizeBytes, 3U);
    EXPECT_EQ(result.lines[4].address, 0x200AU);
    EXPECT_EQ(result.lines[4].sizeBytes, 3U);
    EXPECT_EQ(result.lines[5].address, 0x200DU);
    EXPECT_EQ(result.lines[5].sizeBytes, 2U);
    EXPECT_EQ(result.lines[6].address, 0x200FU);
    EXPECT_EQ(result.lines[6].sizeBytes, 3U);

    ASSERT_TRUE(result.findSymbol("later").has_value());
    EXPECT_EQ(result.findSymbol("later")->address, 0x200DU);
    ASSERT_TRUE(result.findSymbol("far_target").has_value());
    EXPECT_EQ(result.findSymbol("far_target")->address, 0x200FU);

    const auto resolvedLater = result.symbolTable.resolve("LATER", {2, 13});
    ASSERT_TRUE(std::holds_alternative<std::uint32_t>(resolvedLater));
    EXPECT_EQ(std::get<std::uint32_t>(resolvedLater), 0x200DU);
}

TEST(PassOneTest, HandlesStartPresentAndAbsentStartLabelAndOptionalEndEntryPc) {
    {
        // START with program label and END with explicit entry label
        const auto withStart = runSource(". leading comment\n"
                                         "\n"
                                         "PROG  START 0x0500\n"
                                         "      CLEAR A\n"
                                         "ENTRY LDA   #1\n"
                                         "      RSUB\n"
                                         "      END   entry\n");
        ASSERT_TRUE(withStart.succeeded());
        EXPECT_EQ(withStart.startAddress, 0x0500U);
        EXPECT_EQ(withStart.initialPc, 0x0502U);
        EXPECT_EQ(withStart.programLength, 8U);

        ASSERT_TRUE(withStart.findSymbol("PROG").has_value());
        EXPECT_EQ(*withStart.findSymbol("PROG"),
                  (AssemblySymbol{"PROG", 0x0500U, SourcePosition{3, 1}}));
        ASSERT_TRUE(withStart.findSymbol("ENTRY").has_value());
        EXPECT_EQ(*withStart.findSymbol("ENTRY"),
                  (AssemblySymbol{"ENTRY", 0x0502U, SourcePosition{5, 1}}));
    }
    {
        // Without START and without END operand: both startAddress and initialPc default to 0
        const auto withoutStart = runSource("FIRST LDA #1\n"
                                            "      RSUB\n"
                                            "      END\n");
        ASSERT_TRUE(withoutStart.succeeded());
        EXPECT_EQ(withoutStart.startAddress, 0U);
        EXPECT_EQ(withoutStart.initialPc, 0U);
        EXPECT_EQ(withoutStart.programLength, 6U);
        ASSERT_EQ(withoutStart.symbols.size(), 1U);
        EXPECT_EQ(withoutStart.symbols.front(),
                  (AssemblySymbol{"FIRST", 0U, SourcePosition{1, 1}}));
    }
    {
        // Without START, but END specifies entry point after initial instruction
        const auto withoutStartWithEntry = runSource("      CLEAR A\n"
                                                     "BEGIN LDA   #10\n"
                                                     "      END   BEGIN\n");
        ASSERT_TRUE(withoutStartWithEntry.succeeded());
        EXPECT_EQ(withoutStartWithEntry.startAddress, 0U);
        EXPECT_EQ(withoutStartWithEntry.initialPc, 2U);
        EXPECT_EQ(withoutStartWithEntry.programLength, 5U);
    }
}

TEST(PassOneTest, KeepsBaseNoBaseAndCommentsInOrderWithoutIncrementingLocationCounter) {
    const auto result = runSource("      START  0x1000\n"
                                  "      LDB    #TABLE\n"
                                  "      BASE   TABLE\n"
                                  "      . comment between directives\n"
                                  "      LDA    TABLE\n"
                                  "      NOBASE\n"
                                  "TABLE RSUB\n"
                                  "      END\n");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 8U);

    EXPECT_EQ(result.lines[1].mnemonic, "LDB");
    EXPECT_EQ(result.lines[1].address, 0x1000U);
    EXPECT_EQ(result.lines[1].sizeBytes, 3U);

    EXPECT_EQ(result.lines[2].category, LocatedLineCategory::Directive);
    EXPECT_EQ(result.lines[2].directive, LocatedDirectiveKind::Base);
    EXPECT_EQ(result.lines[2].address, 0x1003U);
    EXPECT_EQ(result.lines[2].sizeBytes, 0U);
    EXPECT_EQ(result.lines[2].listingAddress(), std::nullopt);

    EXPECT_EQ(result.lines[3].category, LocatedLineCategory::Comment);
    EXPECT_EQ(result.lines[3].address, 0x1003U);
    EXPECT_EQ(result.lines[3].sizeBytes, 0U);
    EXPECT_EQ(result.lines[3].listingAddress(), std::nullopt);

    EXPECT_EQ(result.lines[4].mnemonic, "LDA");
    EXPECT_EQ(result.lines[4].address, 0x1003U);
    EXPECT_EQ(result.lines[4].sizeBytes, 3U);

    EXPECT_EQ(result.lines[5].category, LocatedLineCategory::Directive);
    EXPECT_EQ(result.lines[5].directive, LocatedDirectiveKind::NoBase);
    EXPECT_EQ(result.lines[5].address, 0x1006U);
    EXPECT_EQ(result.lines[5].sizeBytes, 0U);
    EXPECT_EQ(result.lines[5].listingAddress(), std::nullopt);

    EXPECT_EQ(result.lines[6].mnemonic, "RSUB");
    EXPECT_EQ(result.lines[6].address, 0x1006U);
    EXPECT_EQ(result.lines[6].sizeBytes, 3U);

    EXPECT_EQ(result.programLength, 9U);
    ASSERT_TRUE(result.findSymbol("TABLE").has_value());
    EXPECT_EQ(result.findSymbol("TABLE")->address, 0x1006U);
}

TEST(PassOneTest, ReportsDuplicateSymbolUndefinedSymbolAndReservedLabelName) {
    {
        const auto duplicate = runSource("LOOP  LDA  #1\n"
                                         "loop  STA  0x100\n"
                                         "      END\n");
        EXPECT_FALSE(duplicate.succeeded());
        ASSERT_EQ(duplicate.diagnostics.size(), 1U);
        EXPECT_EQ(duplicate.diagnostics[0].code, PassOneDiagnosticCode::DuplicateSymbol);
        EXPECT_EQ(duplicate.diagnostics[0].position, (SourcePosition{2, 1}));
        EXPECT_EQ(duplicate.diagnostics[0].previousDefinition, (SourcePosition{1, 1}));
        EXPECT_EQ(toAssemblyDiagnostic(duplicate.diagnostics[0]).code,
                  AssemblyDiagnosticCode::DuplicateSymbol);
        ASSERT_EQ(duplicate.symbols.size(), 1U);
        EXPECT_EQ(duplicate.symbols[0].address, 0U);
    }
    {
        const auto undefined = runSource("      START 0x100\n"
                                         "      LDA   MISSING_TARGET\n"
                                         "      BASE  MISSING_BASE\n"
                                         "      END   MISSING_ENTRY\n");
        EXPECT_FALSE(undefined.succeeded());
        ASSERT_EQ(undefined.diagnostics.size(), 3U);
        EXPECT_EQ(undefined.diagnostics[0].code, PassOneDiagnosticCode::UndefinedSymbol);
        EXPECT_EQ(undefined.diagnostics[0].position, (SourcePosition{2, 13}));
        EXPECT_EQ(toAssemblyDiagnostic(undefined.diagnostics[0]).code,
                  AssemblyDiagnosticCode::UndefinedSymbol);
        EXPECT_EQ(undefined.diagnostics[1].code, PassOneDiagnosticCode::UndefinedSymbol);
        EXPECT_EQ(undefined.diagnostics[1].position, (SourcePosition{3, 13}));
        EXPECT_EQ(undefined.diagnostics[2].code, PassOneDiagnosticCode::UndefinedSymbol);
        EXPECT_EQ(undefined.diagnostics[2].position, (SourcePosition{4, 13}));
    }
    {
        const auto reservedLabel = runSource("LDA   CLEAR A\n"
                                             "      END\n");
        EXPECT_FALSE(reservedLabel.succeeded());
        ASSERT_EQ(reservedLabel.diagnostics.size(), 1U);
        EXPECT_EQ(reservedLabel.diagnostics[0].code, PassOneDiagnosticCode::InvalidLabel);
        EXPECT_EQ(reservedLabel.diagnostics[0].position, (SourcePosition{1, 1}));
    }
}

TEST(PassOneTest, ReportsUnknownOperationInvalidExtendedFormatMissingEndAndStatementsAfterEnd) {
    {
        for (const std::string_view badOp : {
                 "  ADDF #1\n  END\n",
                 "  HALT\n  END\n",
                 "  EQU 10\n  END\n",
                 "  ORG 0x100\n  END\n",
             }) {
            SCOPED_TRACE(badOp);
            const auto unknown = runSource(badOp);
            EXPECT_FALSE(unknown.succeeded());
            ASSERT_EQ(unknown.diagnostics.size(), 1U);
            EXPECT_EQ(unknown.diagnostics[0].code, PassOneDiagnosticCode::UnknownOperation);
            EXPECT_EQ(unknown.diagnostics[0].position, (SourcePosition{1, 3}));
        }
    }
    {
        for (const std::string_view badExtended : {
                 "  +CLEAR A\n  END\n",
                 "  +ADDR A,X\n  END\n",
                 "  +RSUB\n  END\n",
                 "  +BASE 0x100\n  END\n",
             }) {
            SCOPED_TRACE(badExtended);
            const auto result = runSource(badExtended);
            EXPECT_FALSE(result.succeeded());
            ASSERT_EQ(result.diagnostics.size(), 1U);
            EXPECT_EQ(result.diagnostics[0].code,
                      PassOneDiagnosticCode::ExtendedFormatNotAllowed);
            EXPECT_EQ(result.diagnostics[0].position, (SourcePosition{1, 3}));
        }
    }
    {
        const auto missingEnd = runSource("  LDA #1\n  RSUB\n");
        EXPECT_FALSE(missingEnd.succeeded());
        ASSERT_EQ(missingEnd.diagnostics.size(), 1U);
        EXPECT_EQ(missingEnd.diagnostics[0].code, PassOneDiagnosticCode::MissingEnd);
        EXPECT_EQ(missingEnd.diagnostics[0].position, (SourcePosition{2, 1}));
    }
    {
        const auto afterEnd = runSource("  LDA #1\n"
                                        "  END\n"
                                        "  . comment after END is allowed\n"
                                        "\n"
                                        "  RSUB\n");
        EXPECT_FALSE(afterEnd.succeeded());
        ASSERT_EQ(afterEnd.diagnostics.size(), 1U);
        EXPECT_EQ(afterEnd.diagnostics[0].code, PassOneDiagnosticCode::StatementAfterEnd);
        EXPECT_EQ(afterEnd.diagnostics[0].position, (SourcePosition{5, 3}));
    }
    {
        const auto duplicateStartAndEnd = runSource("  START 0x100\n"
                                                    "  START 0x200\n"
                                                    "  RSUB\n"
                                                    "  END\n"
                                                    "  END\n");
        EXPECT_FALSE(duplicateStartAndEnd.succeeded());
        ASSERT_EQ(duplicateStartAndEnd.diagnostics.size(), 2U);
        EXPECT_EQ(duplicateStartAndEnd.diagnostics[0].code, PassOneDiagnosticCode::DuplicateStart);
        EXPECT_EQ(duplicateStartAndEnd.diagnostics[0].position, (SourcePosition{2, 3}));
        EXPECT_EQ(duplicateStartAndEnd.diagnostics[1].code, PassOneDiagnosticCode::DuplicateEnd);
        EXPECT_EQ(duplicateStartAndEnd.diagnostics[1].position, (SourcePosition{5, 3}));
    }
    {
        const auto misplacedStart = runSource("  LDA   #1\n"
                                              "  START 0x100\n"
                                              "  END\n");
        EXPECT_FALSE(misplacedStart.succeeded());
        ASSERT_EQ(misplacedStart.diagnostics.size(), 1U);
        EXPECT_EQ(misplacedStart.diagnostics[0].code, PassOneDiagnosticCode::MisplacedStart);
        EXPECT_EQ(misplacedStart.diagnostics[0].position, (SourcePosition{2, 3}));
    }
}

TEST(PassOneTest, ValidatesLastValidAddressNextInvalidAddressAndOverflowWithoutWrapping) {
    {
        // Format 3 instruction occupying 0xFFFFD, 0xFFFFE, 0xFFFFF (last valid byte in 1 MiB)
        const auto exactFit3 = runSource("      START 0xFFFFD\n"
                                         "LAST  RSUB\n"
                                         "      END   LAST\n");
        ASSERT_TRUE(exactFit3.succeeded());
        EXPECT_EQ(exactFit3.startAddress, 0xFFFFDU);
        EXPECT_EQ(exactFit3.initialPc, 0xFFFFDU);
        EXPECT_EQ(exactFit3.programLength, 3U);
        ASSERT_TRUE(exactFit3.findSymbol("LAST").has_value());
        EXPECT_EQ(exactFit3.findSymbol("LAST")->address, 0xFFFFDU);
    }
    {
        // Format 2 at 0xFFFFE..0xFFFFF and Format 4 at 0xFFFFC..0xFFFFF also fit exactly
        const auto exactFit2 = runSource("  START 0xFFFFE\n  CLEAR A\n  END\n");
        ASSERT_TRUE(exactFit2.succeeded());
        EXPECT_EQ(exactFit2.programLength, 2U);

        const auto exactFit4 = runSource("  START 0xFFFFC\n  +LDA #1\n  END\n");
        ASSERT_TRUE(exactFit4.succeeded());
        EXPECT_EQ(exactFit4.programLength, 4U);

        // Label on START at the very last valid byte 0xFFFFF with no instructions
        const auto startAtLastByte = runSource("LAST_BYTE START 0xFFFFF\n  END LAST_BYTE\n");
        ASSERT_TRUE(startAtLastByte.succeeded());
        EXPECT_EQ(startAtLastByte.startAddress, 0xFFFFFU);
        EXPECT_EQ(startAtLastByte.initialPc, 0xFFFFFU);
        EXPECT_EQ(startAtLastByte.programLength, 0U);
    }
    {
        // Next invalid address: START at 0x100000
        const auto invalidStart = runSource("  START 0x100000\n  END\n");
        EXPECT_FALSE(invalidStart.succeeded());
        ASSERT_EQ(invalidStart.diagnostics.size(), 1U);
        EXPECT_EQ(invalidStart.diagnostics[0].code, PassOneDiagnosticCode::InvalidStartOperand);
        EXPECT_EQ(invalidStart.diagnostics[0].position, (SourcePosition{1, 9}));
    }
    {
        // Next invalid address: instruction starting at 0x100000 after previous ended at 0xFFFFF
        const auto nextInvalid = runSource("      START 0xFFFFD\n"
                                           "      RSUB\n"
                                           "AFTER CLEAR A\n"
                                           "      END\n");
        EXPECT_FALSE(nextInvalid.succeeded());
        ASSERT_EQ(nextInvalid.diagnostics.size(), 1U);
        EXPECT_EQ(nextInvalid.diagnostics[0].code, PassOneDiagnosticCode::AddressOutOfRange);
        EXPECT_EQ(nextInvalid.diagnostics[0].position, (SourcePosition{3, 7}));
        EXPECT_FALSE(nextInvalid.findSymbol("AFTER").has_value());
    }
    {
        // Overflow: instruction starts at valid address 0xFFFFE, but 3 bytes cross past 0xFFFFF
        const auto overflow = runSource("      START 0xFFFFE\n"
                                        "      RSUB\n"
                                        "LATER CLEAR A\n"
                                        "      END\n");
        EXPECT_FALSE(overflow.succeeded());
        ASSERT_EQ(overflow.diagnostics.size(), 1U);
        EXPECT_EQ(overflow.diagnostics[0].code, PassOneDiagnosticCode::AddressOutOfRange);
        EXPECT_EQ(overflow.diagnostics[0].position, (SourcePosition{2, 7}));
        // Does not wrap around or locate subsequent instructions
        EXPECT_FALSE(overflow.findSymbol("LATER").has_value());
        EXPECT_EQ(overflow.programLength, 0U);
    }
}

TEST(PassOneTest, ExplicitlyRejectsDataAndReservationDirectivesBeforePackage5WithoutInventingSize) {
    for (const std::string_view directiveSource : {"      START 0x1000\n"
                                                   "DATA1 BYTE  C'EOF'\n"
                                                   "AFTER LDA   #1\n"
                                                   "      END\n",
                                                   "      START 0x1000\n"
                                                   "DATA1 WORD  123\n"
                                                   "AFTER LDA   #1\n"
                                                   "      END\n",
                                                   "      START 0x1000\n"
                                                   "DATA1 RESB  4\n"
                                                   "AFTER LDA   #1\n"
                                                   "      END\n",
                                                   "      START 0x1000\n"
                                                   "DATA1 RESW  2\n"
                                                   "AFTER LDA   #1\n"
                                                   "      END\n"}) {
        SCOPED_TRACE(directiveSource);
        const auto result = runSource(directiveSource);
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, PassOneDiagnosticCode::DataDirectiveNotIntegrated);
        EXPECT_EQ(result.diagnostics[0].position, (SourcePosition{2, 7}));
        // Must not invent a size or assign an address to AFTER
        EXPECT_FALSE(result.findSymbol("AFTER").has_value());
        EXPECT_EQ(result.programLength, 0U);
    }
}

TEST(PassOneTest, RefusesToAssembleWhenSourceParserReportedSyntaxFailure) {
    const auto parsed = SourceParser::parse("1BAD LDA #1\n  END\n");
    ASSERT_FALSE(parsed.succeeded());

    const auto result = PassOne::run(parsed);
    EXPECT_FALSE(result.succeeded());
    EXPECT_TRUE(result.lines.empty());
    EXPECT_TRUE(result.symbols.empty());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].code, PassOneDiagnosticCode::InvalidSourceLine);
    EXPECT_EQ(result.diagnostics[0].position, (SourcePosition{1, 1}));

    const auto directSpanResult = PassOne::run(parsed.lines);
    EXPECT_FALSE(directSpanResult.succeeded());
    EXPECT_TRUE(directSpanResult.lines.empty());
    ASSERT_EQ(directSpanResult.diagnostics.size(), 1U);
    EXPECT_EQ(directSpanResult.diagnostics[0].code, PassOneDiagnosticCode::InvalidSourceLine);
}

TEST(PassOneTest, RecognizesAllThirtyEightVmInstructionsWithExpectedFormatsAndSizes) {
    const auto result = runSource("      START  0x0100\n"
                                  "      ADDR   A,X\n"
                                  "      CLEAR  A\n"
                                  "      COMPR  S,T\n"
                                  "      DIVR   A,B\n"
                                  "      MULR   L,PC\n"
                                  "      RMO    SW,A\n"
                                  "      SHIFTL A,1\n"
                                  "      SHIFTR X,16\n"
                                  "      SUBR   T,S\n"
                                  "      TIXR   X\n"
                                  "      ADD    TARGET\n"
                                  "      AND    TARGET\n"
                                  "      COMP   TARGET\n"
                                  "      DIV    TARGET\n"
                                  "      J      TARGET\n"
                                  "      JEQ    TARGET\n"
                                  "      JGT    TARGET\n"
                                  "      JLT    TARGET\n"
                                  "      JSUB   TARGET\n"
                                  "      LDA    TARGET\n"
                                  "      LDB    TARGET\n"
                                  "      LDCH   TARGET,X\n"
                                  "      LDL    TARGET\n"
                                  "      LDS    TARGET\n"
                                  "      LDT    TARGET\n"
                                  "      LDX    TARGET\n"
                                  "      MUL    TARGET\n"
                                  "      OR     TARGET\n"
                                  "      STA    TARGET\n"
                                  "      STB    TARGET\n"
                                  "      STCH   TARGET,X\n"
                                  "      STL    TARGET\n"
                                  "      STS    TARGET\n"
                                  "      STT    TARGET\n"
                                  "      STX    TARGET\n"
                                  "      SUB    TARGET\n"
                                  "      TIX    TARGET\n"
                                  "TARGET RSUB\n"
                                  "      END\n");

    ASSERT_TRUE(result.succeeded());
    // 10 Format 2 (20 bytes) + 27 Format 3 (81 bytes) + 1 RSUB (3 bytes) = 104 bytes
    EXPECT_EQ(result.programLength, 104U);
    ASSERT_TRUE(result.findSymbol("TARGET").has_value());
    EXPECT_EQ(result.findSymbol("TARGET")->address, 0x0100U + 101U);
}

TEST(PassOneTest, ValidatesInstructionAndDirectiveOperandsAndKeepsInvocationsIndependent) {
    for (const std::string_view badInstructionOperand : {"  LDA\n  END\n",
                                                         "  RSUB 0x100\n  END\n",
                                                         "  CLEAR F\n  END\n",
                                                         "  SHIFTL A,0\n  END\n",
                                                         "  SHIFTR A,17\n  END\n",
                                                         "  ADDR A\n  END\n",
                                                         "  LDA #TABLE,X\nTABLE RSUB\n  END\n",
                                                         "  LDA @TABLE,X\nTABLE RSUB\n  END\n",
                                                         "  LDA TABLE,A\nTABLE RSUB\n  END\n",
                                                         "  LDA #-1\n  END\n",
                                                         "  LDA #0x100000\n  END\n",
                                                         "  LDA TABLE+1\nTABLE RSUB\n  END\n"}) {
        SCOPED_TRACE(badInstructionOperand);
        const auto res = runSource(badInstructionOperand);
        EXPECT_FALSE(res.succeeded());
        ASSERT_FALSE(res.diagnostics.empty());
        EXPECT_EQ(res.diagnostics[0].code, PassOneDiagnosticCode::InvalidInstructionOperand);
    }

    for (const std::string_view badDirectiveOperand : {"  BASE\n  END\n",
                                                       "  BASE -1\n  END\n",
                                                       "  BASE 0x100000\n  END\n",
                                                       "  NOBASE 0x100\n  END\n"}) {
        SCOPED_TRACE(badDirectiveOperand);
        const auto res = runSource(badDirectiveOperand);
        EXPECT_FALSE(res.succeeded());
        ASSERT_EQ(res.diagnostics.size(), 1U);
        EXPECT_EQ(res.diagnostics[0].code, PassOneDiagnosticCode::InvalidDirectiveOperand);
    }

    for (const std::string_view badEndOperand :
         {"  RSUB\n  END 0x1000\n", "  RSUB\n  END FIRST+1\n", "  RSUB\n  END LDA\n"}) {
        SCOPED_TRACE(badEndOperand);
        const auto res = runSource(badEndOperand);
        EXPECT_FALSE(res.succeeded());
        ASSERT_EQ(res.diagnostics.size(), 1U);
        EXPECT_EQ(res.diagnostics[0].code, PassOneDiagnosticCode::InvalidEndOperand);
    }

    const auto firstRun = runSource("ONLY_IN_FIRST RSUB\n  END ONLY_IN_FIRST\n");
    const auto secondRun = runSource("  J ONLY_IN_FIRST\n  END\n");
    EXPECT_TRUE(firstRun.succeeded());
    EXPECT_FALSE(secondRun.succeeded());
    ASSERT_EQ(secondRun.diagnostics.size(), 1U);
    EXPECT_EQ(secondRun.diagnostics[0].code, PassOneDiagnosticCode::UndefinedSymbol);
}

} // namespace
