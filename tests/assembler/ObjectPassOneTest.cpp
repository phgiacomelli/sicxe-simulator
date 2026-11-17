#include "assembler/ObjectPassOne.hpp"

#include "assembler/Assembler.hpp"
#include "assembler/LocatedLine.hpp"
#include "assembler/ObjectOperand.hpp"
#include "assembler/PassOne.hpp"
#include "assembler/SourceParser.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
using namespace assembler;

TEST(ObjectPassOneTest, ExportsDefinedBeforeAndAfterExtDefAndExternalsUsedBeforeAndAfterExtRef) {
    const auto result = ObjectPassOne::run("Copy   START  0x1000\n"
                                           "Early  WORD   10\n"
                                           "       +JSUB  PreRef\n"
                                           "       EXTDEF Early, Later\n"
                                           "       EXTREF preRef, PostRf\n"
                                           "Later  +LDA   #postrf\n"
                                           "       RSUB\n"
                                           "       END    early\n");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.section,
              (ObjectSectionHeader{"COPY", "Copy", SourcePosition{1, 1}, 0x1000U, 14U}));
    EXPECT_EQ(result.startAddress, 0x1000U);
    EXPECT_EQ(result.initialPc, 0x1000U);
    EXPECT_EQ(result.programLength, 14U);

    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(*result.entry,
              (ObjectEntryPoint{
                  "EARLY", "early", SourcePosition{8, 15}, 0x1000U, 0U, SourcePosition{2, 1}}));

    ASSERT_EQ(result.exports.size(), 2U);
    EXPECT_EQ(result.exports[0],
              (ExportedSymbol{
                  "EARLY", "Early", SourcePosition{4, 15}, 0x1000U, 0U, SourcePosition{2, 1}}));
    EXPECT_EQ(result.exports[1],
              (ExportedSymbol{
                  "LATER", "Later", SourcePosition{4, 22}, 0x1007U, 7U, SourcePosition{6, 1}}));
    EXPECT_TRUE(result.findExport("early").has_value());
    EXPECT_TRUE(result.findExport("LATER").has_value());

    ASSERT_EQ(result.externalReferences.size(), 2U);
    EXPECT_EQ(result.externalReferences[0],
              (ExternalSymbol{"PREREF", "preRef", SourcePosition{5, 15}}));
    EXPECT_EQ(result.externalReferences[1],
              (ExternalSymbol{"POSTRF", "PostRf", SourcePosition{5, 23}}));
    EXPECT_TRUE(result.findExternalReference("PREREF").has_value());
    EXPECT_TRUE(result.findExternalReference("postrf").has_value());

    // EXTREF symbols must never enter the local symbol table.
    EXPECT_FALSE(result.findSymbol("PREREF").has_value());
    EXPECT_FALSE(result.findSymbol("POSTRF").has_value());
    EXPECT_FALSE(result.symbolTable.hasPendingReferences());

    const std::vector<AssemblySymbol> expectedLocals = {
        {"COPY", 0x1000U, SourcePosition{1, 1}},
        {"EARLY", 0x1000U, SourcePosition{2, 1}},
        {"LATER", 0x1007U, SourcePosition{6, 1}},
    };
    EXPECT_EQ(result.symbols, expectedLocals);

    ASSERT_EQ(result.lines.size(), 8U);
    EXPECT_EQ(result.lines[1].offset, 0U);
    EXPECT_EQ(result.lines[2].offset, 3U);
    ASSERT_TRUE(result.lines[2].memoryOperand.has_value());
    EXPECT_EQ(result.lines[2].memoryOperand->mode, InstructionAddressing::Simple);
    EXPECT_FALSE(result.lines[2].memoryOperand->indexed);
    EXPECT_EQ(result.lines[2].memoryOperand->targetKind(), ObjectValueKind::ExternalSymbol);
    EXPECT_EQ(std::get<ExternalSymbolReference>(result.lines[2].memoryOperand->target),
              (ExternalSymbolReference{
                  "PREREF", "PreRef", "preRef", SourcePosition{3, 15}, SourcePosition{5, 15}}));

    EXPECT_EQ(result.lines[3].directive, ObjectDirectiveKind::ExtDef);
    EXPECT_EQ(result.lines[3].sizeBytes(), 0U);
    EXPECT_EQ(result.lines[3].exportedSymbols, result.exports);

    EXPECT_EQ(result.lines[4].directive, ObjectDirectiveKind::ExtRef);
    EXPECT_EQ(result.lines[4].sizeBytes(), 0U);
    EXPECT_EQ(result.lines[4].externalSymbols, result.externalReferences);

    EXPECT_EQ(result.lines[5].offset, 7U);
    ASSERT_TRUE(result.lines[5].memoryOperand.has_value());
    EXPECT_EQ(result.lines[5].memoryOperand->mode, InstructionAddressing::Immediate);
    EXPECT_EQ(result.lines[5].memoryOperand->targetKind(), ObjectValueKind::ExternalSymbol);
    EXPECT_EQ(std::get<ExternalSymbolReference>(result.lines[5].memoryOperand->target),
              (ExternalSymbolReference{
                  "POSTRF", "postrf", "PostRf", SourcePosition{6, 16}, SourcePosition{5, 23}}));
}

