#include "linker/LinkPassOne.hpp"

#include "linker/ObjectRecords.hpp"

#include <array>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace linker {
namespace {

ObjectModule parseValidModule(std::string_view text, std::string_view fileName) {
    const auto parsed = parseObject(text, fileName);
    EXPECT_TRUE(std::holds_alternative<ObjectModule>(parsed));
    return std::get<ObjectModule>(parsed);
}

TEST(LinkPassOneTest, PlacesTwoAndThreeSectionsInInputOrderAndResolvesSymbolsAcrossHoles) {
    const auto mainMod = parseValidModule("HMain  000000000030\n"
                                          "DEarly 000003InHole000010After 000020\n"
                                          "RHelperLast  \n"
                                          "T000000064B1000000000\n"
                                          "T00002003000001\n"
                                          "M00000105+Helper\n"
                                          "M00002006+Last  \n"
                                          "E\n",
                                          "main.obj");

    const auto subMod = parseValidModule("HSubSec000000000025\n"
                                         "DHelper000005\n"
                                         "REarly \n"
                                         "T000005034F0000\n"
                                         "E\n",
                                         "sub.obj");

    const auto tailMod = parseValidModule("HTail  000000000010\n"
                                          "DLast  00000F\n"
                                          "T00000003000007\n"
                                          "E\n",
                                          "tail.obj");

    const std::array<ObjectModule, 3> modules = {mainMod, subMod, tailMod};
    const auto result = LinkPassOne::run(modules);

    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(result.totalLength, 0x65U);
    EXPECT_FALSE(result.hasExplicitEntry());
    EXPECT_FALSE(result.explicitEntry.has_value());
    EXPECT_EQ(result.entryOffset, 0U);
    EXPECT_EQ(result.entry,
              (LinkedEntryPoint{0, "Main", 0U, 0U, 0U, false, ObjectPosition{"main.obj", 8, 1}}));

    ASSERT_EQ(result.sections.size(), 3U);
    EXPECT_EQ(result.sections[0],
              (SectionLayout{"Main", "MAIN", 0, 0x00U, 0x30U, {"main.obj", 1, 1}}));
    EXPECT_EQ(result.sections[1],
              (SectionLayout{"SubSec", "SUBSEC", 1, 0x30U, 0x25U, {"sub.obj", 1, 1}}));
    EXPECT_EQ(result.sections[2],
              (SectionLayout{"Tail", "TAIL", 2, 0x55U, 0x10U, {"tail.obj", 1, 1}}));
    EXPECT_EQ(result.sections[0].endOffset(), 0x30U);
    EXPECT_EQ(result.sections[1].endOffset(), 0x55U);
    EXPECT_EQ(result.sections[2].endOffset(), 0x65U);
    EXPECT_TRUE(result.sections[1].containsOffset(0x24U));
    EXPECT_FALSE(result.sections[1].containsOffset(0x25U));
    EXPECT_EQ(result.sections[1].toGlobalOffset(0x05U), 0x35U);

    const std::vector<GlobalSymbol> expectedSymbols = {
        {"Main",
         "MAIN",
         GlobalSymbolKind::Section,
         0,
         "Main",
         0x00U,
         0x00U,
         0x00U,
         {"main.obj", 1, 1}},
        {"Early",
         "EARLY",
         GlobalSymbolKind::DefinedSymbol,
         0,
         "Main",
         0x00U,
         0x03U,
         0x03U,
         {"main.obj", 2, 2}},
        {"InHole",
         "INHOLE",
         GlobalSymbolKind::DefinedSymbol,
         0,
         "Main",
         0x00U,
         0x10U,
         0x10U,
         {"main.obj", 2, 14}},
        {"After",
         "AFTER",
         GlobalSymbolKind::DefinedSymbol,
         0,
         "Main",
         0x00U,
         0x20U,
         0x20U,
         {"main.obj", 2, 26}},
        {"SubSec",
         "SUBSEC",
         GlobalSymbolKind::Section,
         1,
         "SubSec",
         0x30U,
         0x00U,
         0x30U,
         {"sub.obj", 1, 1}},
        {"Helper",
         "HELPER",
         GlobalSymbolKind::DefinedSymbol,
         1,
         "SubSec",
         0x30U,
         0x05U,
         0x35U,
         {"sub.obj", 2, 2}},
        {"Tail",
         "TAIL",
         GlobalSymbolKind::Section,
         2,
         "Tail",
         0x55U,
         0x00U,
         0x55U,
         {"tail.obj", 1, 1}},
        {"Last",
         "LAST",
         GlobalSymbolKind::DefinedSymbol,
         2,
         "Tail",
         0x55U,
         0x0FU,
         0x64U,
         {"tail.obj", 2, 2}},
    };
    EXPECT_EQ(result.symbols, expectedSymbols);
    EXPECT_EQ(result.symbolTable.symbols(), expectedSymbols);
    EXPECT_EQ(result.symbolTable.size(), 8U);
    EXPECT_FALSE(result.symbolTable.empty());
}

TEST(LinkPassOneTest, InvertingInputOrderRecomputesSectionBasesAndGlobalOffsetsDeterministically) {
    const auto first = parseValidModule("HAlpha 000000000018\n"
                                        "DSymA  000004\n"
                                        "E\n",
                                        "alpha.obj");
    const auto second = parseValidModule("HBeta  00000000002C\n"
                                         "DSymB  000010\n"
                                         "E\n",
                                         "beta.obj");
    const auto third = parseValidModule("HGamma 000000000008\n"
                                        "DSymG  000007\n"
                                        "E\n",
                                        "gamma.obj");

    const auto forward = LinkPassOne::run(std::array<ObjectModule, 3>{first, second, third});
    const auto reversed = LinkPassOne::run(std::array<ObjectModule, 3>{third, second, first});

    ASSERT_TRUE(forward.succeeded());
    ASSERT_TRUE(reversed.succeeded());
    EXPECT_EQ(forward.totalLength, 0x4CU);
    EXPECT_EQ(reversed.totalLength, 0x4CU);

    EXPECT_EQ(forward.resolveSymbol("Alpha"), 0x00U);
    EXPECT_EQ(forward.resolveSymbol("SymA"), 0x04U);
    EXPECT_EQ(forward.resolveSymbol("Beta"), 0x18U);
    EXPECT_EQ(forward.resolveSymbol("SymB"), 0x28U);
    EXPECT_EQ(forward.resolveSymbol("Gamma"), 0x44U);
    EXPECT_EQ(forward.resolveSymbol("SymG"), 0x4BU);

    EXPECT_EQ(reversed.resolveSymbol("Gamma"), 0x00U);
    EXPECT_EQ(reversed.resolveSymbol("SymG"), 0x07U);
    EXPECT_EQ(reversed.resolveSymbol("Beta"), 0x08U);
    EXPECT_EQ(reversed.resolveSymbol("SymB"), 0x18U);
    EXPECT_EQ(reversed.resolveSymbol("Alpha"), 0x34U);
    EXPECT_EQ(reversed.resolveSymbol("SymA"), 0x38U);
}

TEST(LinkPassOneTest, ComparesSectionAndSymbolNamesCaseInsensitivelyWhilePreservingSpelling) {
    const auto modA = parseValidModule("HMixSec000000000012\n"
                                       "DCamelA000006\n"
                                       "E\n",
                                       "a.obj");
    const auto modB = parseValidModule("HSub_0200000000000A\n"
                                       "Dval_b 000003\n"
                                       "E\n",
                                       "b.obj");

    const auto result = runLinkPassOne(std::array<ObjectModule, 2>{modA, modB});
    ASSERT_TRUE(result.succeeded());

    ASSERT_TRUE(result.findSection("mixsec").has_value());
    EXPECT_EQ(result.findSection("MIXSEC")->name, "MixSec");
    EXPECT_EQ(result.findSection("sub_02")->name, "Sub_02");
    EXPECT_FALSE(result.findSection("missing").has_value());

    ASSERT_TRUE(result.findSymbol("camela").has_value());
    EXPECT_EQ(result.findSymbol("CAMELA")->name, "CamelA");
    EXPECT_EQ(result.findSymbol("CAMELA")->value(), 0x06U);
    EXPECT_EQ(result.findSymbol("CAMELA")->offset(), 0x06U);

    ASSERT_TRUE(result.findSymbol("VAL_B").has_value());
    EXPECT_EQ(result.findSymbol("VAL_B")->name, "val_b");
    EXPECT_EQ(result.findSymbol("VAL_B")->sectionName, "Sub_02");
    EXPECT_EQ(result.findSymbol("VAL_B")->globalOffset, 0x15U);

    EXPECT_TRUE(result.symbolTable.contains("mixsec"));
    EXPECT_TRUE(result.symbolTable.isDefined("SUB_02"));
    EXPECT_FALSE(result.symbolTable.contains("UNKNOWN"));
    EXPECT_EQ(result.resolveSymbol("UNKNOWN"), std::nullopt);
}

TEST(LinkPassOneTest, AcceptsModulesWithZeroTextRecordsAndValidReservationsOrZeroLength) {
    const auto reservedOnly = parseValidModule("HData  000000000040\n"
                                               "DBuf0  000000BufEnd00003F\n"
                                               "E\n",
                                               "data.obj");
    const auto emptySection = parseValidModule("HEmpty 000000000000\n"
                                               "RData  \n"
                                               "E\n",
                                               "empty.obj");
    const auto codeSection = parseValidModule("HCode  000000000010\n"
                                              "T000000034F0000\n"
                                              "E000000\n",
                                              "code.obj");

    const auto result =
        LinkPassOne::run(std::array<ObjectModule, 3>{reservedOnly, emptySection, codeSection});

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(result.totalLength, 0x50U);
    EXPECT_EQ(result.resolveSymbol("Data"), 0x00U);
    EXPECT_EQ(result.resolveSymbol("Buf0"), 0x00U);
    EXPECT_EQ(result.resolveSymbol("BufEnd"), 0x3FU);
    EXPECT_EQ(result.resolveSymbol("Empty"), 0x40U);
    EXPECT_EQ(result.resolveSymbol("Code"), 0x40U);
    EXPECT_TRUE(result.hasExplicitEntry());
    EXPECT_EQ(result.entryOffset, 0x40U);
    EXPECT_EQ(result.entry.moduleIndex, 2U);
    EXPECT_EQ(result.entry.sectionName, "Code");
}

TEST(LinkPassOneTest, SelectsExplicitEntryFromLaterSectionWithoutReorderingSections) {
    const auto first = parseValidModule("HSecA  000000000020\n"
                                        "T00000003000001\n"
                                        "E\n",
                                        "a.obj");
    const auto second = parseValidModule("HSecB  000000000030\n"
                                         "T000008034F0000\n"
                                         "E000008\n",
                                         "b.obj");
    const auto third = parseValidModule("HSecC  000000000010\n"
                                        "T00000003000002\n"
                                        "E\n",
                                        "c.obj");

    const auto result = LinkPassOne::run(std::array<ObjectModule, 3>{first, second, third});
    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.sections.size(), 3U);
    EXPECT_EQ(result.sections[0].name, "SecA");
    EXPECT_EQ(result.sections[1].name, "SecB");
    EXPECT_EQ(result.sections[2].name, "SecC");