TEST(ObjectPassOneTest, NormalizesNonZeroStartToSectionOffsetsAndClassifiesAllOperandKinds) {
    const auto result = ObjectPassOne::run("SEC_01 START  0x2000\n"
                                           "       EXTDEF BUF, TOTAL\n"
                                           "       EXTREF EXT_A, EXT_B\n"
                                           "       LDB    #BUF\n"
                                           "       BASE   BUF\n"
                                           "       LDA    BUF\n"
                                           "       +LDCH  EXT_A,X\n"
                                           "       +J     @EXT_B\n"
                                           "       +STA   0x12345\n"
                                           "       +LDX   TOTAL\n"
                                           "       NOBASE\n"
                                           "TAG    BYTE   C'OK'\n"
                                           "BUF    RESB   10\n"
                                           "TOTAL  RESW   2\n"
                                           "       END\n");

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.startAddress, 0x2000U);
    EXPECT_EQ(result.initialPc, 0x2000U);
    EXPECT_FALSE(result.entry.has_value());
    // 3 (LDB) + 3 (LDA) + 4 (+LDCH) + 4 (+J) + 4 (+STA) + 4 (+LDX) + 2 (BYTE) + 10 (RESB) + 6
    // (RESW) = 40
    EXPECT_EQ(result.programLength, 40U);
    EXPECT_EQ(result.section.length, 40U);

    ASSERT_TRUE(result.findExport("BUF").has_value());
    EXPECT_EQ(result.findExport("BUF")->address, 0x2018U);
    EXPECT_EQ(result.findExport("BUF")->offset, 24U);
    ASSERT_TRUE(result.findExport("TOTAL").has_value());
    EXPECT_EQ(result.findExport("TOTAL")->address, 0x2022U);
    EXPECT_EQ(result.findExport("TOTAL")->offset, 34U);

    // LDB #BUF -> LocalSymbol at offset 24
    ASSERT_TRUE(result.lines[3].memoryOperand.has_value());
    EXPECT_EQ(result.lines[3].memoryOperand->mode, InstructionAddressing::Immediate);
    EXPECT_EQ(result.lines[3].memoryOperand->targetKind(), ObjectValueKind::LocalSymbol);
    EXPECT_EQ(std::get<LocalSymbolReference>(result.lines[3].memoryOperand->target).offset, 24U);

    // BASE BUF -> LocalSymbol at offset 24
    ASSERT_TRUE(result.lines[4].baseOperand.has_value());
    ASSERT_TRUE(std::holds_alternative<LocalSymbolReference>(*result.lines[4].baseOperand));
    EXPECT_EQ(std::get<LocalSymbolReference>(*result.lines[4].baseOperand).offset, 24U);

    // +LDCH EXT_A,X -> ExternalSymbol indexed
    ASSERT_TRUE(result.lines[6].memoryOperand.has_value());
    EXPECT_EQ(result.lines[6].memoryOperand->mode, InstructionAddressing::Simple);
    EXPECT_TRUE(result.lines[6].memoryOperand->indexed);
    EXPECT_EQ(result.lines[6].memoryOperand->targetKind(), ObjectValueKind::ExternalSymbol);

    // +J @EXT_B -> ExternalSymbol indirect
    ASSERT_TRUE(result.lines[7].memoryOperand.has_value());
    EXPECT_EQ(result.lines[7].memoryOperand->mode, InstructionAddressing::Indirect);
    EXPECT_FALSE(result.lines[7].memoryOperand->indexed);
    EXPECT_EQ(result.lines[7].memoryOperand->targetKind(), ObjectValueKind::ExternalSymbol);

    // +STA 0x12345 -> AbsoluteInteger
    ASSERT_TRUE(result.lines[8].memoryOperand.has_value());
    EXPECT_EQ(result.lines[8].memoryOperand->targetKind(), ObjectValueKind::AbsoluteInteger);
    EXPECT_EQ(std::get<IntegerOperand>(result.lines[8].memoryOperand->target).value, 0x12345);

    // +LDX TOTAL -> LocalSymbol in Format 4
    ASSERT_TRUE(result.lines[9].memoryOperand.has_value());
    EXPECT_EQ(result.lines[9].memoryOperand->targetKind(), ObjectValueKind::LocalSymbol);
    EXPECT_EQ(std::get<LocalSymbolReference>(result.lines[9].memoryOperand->target).offset, 34U);

    // BYTE C'OK' -> pre-decoded bytes {'O', 'K'}
    ASSERT_TRUE(result.lines[11].byteData.has_value());
    EXPECT_EQ(*result.lines[11].byteData, (std::vector<std::uint8_t>{'O', 'K'}));
}

TEST(ObjectPassOneTest, AllowsStrictlyLocalLabelsLongerThanSixCharacters) {
    const auto result = ObjectPassOne::run("MAIN   START  0\n"
                                           "       EXTDEF EXPORT\n"
                                           "VERY_LONG_LOCAL_LABEL CLEAR A\n"
                                           "EXPORT J      VERY_LONG_LOCAL_LABEL\n"
                                           "       END    VERY_LONG_LOCAL_LABEL\n");

    ASSERT_TRUE(result.succeeded());
    ASSERT_TRUE(result.findSymbol("VERY_LONG_LOCAL_LABEL").has_value());
    EXPECT_EQ(result.findSymbol("VERY_LONG_LOCAL_LABEL")->address, 0U);
    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(result.entry->name, "VERY_LONG_LOCAL_LABEL");
    EXPECT_EQ(result.entry->offset, 0U);
}

TEST(ObjectPassOneTest, RejectsMissingStartNamelessStartAndLongSectionOrGlobalNames) {
    {
        const auto missingStart = ObjectPassOne::run("FIRST RSUB\n      END FIRST\n");
        EXPECT_FALSE(missingStart.succeeded());
        ASSERT_EQ(missingStart.diagnostics.size(), 1U);
        EXPECT_EQ(missingStart.diagnostics[0].code, ObjectPassOneDiagnosticCode::MissingStart);
        EXPECT_EQ(missingStart.diagnostics[0].position, (SourcePosition{1, 7}));
    }
    {
        const auto namelessStart = ObjectPassOne::run("      START 0x100\n      RSUB\n      END\n");
        EXPECT_FALSE(namelessStart.succeeded());
        ASSERT_EQ(namelessStart.diagnostics.size(), 1U);
        EXPECT_EQ(namelessStart.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::MissingSectionName);
        EXPECT_EQ(namelessStart.diagnostics[0].position, (SourcePosition{1, 7}));
        EXPECT_EQ(toAssemblyDiagnostic(namelessStart.diagnostics[0]).code,
                  AssemblyDiagnosticCode::InvalidLabel);
    }
    {
        const auto longSection =
            ObjectPassOne::run("SECTION1 START 0\n         RSUB\n         END\n");
        EXPECT_FALSE(longSection.succeeded());
        ASSERT_EQ(longSection.diagnostics.size(), 1U);
        EXPECT_EQ(longSection.diagnostics[0].code, ObjectPassOneDiagnosticCode::SectionNameTooLong);
        EXPECT_EQ(longSection.diagnostics[0].position, (SourcePosition{1, 1}));
        EXPECT_EQ(toAssemblyDiagnostic(longSection.diagnostics[0]).code,
                  AssemblyDiagnosticCode::InvalidLabel);
    }
    {
        const auto longExtDef = ObjectPassOne::run("MAIN START 0\n"
                                                   "     EXTDEF TOOLONG\n"
                                                   "TOOLONG RSUB\n"
                                                   "     END\n");
        EXPECT_FALSE(longExtDef.succeeded());
        ASSERT_EQ(longExtDef.diagnostics.size(), 1U);
        EXPECT_EQ(longExtDef.diagnostics[0].code, ObjectPassOneDiagnosticCode::SymbolNameTooLong);
        EXPECT_EQ(longExtDef.diagnostics[0].position, (SourcePosition{2, 13}));
        EXPECT_EQ(toAssemblyDiagnostic(longExtDef.diagnostics[0]).code,
                  AssemblyDiagnosticCode::InvalidSymbolName);
    }
    {
        const auto longExtRef = ObjectPassOne::run("MAIN START 0\n"
                                                   "     EXTREF OK_SYM, GLOBAL7\n"
                                                   "     RSUB\n"
                                                   "     END\n");
        EXPECT_FALSE(longExtRef.succeeded());
        ASSERT_EQ(longExtRef.diagnostics.size(), 1U);
        EXPECT_EQ(longExtRef.diagnostics[0].code, ObjectPassOneDiagnosticCode::SymbolNameTooLong);
        EXPECT_EQ(longExtRef.diagnostics[0].position, (SourcePosition{2, 21}));
    }
}