    EXPECT_TRUE(result.hasExplicitEntry());
    ASSERT_TRUE(result.explicitEntry.has_value());
    EXPECT_EQ(result.entryOffset, 0x28U);
    EXPECT_EQ(*result.explicitEntry,
              (LinkedEntryPoint{1, "SecB", 0x20U, 0x08U, 0x28U, true, {"b.obj", 3, 1}}));
    EXPECT_EQ(result.entry, *result.explicitEntry);

    EXPECT_TRUE(result.fitsInAddressSpace(0x1000U));
    EXPECT_EQ(result.resolveInitialPc(0x1000U), 0x1028U);
    EXPECT_TRUE(result.fitsInAddressSpace(ObjectAddressSpaceBytes - result.totalLength));
    EXPECT_FALSE(result.fitsInAddressSpace(ObjectAddressSpaceBytes - result.totalLength + 1U));
    EXPECT_EQ(result.resolveInitialPc(ObjectAddressSpaceBytes), std::nullopt);
}

TEST(LinkPassOneTest, RejectsMultipleExplicitEntryRecordsAcrossModules) {
    const auto first = parseValidModule("HSecA  000000000010\n"
                                        "E000002\n",
                                        "a.obj");
    const auto second = parseValidModule("HSecB  000000000010\n"
                                         "E000004\n",
                                         "b.obj");

    const auto result = LinkPassOne::run(std::array<ObjectModule, 2>{first, second});
    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::DuplicateEntry);
    EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"b.obj", 2, 1}));
    EXPECT_NE(result.diagnostics[0].message.find("SecA"), std::string::npos);
    EXPECT_NE(result.diagnostics[0].message.find("a.obj:2:1"), std::string::npos);
}