TEST(ObjectPassOneTest, ReportsUndefinedExportsDuplicateDeclarationsAndDeclarationConflicts) {
    {
        const auto undefinedExport = ObjectPassOne::run("MAIN START 0\n"
                                                        "     EXTDEF DEF1, UNDEF1\n"
                                                        "DEF1 RSUB\n"
                                                        "     END\n");
        EXPECT_FALSE(undefinedExport.succeeded());
        ASSERT_EQ(undefinedExport.diagnostics.size(), 1U);
        EXPECT_EQ(undefinedExport.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::UndefinedExportedSymbol);
        EXPECT_EQ(undefinedExport.diagnostics[0].position, (SourcePosition{2, 19}));
        EXPECT_EQ(toAssemblyDiagnostic(undefinedExport.diagnostics[0]).code,
                  AssemblyDiagnosticCode::UndefinedSymbol);
    }
    {
        const auto duplicateExtDef = ObjectPassOne::run("MAIN START 0\n"
                                                        "     EXTDEF ALPHA, alpha\n"
                                                        "ALPHA RSUB\n"
                                                        "     END\n");
        EXPECT_FALSE(duplicateExtDef.succeeded());
        ASSERT_EQ(duplicateExtDef.diagnostics.size(), 1U);
        EXPECT_EQ(duplicateExtDef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::DuplicateExternalDeclaration);
        EXPECT_EQ(duplicateExtDef.diagnostics[0].position, (SourcePosition{2, 20}));
        EXPECT_EQ(duplicateExtDef.diagnostics[0].previousDefinition, (SourcePosition{2, 13}));
    }
    {
        const auto duplicateExtRef = ObjectPassOne::run("MAIN START 0\n"
                                                        "     EXTREF BETA\n"
                                                        "     EXTREF beta\n"
                                                        "     RSUB\n"
                                                        "     END\n");
        EXPECT_FALSE(duplicateExtRef.succeeded());
        ASSERT_EQ(duplicateExtRef.diagnostics.size(), 1U);
        EXPECT_EQ(duplicateExtRef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::DuplicateExternalDeclaration);
        EXPECT_EQ(duplicateExtRef.diagnostics[0].position, (SourcePosition{3, 13}));
        EXPECT_EQ(duplicateExtRef.diagnostics[0].previousDefinition, (SourcePosition{2, 13}));
    }
    {
        const auto extDefThenExtRef = ObjectPassOne::run("MAIN START 0\n"
                                                         "     EXTDEF GAMMA\n"
                                                         "     EXTREF gamma\n"
                                                         "     RSUB\n"
                                                         "     END\n");
        EXPECT_FALSE(extDefThenExtRef.succeeded());
        ASSERT_EQ(extDefThenExtRef.diagnostics.size(), 1U);
        EXPECT_EQ(extDefThenExtRef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::ExternalDeclarationConflict);
        EXPECT_EQ(extDefThenExtRef.diagnostics[0].position, (SourcePosition{3, 13}));
        EXPECT_EQ(extDefThenExtRef.diagnostics[0].previousDefinition, (SourcePosition{2, 13}));
    }
    {
        const auto extRefThenExtDef = ObjectPassOne::run("MAIN START 0\n"
                                                         "     EXTREF DELTA\n"
                                                         "     EXTDEF delta\n"
                                                         "     RSUB\n"
                                                         "     END\n");
        EXPECT_FALSE(extRefThenExtDef.succeeded());
        ASSERT_EQ(extRefThenExtDef.diagnostics.size(), 1U);
        EXPECT_EQ(extRefThenExtDef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::ExternalDeclarationConflict);
        EXPECT_EQ(extRefThenExtDef.diagnostics[0].position, (SourcePosition{3, 13}));
        EXPECT_EQ(extRefThenExtDef.diagnostics[0].previousDefinition, (SourcePosition{2, 13}));
    }
    {
        const auto localBeforeExtRef = ObjectPassOne::run("MAIN  START 0\n"
                                                          "LOCAL WORD  1\n"
                                                          "      EXTREF local\n"
                                                          "      END\n");
        EXPECT_FALSE(localBeforeExtRef.succeeded());
        ASSERT_EQ(localBeforeExtRef.diagnostics.size(), 1U);
        EXPECT_EQ(localBeforeExtRef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::LocalExternalConflict);
        EXPECT_EQ(localBeforeExtRef.diagnostics[0].position, (SourcePosition{3, 14}));
        EXPECT_EQ(localBeforeExtRef.diagnostics[0].previousDefinition, (SourcePosition{2, 1}));
    }
    {
        const auto extRefBeforeLocal = ObjectPassOne::run("MAIN  START 0\n"
                                                          "      EXTREF EXT1\n"
                                                          "ext1  WORD  1\n"
                                                          "      END\n");
        EXPECT_FALSE(extRefBeforeLocal.succeeded());
        ASSERT_EQ(extRefBeforeLocal.diagnostics.size(), 1U);
        EXPECT_EQ(extRefBeforeLocal.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::LocalExternalConflict);
        EXPECT_EQ(extRefBeforeLocal.diagnostics[0].position, (SourcePosition{3, 1}));
        EXPECT_EQ(extRefBeforeLocal.diagnostics[0].previousDefinition, (SourcePosition{2, 14}));
    }
    {
        const auto sectionConflictInExtDef = ObjectPassOne::run("MAIN START 0\n"
                                                                "     EXTDEF main\n"
                                                                "     RSUB\n"
                                                                "     END\n");
        EXPECT_FALSE(sectionConflictInExtDef.succeeded());
        ASSERT_EQ(sectionConflictInExtDef.diagnostics.size(), 1U);
        EXPECT_EQ(sectionConflictInExtDef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::ExternalDeclarationConflict);
        EXPECT_EQ(sectionConflictInExtDef.diagnostics[0].position, (SourcePosition{2, 13}));
        EXPECT_EQ(sectionConflictInExtDef.diagnostics[0].previousDefinition,
                  (SourcePosition{1, 1}));
    }
    {
        const auto sectionConflictInExtRef = ObjectPassOne::run("MAIN START 0\n"
                                                                "     EXTREF main\n"
                                                                "     RSUB\n"
                                                                "     END\n");
        EXPECT_FALSE(sectionConflictInExtRef.succeeded());
        ASSERT_EQ(sectionConflictInExtRef.diagnostics.size(), 1U);
        EXPECT_EQ(sectionConflictInExtRef.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::LocalExternalConflict);
        EXPECT_EQ(sectionConflictInExtRef.diagnostics[0].position, (SourcePosition{2, 13}));
        EXPECT_EQ(sectionConflictInExtRef.diagnostics[0].previousDefinition,
                  (SourcePosition{1, 1}));
    }
}

TEST(ObjectPassOneTest, RejectsExternalSymbolInFormat3BaseAndEndBothBeforeAndAfterExtRef) {
    for (const std::string_view format3WithExt : {
             "MAIN START 0\n     EXTREF EXT\n     LDA EXT\n     END\n",
             "MAIN START 0\n     LDA EXT\n     EXTREF EXT\n     END\n",
             "MAIN START 0\n     EXTREF EXT\n     LDA #EXT\n     END\n",
             "MAIN START 0\n     EXTREF EXT\n     J @EXT\n     END\n",
             "MAIN START 0\n     EXTREF EXT\n     LDCH EXT,X\n     END\n",
         }) {
        SCOPED_TRACE(format3WithExt);
        const auto result = ObjectPassOne::run(format3WithExt);
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectPassOneDiagnosticCode::ExternalInFormat3);
        EXPECT_EQ(toAssemblyDiagnostic(result.diagnostics[0]).code,
                  AssemblyDiagnosticCode::SymbolNotAllowed);
    }

    for (const std::string_view baseWithExt : {
             "MAIN START 0\n     EXTREF EXT\n     BASE EXT\n     RSUB\n     END\n",
             "MAIN START 0\n     BASE EXT\n     EXTREF EXT\n     RSUB\n     END\n",
         }) {
        SCOPED_TRACE(baseWithExt);
        const auto result = ObjectPassOne::run(baseWithExt);
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectPassOneDiagnosticCode::ExternalInBase);
        EXPECT_EQ(toAssemblyDiagnostic(result.diagnostics[0]).code,
                  AssemblyDiagnosticCode::SymbolNotAllowed);
    }

    const auto endWithExt = ObjectPassOne::run("MAIN START 0\n"
                                               "     EXTREF EXT\n"
                                               "     RSUB\n"
                                               "     END EXT\n");
    EXPECT_FALSE(endWithExt.succeeded());
    ASSERT_EQ(endWithExt.diagnostics.size(), 1U);
    EXPECT_EQ(endWithExt.diagnostics[0].code, ObjectPassOneDiagnosticCode::ExternalInEnd);
    EXPECT_EQ(endWithExt.diagnostics[0].position, (SourcePosition{4, 10}));
    EXPECT_EQ(toAssemblyDiagnostic(endWithExt.diagnostics[0]).code,
              AssemblyDiagnosticCode::SymbolNotAllowed);
}