TEST(LinkPassOneTest, RejectsDuplicateSectionsDuplicateDefinitionsAndSectionSymbolCollisions) {
    {
        // Duplicate section name across modules (case-insensitive)
        const auto mod1 = parseValidModule("HProg  000000000010\nE\n", "one.obj");
        const auto mod2 = parseValidModule("HPROG  000000000010\nE\n", "two.obj");
        const auto result = LinkPassOne::run(std::array<ObjectModule, 2>{mod1, mod2});
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::DuplicateName);
        EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"two.obj", 1, 1}));
        EXPECT_NE(result.diagnostics[0].message.find("Prog"), std::string::npos);
        EXPECT_NE(result.diagnostics[0].message.find("one.obj:1:1"), std::string::npos);
    }
    {
        // Duplicate D symbol across modules (case-insensitive)
        const auto mod1 = parseValidModule("HSecA  000000000010\n"
                                           "DShared000004\n"
                                           "E\n",
                                           "a.obj");
        const auto mod2 = parseValidModule("HSecB  000000000010\n"
                                           "Dshared000008\n"
                                           "E\n",
                                           "b.obj");
        const auto result = LinkPassOne::run(std::array<ObjectModule, 2>{mod1, mod2});
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::DuplicateName);
        EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"b.obj", 2, 2}));
        EXPECT_NE(result.diagnostics[0].message.find("Shared"), std::string::npos);
        EXPECT_NE(result.diagnostics[0].message.find("a.obj:2:2"), std::string::npos);
        EXPECT_EQ(result.resolveSymbol("Shared"), 0x04U);
    }
    {
        // Section name in module 1 colliding with D symbol in module 2
        const auto mod1 = parseValidModule("HUtil  000000000010\nE\n", "util.obj");
        const auto mod2 = parseValidModule("HMain  000000000010\n"
                                           "Dutil  000002\n"
                                           "E\n",
                                           "main.obj");
        const auto result = LinkPassOne::run(std::array<ObjectModule, 2>{mod1, mod2});
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::DuplicateName);
        EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"main.obj", 2, 2}));
    }
    {
        // D symbol in module 1 colliding with section name in module 2
        const auto mod1 = parseValidModule("HMain  000000000010\n"
                                           "Dutil  000002\n"
                                           "E\n",
                                           "main.obj");
        const auto mod2 = parseValidModule("HUtil  000000000010\nE\n", "util.obj");
        const auto result = LinkPassOne::run(std::array<ObjectModule, 2>{mod1, mod2});
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::DuplicateName);
        EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"util.obj", 1, 1}));
    }
    {
        // Section name equal to D in the same module
        ObjectModule sameModule;
        sameModule.header = {"MAIN", 0x10, {"same.obj", 1, 1}};
        sameModule.definitions = {{"main", 0x02, {"same.obj", 2, 2}}};
        sameModule.entry = {std::nullopt, {"same.obj", 3, 1}};

        const auto result = LinkPassOne::run(std::span<const ObjectModule>{&sameModule, 1});
        EXPECT_FALSE(result.succeeded());
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::DuplicateName);
        EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"same.obj", 2, 2}));
    }
}