TEST(ObjectPassOneTest, ParsesSimpleAndMixedLinearWordExpressionsAndClassifiesTerms) {
    const auto result = ObjectPassOne::run("MAIN   START  0x1000\n"
                                           "       EXTREF EXT1, EXT2\n"
                                           "W0     WORD   42\n"
                                           "W1     WORD   -0x800000\n"
                                           "W2     WORD   +W0\n"
                                           "W3     WORD   -EXT1\n"
                                           "W4     WORD   W3-W0\n"
                                           "W5     WORD   W2+EXT1-EXT2-4+0x10\n"
                                           "       END    MAIN\n");

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.lines.size(), 9U);

    // W0: WORD 42 -> +42
    ASSERT_TRUE(result.lines[2].wordExpression.has_value());
    EXPECT_EQ(result.lines[2].wordExpression->terms.size(), 1U);
    EXPECT_EQ(result.lines[2].wordExpression->terms[0].kind(), ObjectValueKind::AbsoluteInteger);
    EXPECT_EQ(result.lines[2].wordExpression->evaluateInitialAddend(), 42);
    EXPECT_EQ(result.lines[2].wordExpression->localRelocationBalance(), 0);

    // W1: WORD -0x800000 -> -8388608
    ASSERT_TRUE(result.lines[3].wordExpression.has_value());
    EXPECT_EQ(result.lines[3].wordExpression->evaluateInitialAddend(), -0x800000);
    EXPECT_EQ(result.lines[3].wordExpression->localRelocationBalance(), 0);

    // W2: WORD +W0 -> local offset 0, relocation balance +1
    ASSERT_TRUE(result.lines[4].wordExpression.has_value());
    EXPECT_EQ(result.lines[4].wordExpression->terms.size(), 1U);
    EXPECT_EQ(result.lines[4].wordExpression->terms[0].sign, ObjectTermSign::Add);
    EXPECT_EQ(result.lines[4].wordExpression->terms[0].kind(), ObjectValueKind::LocalSymbol);
    EXPECT_EQ(result.lines[4].wordExpression->evaluateInitialAddend(), 0);
    EXPECT_EQ(result.lines[4].wordExpression->localRelocationBalance(), 1);

    // W3: WORD -EXT1 -> external, initial addend 0, local balance 0
    ASSERT_TRUE(result.lines[5].wordExpression.has_value());
    EXPECT_EQ(result.lines[5].wordExpression->terms.size(), 1U);
    EXPECT_EQ(result.lines[5].wordExpression->terms[0].sign, ObjectTermSign::Subtract);
    EXPECT_EQ(result.lines[5].wordExpression->terms[0].kind(), ObjectValueKind::ExternalSymbol);
    EXPECT_EQ(result.lines[5].wordExpression->evaluateInitialAddend(), 0);
    EXPECT_EQ(result.lines[5].wordExpression->localRelocationBalance(), 0);

    // W4: WORD W3-W0 -> (9 - 0) = 9, local relocation balance 0 (cancels!)
    ASSERT_TRUE(result.lines[6].wordExpression.has_value());
    EXPECT_EQ(result.lines[6].wordExpression->terms.size(), 2U);
    EXPECT_EQ(result.lines[6].wordExpression->evaluateInitialAddend(), 9);
    EXPECT_EQ(result.lines[6].wordExpression->localRelocationBalance(), 0);

    // W5: WORD W2+EXT1-EXT2-4+0x10 -> 6 + 0 - 0 - 4 + 16 = 18, local balance +1
    ASSERT_TRUE(result.lines[7].wordExpression.has_value());
    ASSERT_EQ(result.lines[7].wordExpression->terms.size(), 5U);
    EXPECT_EQ(result.lines[7].wordExpression->terms[0].kind(), ObjectValueKind::LocalSymbol);
    EXPECT_EQ(result.lines[7].wordExpression->terms[1].kind(), ObjectValueKind::ExternalSymbol);
    EXPECT_EQ(result.lines[7].wordExpression->terms[1].sign, ObjectTermSign::Add);
    EXPECT_EQ(result.lines[7].wordExpression->terms[2].kind(), ObjectValueKind::ExternalSymbol);
    EXPECT_EQ(result.lines[7].wordExpression->terms[2].sign, ObjectTermSign::Subtract);
    EXPECT_EQ(result.lines[7].wordExpression->terms[3].kind(), ObjectValueKind::AbsoluteInteger);
    EXPECT_EQ(result.lines[7].wordExpression->terms[3].sign, ObjectTermSign::Subtract);
    EXPECT_EQ(result.lines[7].wordExpression->terms[4].kind(), ObjectValueKind::AbsoluteInteger);
    EXPECT_EQ(result.lines[7].wordExpression->terms[4].sign, ObjectTermSign::Add);
    EXPECT_EQ(result.lines[7].wordExpression->evaluateInitialAddend(), 18);
    EXPECT_EQ(result.lines[7].wordExpression->localRelocationBalance(), 1);
}