TEST(LinkPassOneTest, RejectsDefinitionAndEntryOffsetsOutsideSectionBounds) {
    ObjectModule mod;
    mod.header = {"MAIN", 0x10, {"bounds.obj", 1, 1}};
    mod.definitions = {{"ATEND", 0x10, {"bounds.obj", 2, 2}},
                       {"PAST", 0x20, {"bounds.obj", 2, 14}}};
    mod.entry = {0x10, {"bounds.obj", 3, 1}};

    const auto result = LinkPassOne::run(std::span<const ObjectModule>{&mod, 1});
    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(result.diagnostics.size(), 3U);
    EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"bounds.obj", 2, 8}));
    EXPECT_EQ(result.diagnostics[1].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(result.diagnostics[1].position, (ObjectPosition{"bounds.obj", 2, 20}));
    EXPECT_EQ(result.diagnostics[2].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(result.diagnostics[2].position, (ObjectPosition{"bounds.obj", 3, 2}));
    EXPECT_FALSE(result.hasExplicitEntry());
}

TEST(LinkPassOneTest, AcceptsCombinedLengthAtOneMegabyteBoundaryAndRejectsOverflow) {
    const auto sec1 = parseValidModule("HSec1  000000040000\nE\n", "s1.obj");
    const auto sec2 = parseValidModule("HSec2  000000080000\nE\n", "s2.obj");
    const auto sec3Exact =
        parseValidModule("HSec3  000000040000\nDLast  03FFFF\nE03FFFF\n", "s3.obj");

    const auto atLimit = LinkPassOne::run(std::array<ObjectModule, 3>{sec1, sec2, sec3Exact});
    ASSERT_TRUE(atLimit.succeeded());
    EXPECT_EQ(atLimit.totalLength, ObjectAddressSpaceBytes);
    EXPECT_EQ(atLimit.resolveSymbol("Last"), 0xFFFFFU);
    EXPECT_EQ(atLimit.entryOffset, 0xFFFFFU);
    EXPECT_TRUE(atLimit.fitsInAddressSpace(0U));
    EXPECT_EQ(atLimit.resolveInitialPc(0U), 0xFFFFFU);
    EXPECT_FALSE(atLimit.fitsInAddressSpace(1U));

    const auto sec3Overflow = parseValidModule("HSec3  000000040001\nE\n", "s3_over.obj");
    const auto overLimit = LinkPassOne::run(std::array<ObjectModule, 3>{sec1, sec2, sec3Overflow});
    EXPECT_FALSE(overLimit.succeeded());
    EXPECT_FALSE(overLimit.fitsInAddressSpace(0U));
    EXPECT_FALSE(overLimit.resolveInitialPc(0U));
    ASSERT_EQ(overLimit.diagnostics.size(), 1U);
    EXPECT_EQ(overLimit.diagnostics[0].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(overLimit.diagnostics[0].position, (ObjectPosition{"s3_over.obj", 1, 14}));

    ObjectModule hugeSingle;
    hugeSingle.header = {"HUGE", 0xFFFFFFFFU, {"huge.obj", 1, 1}};
    hugeSingle.entry = {std::nullopt, {"huge.obj", 2, 1}};
    const auto hugeResult = LinkPassOne::run(std::span<const ObjectModule>{&hugeSingle, 1});
    EXPECT_FALSE(hugeResult.succeeded());
    ASSERT_EQ(hugeResult.diagnostics.size(), 1U);
    EXPECT_EQ(hugeResult.diagnostics[0].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(hugeResult.diagnostics[0].position, (ObjectPosition{"huge.obj", 1, 14}));
}

TEST(LinkPassOneTest,
     ValidatesReferencesAndModificationsStructurallyWhileLeavingUndefinedExternalsForPassTwo) {
    // UNDEF is declared in R and used in M, but not defined in any module.
    // Pass 1 must succeed so Pass 2 handles undefined external symbol resolution.
    const auto validWithUnresolvedExternal = parseValidModule("HMain  000000000006\n"
                                                              "RUndef \n"
                                                              "T000000064B1000000000\n"
                                                              "M00000105+Undef \n"
                                                              "M00000306+Main  \n"
                                                              "E000000\n",
                                                              "valid.obj");
    const auto passOneOk =
        LinkPassOne::run(std::span<const ObjectModule>{&validWithUnresolvedExternal, 1});
    EXPECT_TRUE(passOneOk.succeeded());

    // Even if OTHER is defined in another module, using OTHER in M without declaring it in R
    // of the current module is a structural UndeclaredReference error in Pass 1.
    ObjectModule missingR;
    missingR.header = {"MAIN", 3, {"m1.obj", 1, 1}};
    missingR.text = {{0, {0x00, 0x00, 0x00}, {"m1.obj", 2, 1}}};
    missingR.modifications = {{0, 6, ModificationSign::Add, "OTHER", {"m1.obj", 3, 1}}};
    missingR.entry = {std::nullopt, {"m1.obj", 4, 1}};

    const auto otherMod = parseValidModule("HOTHER 000000000003\nE\n", "m2.obj");
    const auto undeclared = LinkPassOne::run(std::array<ObjectModule, 2>{missingR, otherMod});
    EXPECT_FALSE(undeclared.succeeded());
    ASSERT_EQ(undeclared.diagnostics.size(), 1U);
    EXPECT_EQ(undeclared.diagnostics[0].code, ObjectDiagnosticCode::UndeclaredReference);
    EXPECT_EQ(undeclared.diagnostics[0].position, (ObjectPosition{"m1.obj", 3, 11}));
}

TEST(LinkPassOneTest, GathersMultipleStructuralDiagnosticsAcrossModulesInSingleRun) {
    ObjectModule bad1;
    bad1.header = {"1BAD", 4, {"bad1.obj", 1, 1}};
    bad1.definitions = {{"VAL", 2, {"bad1.obj", 2, 2}}};
    bad1.references = {{"val", {"bad1.obj", 3, 2}}};
    bad1.text = {{0, {0x01, 0x02}, {"bad1.obj", 4, 1}}};
    bad1.modifications = {{0, 6, ModificationSign::Add, "VAL", {"bad1.obj", 5, 1}}};
    bad1.entry = {std::nullopt, {"bad1.obj", 6, 1}};

    ObjectModule bad2;
    bad2.header = {"SEC2", 3, {"bad2.obj", 1, 1}};
    bad2.definitions = {{"VAL", 0, {"bad2.obj", 2, 2}}};
    bad2.entry = {5, {"bad2.obj", 3, 1}};

    const auto result = LinkPassOne::run(std::array<ObjectModule, 2>{bad1, bad2});
    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(result.diagnostics.size(), 5U);
    EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::InvalidName);
    EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"bad1.obj", 1, 2}));
    EXPECT_EQ(result.diagnostics[1].code, ObjectDiagnosticCode::DuplicateName);
    EXPECT_EQ(result.diagnostics[1].position, (ObjectPosition{"bad1.obj", 3, 2}));
    EXPECT_EQ(result.diagnostics[2].code, ObjectDiagnosticCode::MissingText);
    EXPECT_EQ(result.diagnostics[2].position, (ObjectPosition{"bad1.obj", 5, 2}));
    EXPECT_EQ(result.diagnostics[3].code, ObjectDiagnosticCode::DuplicateName);
    EXPECT_EQ(result.diagnostics[3].position, (ObjectPosition{"bad2.obj", 2, 2}));
    EXPECT_EQ(result.diagnostics[4].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(result.diagnostics[4].position, (ObjectPosition{"bad2.obj", 3, 2}));
    EXPECT_EQ(result.failure().diagnostics, result.diagnostics);
}

TEST(LinkPassOneTest, RejectsEmptyInputAndDoesNotPreserveGlobalSymbolTableStateBetweenRuns) {
    const auto emptyResult = LinkPassOne::run(std::span<const ObjectModule>{});
    EXPECT_FALSE(emptyResult.succeeded());
    EXPECT_FALSE(emptyResult.fitsInAddressSpace(0U));
    EXPECT_FALSE(emptyResult.resolveInitialPc(0U));
    ASSERT_EQ(emptyResult.diagnostics.size(), 1U);
    EXPECT_EQ(emptyResult.diagnostics[0].code, ObjectDiagnosticCode::MissingRecord);

    LinkPassOneResult firstRun;
    {
        std::string source = "HTemp  000000000010\nDExport000004\nE000002\n";
        const auto mod = parseValidModule(source, "temp.obj");
        firstRun = LinkPassOne::run(std::span<const ObjectModule>{&mod, 1});
        source.assign(source.size(), 'X');
    }
    ASSERT_TRUE(firstRun.succeeded());
    EXPECT_EQ(firstRun.resolveSymbol("Temp"), 0x00U);
    EXPECT_EQ(firstRun.resolveSymbol("Export"), 0x04U);
    EXPECT_EQ(firstRun.entryOffset, 0x02U);

    // A second independent run with the same section and symbol names must not collide with
    // firstRun
    const auto modAgain =
        parseValidModule("HTemp  000000000020\nDExport000008\nE000006\n", "again.obj");
    const auto secondRun = LinkPassOne::run(std::span<const ObjectModule>{&modAgain, 1});
    ASSERT_TRUE(secondRun.succeeded());
    EXPECT_EQ(secondRun.totalLength, 0x20U);
    EXPECT_EQ(secondRun.resolveSymbol("Temp"), 0x00U);
    EXPECT_EQ(secondRun.resolveSymbol("Export"), 0x08U);
    EXPECT_EQ(secondRun.entryOffset, 0x06U);
}