TEST(ObjectPassOneTest,
     RejectsInvalidWordExpressionsDanglingSignsUnsupportedOperatorsAndUndeclaredRefs) {
    for (const std::string_view badWordOperand : {
             "",
             "+",
             "-",
             "A+",
             "A-",
             "10+",
             "10-",
             "++A",
             "+-A",
             "-+A",
             "--A",
             "A++B",
             "A+-B",
             "A-+B",
             "A--B",
             "1+-2",
             "2*A",
             "A/2",
             "(A+B)",
             "L(A)",
             "=5",
             "0x",
             "A+0x",
             "0x1G",
             "A+12abc",
             "_BAD",
             "A+_BAD",
             "A,B",
             "#A",
             "@A",
             "LDA",
             "A+START",
             "EXTDEF",
             "A-EXTREF",
             "0x1000000",
             "-0x800001",
             "A+0x1000000",
             "9223372036854775808",
         }) {
        SCOPED_TRACE(badWordOperand);
        const auto parsed = parseObjectWordExpression(badWordOperand, SourcePosition{2, 10});
        EXPECT_TRUE(std::holds_alternative<OperandDiagnostic>(parsed));

        const std::string fullSource =
            "MAIN START 0\nA WORD " + std::string{badWordOperand} + "\nB RSUB\n END\n";
        const auto pass = ObjectPassOne::run(fullSource);
        EXPECT_FALSE(pass.succeeded());
        ASSERT_FALSE(pass.diagnostics.empty());
        EXPECT_EQ(pass.diagnostics.front().code,
                  ObjectPassOneDiagnosticCode::InvalidDirectiveOperand);
    }

    const auto undeclaredInWord = ObjectPassOne::run("MAIN  START  0\n"
                                                     "      EXTREF EXT1\n"
                                                     "LOCAL WORD   1\n"
                                                     "EXPR  WORD   LOCAL+MISSING1-EXT1+MISSING2\n"
                                                     "      END    MAIN\n");
    EXPECT_FALSE(undeclaredInWord.succeeded());
    ASSERT_EQ(undeclaredInWord.diagnostics.size(), 2U);
    EXPECT_EQ(undeclaredInWord.diagnostics[0].code, ObjectPassOneDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(undeclaredInWord.diagnostics[0].position, (SourcePosition{4, 20}));
    EXPECT_EQ(undeclaredInWord.diagnostics[1].code, ObjectPassOneDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(undeclaredInWord.diagnostics[1].position, (SourcePosition{4, 34}));
}

TEST(ObjectPassOneTest, RejectsReservedNamesAndUnsupportedDirectivesInObjectMode) {
    for (const std::string_view unsupportedDirective : {
             "MAIN START 0\n     INTDEF A\nA    RSUB\n     END\n",
             "MAIN START 0\n     INTUSE A\n     RSUB\n     END\n",
             "MAIN START 0\n     CSECT\n     RSUB\n     END\n",
             "MAIN START 0\nA    EQU 1\n     RSUB\n     END\n",
             "MAIN START 0\n     ORG 0x100\n     RSUB\n     END\n",
         }) {
        SCOPED_TRACE(unsupportedDirective);
        const auto result = ObjectPassOne::run(unsupportedDirective);
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectPassOneDiagnosticCode::UnknownOperation);
    }

    for (const std::string_view badExtList : {
             "MAIN START 0\n     EXTDEF\n     RSUB\n     END\n",
             "MAIN START 0\n     EXTREF\n     RSUB\n     END\n",
             "MAIN START 0\n     EXTDEF ,A\nA    RSUB\n     END\n",
             "MAIN START 0\n     EXTDEF A,\nA    RSUB\n     END\n",
             "MAIN START 0\n     EXTREF A,,B\n     RSUB\n     END\n",
             "MAIN START 0\n     EXTDEF LDA\n     RSUB\n     END\n",
             "MAIN START 0\n     EXTREF extdef\n     RSUB\n     END\n",
             "MAIN START 0\n     EXTDEF 123\n     RSUB\n     END\n",
             "MAIN START 0\n+EXTDEF A\nA RSUB\n END\n",
             "MAIN START 0\nLBL EXTDEF A\nA RSUB\n END\n",
         }) {
        SCOPED_TRACE(badExtList);
        const auto result = ObjectPassOne::run(badExtList);
        EXPECT_FALSE(result.succeeded());
        ASSERT_FALSE(result.diagnostics.empty());
    }

    const auto reservedSection = ObjectPassOne::run("EXTDEF START 0\n       RSUB\n       END\n");
    EXPECT_FALSE(reservedSection.succeeded());
    ASSERT_EQ(reservedSection.diagnostics.size(), 1U);
    EXPECT_EQ(reservedSection.diagnostics[0].code, ObjectPassOneDiagnosticCode::InvalidSectionName);
}

TEST(ObjectPassOneTest, AbsoluteModeRegressionRejectsExtDefExtRefAndWordExpressions) {
    for (const std::string_view rejectedInAbsolute : {
             "MAIN START 0\n     EXTDEF A\nA    RSUB\n     END MAIN\n",
             "MAIN START 0\n     EXTREF B\n     +JSUB B\n     END MAIN\n",
             "MAIN START 0\nA    WORD 1\nB    WORD A+1\n     END MAIN\n",
             "MAIN START 0\nA    WORD 1\nB    WORD -A\n     END MAIN\n",
         }) {
        SCOPED_TRACE(rejectedInAbsolute);
        const auto passOne = PassOne::run(SourceParser::parse(rejectedInAbsolute));
        EXPECT_FALSE(passOne.succeeded());
        const auto assembled = assemble(rejectedInAbsolute);
        EXPECT_TRUE(std::holds_alternative<AssemblyFailure>(assembled));
    }

    const std::string_view validAbsoluteSource = "PROG   START  0x0100\n"
                                                 "FIRST  CLEAR  A\n"
                                                 "       LDA    VALUE\n"
                                                 "       +JSUB  CALLEE\n"
                                                 "CALLEE RSUB\n"
                                                 "VALUE  WORD   37\n"
                                                 "TEXT   BYTE   C'SIC'\n"
                                                 "BUFFER RESW   2\n"
                                                 "       END    FIRST\n";
    const auto absoluteResult = assemble(validAbsoluteSource);
    const auto* success = std::get_if<AssemblySuccess>(&absoluteResult);
    ASSERT_NE(success, nullptr);
    EXPECT_EQ(success->loadAddress, 0x0100U);
    EXPECT_EQ(success->initialPc, 0x0100U);
    // 2 + 3 + 4 + 3 + 3 + 3 + 6 = 24 bytes
    EXPECT_EQ(success->bytes.size(), 24U);
}

TEST(ObjectPassOneTest, EnforcesSingleSectionPerSourceAndValidatesStatementPlacementAndLimits) {
    {
        const auto extBeforeStart = ObjectPassOne::run("     EXTDEF A\n"
                                                       "MAIN START  0\n"
                                                       "A    RSUB\n"
                                                       "     END\n");
        EXPECT_FALSE(extBeforeStart.succeeded());
        ASSERT_EQ(extBeforeStart.diagnostics.size(), 1U);
        EXPECT_EQ(extBeforeStart.diagnostics[0].code, ObjectPassOneDiagnosticCode::MisplacedStart);
        EXPECT_EQ(extBeforeStart.diagnostics[0].position, (SourcePosition{2, 6}));
    }
    {
        const auto secondSection = ObjectPassOne::run("SEC1 START 0\n"
                                                      "     RSUB\n"
                                                      "SEC2 START 0x100\n"
                                                      "     END\n");
        EXPECT_FALSE(secondSection.succeeded());
        ASSERT_EQ(secondSection.diagnostics.size(), 1U);
        EXPECT_EQ(secondSection.diagnostics[0].code, ObjectPassOneDiagnosticCode::DuplicateStart);
        EXPECT_EQ(secondSection.diagnostics[0].position, (SourcePosition{3, 6}));
    }
    {
        const auto exportDefinedTwice = ObjectPassOne::run("MAIN START  0\n"
                                                           "     EXTDEF DUP\n"
                                                           "DUP  WORD   1\n"
                                                           "dup  WORD   2\n"
                                                           "     END\n");
        EXPECT_FALSE(exportDefinedTwice.succeeded());
        ASSERT_EQ(exportDefinedTwice.diagnostics.size(), 1U);
        EXPECT_EQ(exportDefinedTwice.diagnostics[0].code,
                  ObjectPassOneDiagnosticCode::DuplicateSymbol);
        EXPECT_EQ(exportDefinedTwice.diagnostics[0].position, (SourcePosition{4, 1}));
        EXPECT_EQ(exportDefinedTwice.diagnostics[0].previousDefinition, (SourcePosition{3, 1}));
    }
    {
        const auto exactFit = ObjectPassOne::run("LAST_S START 0xFFFFA\n"
                                                 "       EXTDEF W_END\n"
                                                 "       RSUB\n"
                                                 "W_END  WORD  -1\n"
                                                 "       END   LAST_S\n");
        ASSERT_TRUE(exactFit.succeeded());
        EXPECT_EQ(exactFit.startAddress, 0xFFFFAU);
        EXPECT_EQ(exactFit.programLength, 6U);
        ASSERT_TRUE(exactFit.findExport("W_END").has_value());
        EXPECT_EQ(exactFit.findExport("W_END")->address, 0xFFFFDU);
        EXPECT_EQ(exactFit.findExport("W_END")->offset, 3U);
    }
    {
        const auto overflow = ObjectPassOne::run("MAIN  START 0xFFFFE\n"
                                                 "      WORD  1\n"
                                                 "LATER RSUB\n"
                                                 "      END\n");
        EXPECT_FALSE(overflow.succeeded());
        ASSERT_EQ(overflow.diagnostics.size(), 1U);
        EXPECT_EQ(overflow.diagnostics[0].code, ObjectPassOneDiagnosticCode::AddressOutOfRange);
        EXPECT_FALSE(overflow.findSymbol("LATER").has_value());
    }
}

TEST(ObjectPassOneTest, OwnsAllStringsAndKeepsSuccessiveRunsIndependent) {
    ObjectPassOneResult first;
    {
        std::string mutableSource = "ModA   START  0x0200\n"
                                    "       EXTDEF Val_1\n"
                                    "       EXTREF Ext_1\n"
                                    "Val_1  WORD   Ext_1+4\n"
                                    "       +JSUB  Ext_1\n"
                                    "       END    ModA\n";
        first = ObjectPassOne::run(mutableSource);
        mutableSource.assign(256, 'X');
    }
    ASSERT_TRUE(first.succeeded());
    EXPECT_EQ(first.section.name, "MODA");
    EXPECT_EQ(first.section.source, "ModA");
    ASSERT_EQ(first.exports.size(), 1U);
    EXPECT_EQ(first.exports[0].source, "Val_1");
    ASSERT_EQ(first.externalReferences.size(), 1U);
    EXPECT_EQ(first.externalReferences[0].source, "Ext_1");

    // Second run does not inherit Ext_1 or Val_1 from first run.
    const auto second = ObjectPassOne::run("ModB START 0\n"
                                           "     +JSUB Ext_1\n"
                                           "     WORD  Val_1\n"
                                           "     END\n");
    EXPECT_FALSE(second.succeeded());
    ASSERT_EQ(second.diagnostics.size(), 2U);
    EXPECT_EQ(second.diagnostics[0].code, ObjectPassOneDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(second.diagnostics[1].code, ObjectPassOneDiagnosticCode::UndefinedSymbol);
}

} // namespace