TEST(LinkPassOneTest, GlobalSymbolTableValidatesNamesAndPreservesFirstDefinitionOnCollision) {
    EXPECT_TRUE(GlobalSymbolTable::isValidName("A"));
    EXPECT_TRUE(GlobalSymbolTable::isValidName("SEC_01"));
    EXPECT_FALSE(GlobalSymbolTable::isValidName(""));
    EXPECT_FALSE(GlobalSymbolTable::isValidName("1ABC"));
    EXPECT_FALSE(GlobalSymbolTable::isValidName("_ABC"));
    EXPECT_FALSE(GlobalSymbolTable::isValidName("TOOLONG"));
    EXPECT_FALSE(GlobalSymbolTable::isValidName("AB$C"));

    GlobalSymbolTable table;
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.size(), 0U);

    const auto badSection = table.defineSection("1BAD", 0, 0, {"a.obj", 1, 2});
    ASSERT_TRUE(badSection.has_value());
    EXPECT_EQ(badSection->code, ObjectDiagnosticCode::InvalidName);
    EXPECT_EQ(badSection->position, (ObjectPosition{"a.obj", 1, 2}));

    const auto badSymbol = table.defineSymbol("TOO_LONG", 0, "MAIN", 0, 0, {"a.obj", 2, 2});
    ASSERT_TRUE(badSymbol.has_value());
    EXPECT_EQ(badSymbol->code, ObjectDiagnosticCode::InvalidName);
    EXPECT_EQ(badSymbol->position, (ObjectPosition{"a.obj", 2, 8}));

    const auto overflow = table.defineSymbol("WRAP", 0, "MAIN", 0xFFFFFFFFU, 1, {"a.obj", 2, 2});
    ASSERT_TRUE(overflow);
    EXPECT_EQ(overflow->code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_FALSE(table.contains("WRAP"));

    EXPECT_FALSE(table.defineSection("Main", 0, 0x100U, {"a.obj", 1, 1}).has_value());
    EXPECT_FALSE(
        table.defineSymbol("Val_1", 0, "Main", 0x100U, 0x20U, {"a.obj", 2, 2}).has_value());

    const auto dupSection = table.defineSection("main", 1, 0x200U, {"b.obj", 1, 1});
    ASSERT_TRUE(dupSection.has_value());
    EXPECT_EQ(dupSection->code, ObjectDiagnosticCode::DuplicateName);
    EXPECT_EQ(dupSection->position, (ObjectPosition{"b.obj", 1, 1}));

    const auto dupSymbol = table.defineSymbol("VAL_1", 1, "Sub", 0x200U, 0x04U, {"b.obj", 2, 2});
    ASSERT_TRUE(dupSymbol.has_value());
    EXPECT_EQ(dupSymbol->code, ObjectDiagnosticCode::DuplicateName);
    EXPECT_EQ(dupSymbol->position, (ObjectPosition{"b.obj", 2, 2}));

    EXPECT_EQ(table.size(), 2U);
    EXPECT_EQ(table.resolve("MAIN"), 0x100U);
    EXPECT_EQ(table.resolve("val_1"), 0x120U);
}

TEST(LinkPassOneTest, DistinguishesExplicitZeroEntryInLaterSectionFromImplicitDefaultEntry) {
    const auto first = parseValidModule("HFirst 000000000010\nE\n", "first.obj");
    const auto second = parseValidModule("HSecond000000000020\nE\n", "second.obj");
    const auto thirdWithZeroEntry = parseValidModule("HThird 000000000015\nE000000\n", "third.obj");

    const auto result =
        LinkPassOne::run(std::array<ObjectModule, 3>{first, second, thirdWithZeroEntry});
    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.hasExplicitEntry());
    ASSERT_TRUE(result.explicitEntry.has_value());
    EXPECT_EQ(result.entryOffset, 0x30U);
    EXPECT_EQ(result.explicitEntry->moduleIndex, 2U);
    EXPECT_EQ(result.explicitEntry->sectionName, "Third");
    EXPECT_EQ(result.explicitEntry->sectionBaseOffset, 0x30U);
    EXPECT_EQ(result.explicitEntry->relativeOffset, 0x00U);
    EXPECT_EQ(result.explicitEntry->globalOffset, 0x30U);
}

TEST(LinkPassOneTest, ValidatesTextAndModificationInvariantsForInMemoryModules) {
    ObjectModule mod;
    mod.header = {"MAIN", 0x10, {"mem.obj", 1, 1}};
    mod.references = {{"BAD!", {"mem.obj", 2, 2}}, {"EXT", {"mem.obj", 2, 8}}};
    mod.text = {
        {0, {}, {"mem.obj", 3, 1}},
        {0, std::vector<std::uint8_t>(31, 0), {"mem.obj", 4, 1}},
        {0x0F, {0x01, 0x02}, {"mem.obj", 5, 1}},
        {0, {0x01, 0x02, 0x03}, {"mem.obj", 6, 1}},
        {2, {0x04, 0x05}, {"mem.obj", 7, 1}},
    };
    mod.modifications = {
        {0, 4, ModificationSign::Add, "MAIN", {"mem.obj", 8, 1}},
        {0, 5, static_cast<ModificationSign>(99), "MAIN", {"mem.obj", 9, 1}},
        {0x0E, 5, ModificationSign::Add, "MAIN", {"mem.obj", 10, 1}},
        {0, 5, ModificationSign::Add, "1BAD", {"mem.obj", 11, 1}},
    };
    mod.entry = {std::nullopt, {"mem.obj", 12, 1}};

    const auto result = LinkPassOne::run(std::span<const ObjectModule>{&mod, 1});
    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(result.diagnostics.size(), 9U);
    EXPECT_EQ(result.diagnostics[0].code, ObjectDiagnosticCode::InvalidName);
    EXPECT_EQ(result.diagnostics[0].position, (ObjectPosition{"mem.obj", 2, 5}));
    EXPECT_EQ(result.diagnostics[1].code, ObjectDiagnosticCode::InvalidTextLength);
    EXPECT_EQ(result.diagnostics[1].position, (ObjectPosition{"mem.obj", 3, 8}));
    EXPECT_EQ(result.diagnostics[2].code, ObjectDiagnosticCode::InvalidTextLength);
    EXPECT_EQ(result.diagnostics[2].position, (ObjectPosition{"mem.obj", 4, 8}));
    EXPECT_EQ(result.diagnostics[3].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(result.diagnostics[3].position, (ObjectPosition{"mem.obj", 5, 2}));
    EXPECT_EQ(result.diagnostics[4].code, ObjectDiagnosticCode::OverlappingText);
    EXPECT_EQ(result.diagnostics[4].position, (ObjectPosition{"mem.obj", 7, 2}));
    EXPECT_EQ(result.diagnostics[5].code, ObjectDiagnosticCode::InvalidModification);
    EXPECT_EQ(result.diagnostics[5].position, (ObjectPosition{"mem.obj", 8, 8}));
    EXPECT_EQ(result.diagnostics[6].code, ObjectDiagnosticCode::InvalidModification);
    EXPECT_EQ(result.diagnostics[6].position, (ObjectPosition{"mem.obj", 9, 10}));
    EXPECT_EQ(result.diagnostics[7].code, ObjectDiagnosticCode::ValueOutOfRange);
    EXPECT_EQ(result.diagnostics[7].position, (ObjectPosition{"mem.obj", 10, 2}));
    EXPECT_EQ(result.diagnostics[8].code, ObjectDiagnosticCode::InvalidName);
    EXPECT_EQ(result.diagnostics[8].position, (ObjectPosition{"mem.obj", 11, 11}));
}

} // namespace
} // namespace linker
